#include "aujson.hpp"
#include "detail/number.hpp"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace aujson {

namespace {

std::string indentStr(int level, const WriteOptions& wo) {
    return std::string(static_cast<size_t>(level) *
                       static_cast<size_t>(wo.indent > 0 ? wo.indent : 0), ' ');
}

// 整数形态 double：直接整数输出 + ".0"（免 snprintf）
std::string formatIntegralDouble(double d) {
    long long ll = static_cast<long long>(d);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld", ll);
    std::string s = buf;
    s += ".0";
    return s;
}

// 短十进制小数（d × 10^k 为整数，k ≤ 9）：手写数字输出 + 快速路径回读验证，
// 验证失败退回 snprintf 通用路径。
std::string formatShortDecimal(double d) {
    for (int k = 1; k <= 9; ++k) {
        double scaled = d * detail::pow10(k);
        double r = std::llround(scaled);
        if (r != scaled || r <= -9223372036854775808.0 || r >= 9223372036854775808.0)
            continue;
        long long v = static_cast<long long>(r);
        bool neg = v < 0;
        unsigned long long m = neg ? static_cast<unsigned long long>(-(v + 1)) + 1ULL
                                   : static_cast<unsigned long long>(v);
        char digits[24];
        int nd = 0;
        do { digits[nd++] = static_cast<char>('0' + (m % 10)); m /= 10; } while (m);

        std::string s;
        if (neg) s += '-';
        if (nd <= k) {
            s += "0.";
            for (int z = 0; z < k - nd; ++z) s += '0';
        }
        for (int idx = nd - 1; idx >= 0; --idx) {
            s += digits[idx];
            if (idx == k) s += '.';
        }
        double back = 0;
        if (detail::fastDouble(s.c_str(), s.size(), back) && back == d) return s;
    }
    return std::string();
}

// 浮点输出：短十进制快速路径优先；通用路径 %.15g → %.17g（17 位必回读相等）
std::string formatDouble(double d) {
    if (!std::isfinite(d)) return "null";   // NaN / Inf → null，不产生非法 JSON
    if (d == std::floor(d) && d >= -9223372036854775808.0 && d < 9223372036854775808.0)
        return formatIntegralDouble(d);
    std::string s = formatShortDecimal(d);
    if (!s.empty()) return s;

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.15g", d);
    if (std::strtod(buf, nullptr) != d)
        std::snprintf(buf, sizeof(buf), "%.17g", d);
    s = buf;
    // 整数形态补 ".0"，保证 round-trip 后仍是 Double
    if (s.find('.') == std::string::npos &&
        s.find('e') == std::string::npos &&
        s.find('E') == std::string::npos)
        s += ".0";
    return s;
}

// codepoint → \uXXXX（或代理对）
void emitCodePoint(std::string& out, uint32_t cp) {
    char buf[8];
    if (cp <= 0xFFFF) {
        std::snprintf(buf, sizeof(buf), "\\u%04x", cp);
        out += buf;
    } else {
        cp -= 0x10000;
        std::snprintf(buf, sizeof(buf), "\\u%04x", 0xD800 + (cp >> 10));
        out += buf;
        std::snprintf(buf, sizeof(buf), "\\u%04x", 0xDC00 + (cp & 0x3FF));
        out += buf;
    }
}

void writeString(std::string& out, const std::string& s, const WriteOptions& wo) {
    out += '"';
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        switch (c) {
        case '"':  out += "\\\""; ++i; break;
        case '\\': out += "\\\\"; ++i; break;
        case '\b': out += "\\b"; ++i; break;
        case '\f': out += "\\f"; ++i; break;
        case '\n': out += "\\n"; ++i; break;
        case '\r': out += "\\r"; ++i; break;
        case '\t': out += "\\t"; ++i; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
                ++i;
            } else if (c == '/' && wo.escapeSlash) {
                out += "\\/";
                ++i;
            } else if (wo.ensureAscii && c >= 0x80) {
                // 解码 UTF-8 序列 → \uXXXX（或代理对）；非法序列退化为逐字节 \u00XX
                uint32_t cp = 0;
                int seq = 0;
                if ((c >> 5) == 0x6)      { cp = c & 0x1F; seq = 1; }
                else if ((c >> 4) == 0xE) { cp = c & 0x0F; seq = 2; }
                else if ((c >> 3) == 0xF) { cp = c & 0x07; seq = 3; }
                bool ok = seq > 0 && i + seq < s.size();
                if (ok) {
                    for (int k = 1; k <= seq; ++k) {
                        unsigned char b = static_cast<unsigned char>(s[i + k]);
                        if ((b >> 6) != 0x2) { ok = false; break; }
                        cp = (cp << 6) | (b & 0x3F);
                    }
                }
                uint32_t minCp = seq == 1 ? 0x80 : (seq == 2 ? 0x800 : 0x10000);
                if (ok && cp >= minCp && cp <= 0x10FFFF &&
                    !(cp >= 0xD800 && cp <= 0xDFFF)) {
                    emitCodePoint(out, cp);
                    i += static_cast<size_t>(seq) + 1;
                } else {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                    ++i;
                }
            } else {
                out += s[i];  // UTF-8 原样直出
                ++i;
            }
        }
    }
    out += '"';
}

// 缩进缓存：每级只构造一次（pretty 时避免逐元素构造字符串）
const std::string& indentFor(int level, const WriteOptions& wo, std::vector<std::string>& cache) {
    if (static_cast<int>(cache.size()) <= level) {
        size_t old = cache.size();
        cache.resize(static_cast<size_t>(level) + 1);
        for (size_t k = old; k < cache.size(); ++k)
            cache[k] = std::string(k * static_cast<size_t>(wo.indent > 0 ? wo.indent : 0), ' ');
    }
    return cache[static_cast<size_t>(level)];
}

void writeValue(std::string& out, const Value& v, const WriteOptions& wo, int level,
                std::vector<std::string>& indentCache);

void writeArray(std::string& out, const Value& v, const WriteOptions& wo, int level,
                std::vector<std::string>& indentCache) {
    out += '[';
    size_t n = v.size();
    for (size_t i = 0; i < n; ++i) {
        if (i) out += ',';
        if (wo.pretty) out += '\n' + indentFor(level + 1, wo, indentCache);
        writeValue(out, v[i], wo, level + 1, indentCache);
    }
    if (wo.pretty && n) out += '\n' + indentFor(level, wo, indentCache);
    out += ']';
}

void writeObject(std::string& out, const Value& v, const WriteOptions& wo, int level,
                 std::vector<std::string>& indentCache) {
    out += '{';
    size_t n = v.size();
    // 收集 (键, 值) 视图；sortKeys 时仅排序输出，不改存储顺序
    std::vector<std::pair<const std::string*, const Value*>> kv;
    kv.reserve(n);
    for (auto it = v.begin(); it != v.end(); ++it) {
        Value::ConstItem item = *it;
        kv.push_back(std::make_pair(item.key, item.value));
    }
    if (wo.sortKeys) {
        std::sort(kv.begin(), kv.end(),
                  [](const std::pair<const std::string*, const Value*>& a,
                     const std::pair<const std::string*, const Value*>& b) {
                      return *a.first < *b.first;
                  });
    }
    for (size_t i = 0; i < kv.size(); ++i) {
        if (i) out += ',';
        if (wo.pretty) out += '\n' + indentFor(level + 1, wo, indentCache);
        writeString(out, *kv[i].first, wo);
        out += wo.pretty ? ": " : ":";
        writeValue(out, *kv[i].second, wo, level + 1, indentCache);
    }
    if (wo.pretty && n) out += '\n' + indentFor(level, wo, indentCache);
    out += '}';
}

void writeValue(std::string& out, const Value& v, const WriteOptions& wo, int level,
                std::vector<std::string>& indentCache) {
    switch (v.type()) {
    case Type::Null:   out += "null"; return;
    case Type::Bool:   out += v.asBool() ? "true" : "false"; return;
    case Type::Int: {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%" PRId64, static_cast<int64_t>(v.asInt()));
        out += buf;
        return;
    }
    case Type::UInt: {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%" PRIu64, static_cast<uint64_t>(v.asUInt()));
        out += buf;
        return;
    }
    case Type::Double: out += formatDouble(v.asDouble()); return;
    case Type::String: writeString(out, v.asString(), wo); return;
    case Type::Array:  writeArray(out, v, wo, level, indentCache); return;
    case Type::Object: writeObject(out, v, wo, level, indentCache); return;
    }
}

size_t estimateSize(const Value& v) {
    switch (v.type()) {
    case Type::Null:   return 4;
    case Type::Bool:   return 5;
    case Type::Int:    return 21;
    case Type::UInt:   return 21;
    case Type::Double: return 24;
    case Type::String: return v.asString().size() + 2;
    case Type::Array: {
        size_t s = 2;
        for (auto el : v) s += estimateSize(*el.value) + 1;
        return s;
    }
    case Type::Object: {
        size_t s = 2;
        for (auto item : v)
            s += estimateSize(*item.value) + (item.key ? item.key->size() : 0) + 4;
        return s;
    }
    }
    return 16;
}

} // namespace

std::string stringify(const Value& v, const WriteOptions& opt) {
    std::string out;
    out.reserve(estimateSize(v) + 8);
    std::vector<std::string> indentCache;
    writeValue(out, v, opt, 0, indentCache);
    if (opt.newlineAtEnd) out += '\n';
    return out;
}

} // namespace aujson
