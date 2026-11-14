#include "aujson.hpp"
#include "detail/number.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

namespace aujson {

SaxHandler::~SaxHandler() {}

namespace {

inline bool isWs(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
inline bool isDigit(char c) { return c >= '0' && c <= '9'; }

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// UTF-8 合法性校验（RFC 3629：拒绝过长编码、代理区、超 U+10FFFF）
bool utf8Valid(const char* p, size_t len) {
    size_t i = 0;
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(p[i]);
        int extra = 0;
        uint32_t cp = 0;
        if (c < 0x80) { ++i; continue; }
        if ((c >> 5) == 0x6)      { extra = 1; cp = c & 0x1F; }
        else if ((c >> 4) == 0xE) { extra = 2; cp = c & 0x0F; }
        else if ((c >> 4) == 0xF) { extra = 3; cp = c & 0x07; }
        else return false;
        if (i + 1 + extra > len) return false;
        for (int k = 1; k <= extra; ++k) {
            unsigned char b = static_cast<unsigned char>(p[i + k]);
            if ((b >> 6) != 0x2) return false;
            cp = (cp << 6) | (b & 0x3F);
        }
        if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) ||
            (extra == 3 && cp < 0x10000) || cp > 0x10FFFF ||
            (cp >= 0xD800 && cp <= 0xDFFF))
            return false;
        i += 1 + extra;
    }
    return true;
}

void utf8Encode(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out += static_cast<char>(cp);
    } else if (cp <= 0x7FF) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// SWAR 词扫描 [p, p+len)：检测控制字符（<0x20）与高位字节（≥0x80）。
// 返回 true 表示存在控制字符（ctrlOff 记其下标）；nonAscii 出参记录高位字节是否出现。
// 精确性：字节 <0x20 ⇔ (b & 0xE0)==0；"词内存在 0 字节"用 haszero 技巧判定，无假阳性。
bool spanScan(const char* p, size_t len, bool& nonAscii, size_t& ctrlOff) {
    size_t i = 0;
    for (; i + 8 <= len; i += 8) {
        uint64_t w;
        std::memcpy(&w, p + i, 8);
        if (w & 0x8080808080808080ULL) nonAscii = true;
        const uint64_t v = w & 0xE0E0E0E0E0E0E0E0ULL;
        if ((v - 0x0101010101010101ULL) & ~v & 0x8080808080808080ULL) {
            for (int k = 0; k < 8; ++k) {          // 词内定位精确字节（罕见：非法输入路径）
                if (static_cast<unsigned char>(p[i + k]) < 0x20) {
                    ctrlOff = i + static_cast<size_t>(k);
                    return true;
                }
            }
        }
    }
    for (; i < len; ++i) {
        unsigned char c = static_cast<unsigned char>(p[i]);
        if (c < 0x20) { ctrlOff = i; return true; }
        if (c >= 0x80) nonAscii = true;
    }
    return false;
}

// 字符串扫描标志表：普通字符一次查表即判（代替逐字节多次比较）
enum {
    SF_CTRL = 1,      // 控制字符（< 0x20，非法）
    SF_SPECIAL = 2,   // 引号 / 反斜杠（需单独处理）
    SF_NONASCII = 4   // 非 ASCII（≥ 0x80，触发 UTF-8 校验）
};
const unsigned char* strFlags() {
    struct Init {
        unsigned char t[256];
        Init() {
            for (int i = 0; i < 256; ++i) {
                unsigned f = 0;
                if (i < 0x20) f |= SF_CTRL;
                if (i == '"' || i == '\'' || i == '\\') f |= SF_SPECIAL;
                if (i >= 0x80) f |= SF_NONASCII;
                t[i] = static_cast<unsigned char>(f);
            }
        }
    };
    static const Init s;
    return s.t;
}

} // namespace

// ---------------------------------------------------------------- DOM 构建器
// 扫描器模板化直调（非虚、可内联）；DOM 由该构建器组装。stack_ 保存容器指针：
// 容器的兄弟节点只可能在其全部子节点关闭之后追加，故栈内指针不会失效。
class DomBuilder {
public:
    void onNull() { leaf(Value()); }
    void onBool(bool b) { leaf(Value(b)); }
    void onInt(int64_t i) { leaf(Value(i)); }
    void onUInt(uint64_t u) { leaf(Value(u)); }
    void onDouble(double d) { leaf(Value(d)); }
    void onString(const char* data, size_t len) { leaf(Value(data, len)); }
    void onKey(const char* data, size_t len) { pendingKey_.assign(data, len); }
    void onStartArray() { open(Value::makeArray()); }
    void onStartObject() { open(Value::makeObject()); }
    void onEndArray() { stack_.pop_back(); }
    void onEndObject() { stack_.pop_back(); }

    Value take() { return std::move(root_); }

private:
    void leaf(Value v) {
        if (stack_.empty()) {
            root_ = std::move(v);
            return;
        }
        Value& parent = *stack_.back();
        if (parent.isArray()) parent.push_back(std::move(v));
        else parent.insertKey(std::move(pendingKey_), std::move(v));  // 键零拷贝
    }

    void open(Value c) {
        if (stack_.empty()) {
            root_ = std::move(c);
            stack_.push_back(&root_);
            return;
        }
        Value& parent = *stack_.back();
        Value& slot = parent.isArray()
            ? parent.push_back(std::move(c))
            : parent.insertKey(std::move(pendingKey_), std::move(c));
        // 解析期预留：对象常见 5-8 键取 8；数组（如 tags）常为 2-4 元素取 4，避免过度分配
        slot.reserve(slot.isArray() ? 4 : 8);
        stack_.push_back(&slot);
    }

    std::vector<Value*> stack_;
    std::string pendingKey_;
    Value root_;
};

// ---------------------------------------------------------------- 扫描器

// 扫描器：handler 模板化（SAX 走基类虚回调；DOM 直调 DomBuilder，可内联去虚化）
template <class Handler>
class Parser {
public:
    Parser(const char* data, size_t len, Handler& handler, const ParseOptions& opt)
        : data_(data), end_(data + len), handler_(handler), opt_(opt) {}

    ParseResult run() {
        p_ = data_;
        // 跳过 UTF-8 BOM
        if (static_cast<size_t>(end_ - p_) >= 3 &&
            static_cast<unsigned char>(p_[0]) == 0xEF &&
            static_cast<unsigned char>(p_[1]) == 0xBB &&
            static_cast<unsigned char>(p_[2]) == 0xBF)
            p_ += 3;
        if (!skipWs()) return result_;
        if (p_ >= end_) { fail(ParseErrorCode::ExpectedValue, "输入为空"); return result_; }
        if (!parseValue(0)) return result_;
        if (!skipWs()) return result_;
        if (p_ < end_) { fail(ParseErrorCode::TrailingCharacters, "值之后有多余字符"); return result_; }
        result_.ok = true;
        return result_;
    }

private:
    bool fail(ParseErrorCode code, const char* msg) {
        size_t offset = static_cast<size_t>(p_ - data_);
        size_t line = 1, column = 1;
        for (const char* q = data_; q < p_; ++q) {
            if (*q == '\n') { ++line; column = 1; } else { ++column; }
        }
        result_.ok = false;
        result_.error.code = code;
        result_.error.message = msg;
        result_.error.offset = offset;
        result_.error.line = line;
        result_.error.column = column;
        return false;
    }

    bool skipWs() {
        for (;;) {
            while (p_ < end_ && isWs(*p_)) ++p_;
            if (!opt_.allowComments || p_ >= end_) return true;
            if (p_ + 1 < end_ && *p_ == '/' && p_[1] == '/') {
                p_ += 2;
                while (p_ < end_ && *p_ != '\n') ++p_;
                continue;
            }
            if (p_ + 1 < end_ && *p_ == '/' && p_[1] == '*') {
                p_ += 2;
                for (;;) {
                    if (p_ >= end_) {
                        fail(ParseErrorCode::UnclosedComment, "块注释未闭合");
                        return false;
                    }
                    if (*p_ == '*' && p_ + 1 < end_ && p_[1] == '/') { p_ += 2; break; }
                    ++p_;
                }
                continue;
            }
            return true;
        }
    }

    bool atDelimiter() const {
        if (p_ >= end_) return true;
        char c = *p_;
        return isWs(c) || c == ',' || c == ']' || c == '}';
    }

    bool parseValue(int depth) {
        if (depth > opt_.maxDepth)
            return fail(ParseErrorCode::DepthExceeded, "嵌套深度超过 maxDepth");
        if (!skipWs()) return false;
        if (p_ >= end_) return fail(ParseErrorCode::ExpectedValue, "期望一个值");

        char c = *p_;
        if (c == '{') return parseObject(depth);
        if (c == '[') return parseArray(depth);
        if (c == '"' || (c == '\'' && opt_.allowSingleQuotes)) return parseStringValue();
        if (c == 't') return parseBool(true);
        if (c == 'f') return parseBool(false);
        if (c == 'n') return parseNull();
        if (opt_.allowNaNInf && (c == 'N' || c == 'I')) return parseSpecialDouble();
        if (c == '-' || isDigit(c)) return parseNumber();
        return fail(ParseErrorCode::ExpectedValue, "期望一个值");
    }

    // 字面量匹配 + 后随分隔符检查（p_ 指向字面量首字符）
    bool matchKeyword(const char* lit) {
        size_t n = std::strlen(lit);
        if (static_cast<size_t>(end_ - p_) < n || std::memcmp(p_, lit, n) != 0) return false;
        const char* after = p_ + n;
        if (after < end_ && !isWs(*after) && *after != ',' && *after != ']' && *after != '}')
            return false;
        p_ = after;
        return true;
    }

    bool parseBool(bool expectTrue) {
        if (!matchKeyword(expectTrue ? "true" : "false"))
            return fail(ParseErrorCode::InvalidLiteral,
                        expectTrue ? "true 写法不合法" : "false 写法不合法");
        handler_.onBool(expectTrue);
        return true;
    }

    bool parseNull() {
        if (!matchKeyword("null"))
            return fail(ParseErrorCode::InvalidLiteral, "null 写法不合法");
        handler_.onNull();
        return true;
    }

    bool parseSpecialDouble() {
        if (matchKeyword("NaN")) {
            handler_.onDouble(NAN);
            return true;
        }
        if (matchKeyword("Infinity")) {
            handler_.onDouble(INFINITY);
            return true;
        }
        if (*p_ == '-') {
            const char* save = p_;
            ++p_;
            if (matchKeyword("Infinity")) {
                handler_.onDouble(-INFINITY);
                return true;
            }
            p_ = save;
        }
        return fail(ParseErrorCode::ExpectedValue, "期望一个值");
    }

    // 字符串：未转义时 outData/outLen 直接指向输入缓冲（零拷贝），否则写入 scratch。
    // 两段式：前 32 字节逐字节查表扫描（短串多数在此闭合，无 memchr 调用开销）；
    // 更长则 memchr(SIMD) 定位闭引号/反斜杠 + spanScan(SWAR) 校验控制字符与高位字节。
    // 语义与逐字节扫描一致：控制字符非法（未闭合时也先报控制字符）、
    // 非 ASCII 触发 UTF-8 校验、转义闭引号不提前截断。
    bool parseString(const char*& outData, size_t& outLen, std::string& scratch) {
        char quote = *p_++;
        const unsigned char uquote = static_cast<unsigned char>(quote);
        const char* start = p_;
        bool nonAscii = false;
        const char* close = nullptr;
        const char* bs = nullptr;

        // ---- 阶段 1：逐字节查表扫描前 32 字节 ----
        const char* q = start;
        const char* limit = (end_ - start > 32) ? start + 32 : end_;
        const unsigned char* fl = strFlags();
        while (q < limit) {
            unsigned char c = static_cast<unsigned char>(*q);
            unsigned f = fl[c];
            if (f == 0) { ++q; continue; }    // 普通字符快路径：单表查询
            if (f & SF_CTRL) {
                p_ = q;
                return fail(ParseErrorCode::UnterminatedString, "字符串中含未转义控制字符");
            }
            if (f & SF_NONASCII) nonAscii = true;
            if (c == uquote) { close = q; break; }
            if (c == '\\') { bs = q; break; }   // 转义起点，交 decodeString 处理
            ++q;   // 另一种引号（单引号模式下的 " 等）按普通字符处理
        }

        // ---- 阶段 2（仅 >32 字节长串）：memchr + SWAR ----
        if (!close && !bs) {
            if (q >= end_) {
                p_ = end_;
                return fail(ParseErrorCode::UnterminatedString, "字符串未闭合");
            }
            close = static_cast<const char*>(
                std::memchr(q, quote, static_cast<size_t>(end_ - q)));
            if (!close) {
                bool nonAsciiDummy = false;
                size_t ctrlOff = 0;
                if (spanScan(q, static_cast<size_t>(end_ - q), nonAsciiDummy, ctrlOff)) {
                    p_ = q + ctrlOff;
                    return fail(ParseErrorCode::UnterminatedString,
                                "字符串中含未转义控制字符");
                }
                p_ = end_;
                return fail(ParseErrorCode::UnterminatedString, "字符串未闭合");
            }
            size_t ctrlOff = 0;
            if (spanScan(q, static_cast<size_t>(close - q), nonAscii, ctrlOff)) {
                p_ = q + ctrlOff;
                return fail(ParseErrorCode::UnterminatedString,
                            "字符串中含未转义控制字符");
            }
            bs = static_cast<const char*>(
                std::memchr(q, '\\', static_cast<size_t>(close - q)));
        }

        // ---- 收尾：零拷贝 / 转义解码 + UTF-8 校验（按分支各自的原始串终点）----
        if (!bs) {
            // 纯 ASCII 串无需 UTF-8 校验（绝大多数负载的快路径）
            if (nonAscii && opt_.validateUtf8 &&
                !utf8Valid(start, static_cast<size_t>(close - start)))
                return fail(ParseErrorCode::InvalidUtf8, "字符串中含非法 UTF-8 字节");
            outData = start;
            outLen = static_cast<size_t>(close - start);
            p_ = close + 1;
            return true;
        }
        scratch.assign(start, static_cast<size_t>(bs - start));
        p_ = bs;
        if (!decodeString(scratch, quote)) return false;
        const char* realClose = p_ - 1;   // decodeString 消费完闭引号，回退即其位置
        if (nonAscii && opt_.validateUtf8 &&
            !utf8Valid(start, static_cast<size_t>(realClose - start)))
            return fail(ParseErrorCode::InvalidUtf8, "字符串中含非法 UTF-8 字节");
        outData = scratch.data();
        outLen = scratch.size();
        return true;
    }

    // 慢路径：从 p_ 指向的 '\\' 开始解码，直到闭引号
    bool decodeString(std::string& out, char quote) {
        for (;;) {
            if (p_ >= end_) return fail(ParseErrorCode::UnterminatedString, "字符串未闭合");
            char c = *p_++;
            if (c == quote) return true;
            if (static_cast<unsigned char>(c) < 0x20)
                return fail(ParseErrorCode::UnterminatedString, "字符串中含未转义控制字符");
            if (c != '\\') { out += c; continue; }
            if (p_ >= end_) return fail(ParseErrorCode::UnterminatedString, "字符串未闭合");
            char e = *p_++;
            switch (e) {
            case '"':  out += '"'; break;
            case '\'': out += '\''; break;
            case '\\': out += '\\'; break;
            case '/':  out += '/'; break;
            case 'b':  out += '\b'; break;
            case 'f':  out += '\f'; break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            case 'u': {
                uint32_t cp = 0;
                if (!readHex4(cp)) return false;
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    // 高代理：必须紧跟 \uDC00-DFFF
                    if (p_ + 1 < end_ && *p_ == '\\' && p_[1] == 'u') {
                        const char* save = p_;
                        p_ += 2;
                        uint32_t lo = 0;
                        if (readHex4(lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            utf8Encode(out, cp);
                        } else {
                            p_ = save;
                            return fail(ParseErrorCode::InvalidUnicode, "孤立的高代理项");
                        }
                    } else {
                        return fail(ParseErrorCode::InvalidUnicode, "孤立的高代理项");
                    }
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return fail(ParseErrorCode::InvalidUnicode, "孤立的低代理项");
                } else {
                    utf8Encode(out, cp);
                }
                break;
            }
            default:
                return fail(ParseErrorCode::InvalidEscape, "非法转义字符");
            }
        }
    }

    bool readHex4(uint32_t& v) {
        if (end_ - p_ < 4)
            return fail(ParseErrorCode::InvalidUnicode, "\\u 后缺少 4 位十六进制数字");
        v = 0;
        for (int i = 0; i < 4; ++i) {
            int h = hexVal(*p_++);
            if (h < 0) return fail(ParseErrorCode::InvalidUnicode, "非法十六进制转义");
            v = (v << 4) | static_cast<uint32_t>(h);
        }
        return true;
    }

    bool parseStringValue() {
        const char* data = nullptr;
        size_t len = 0;
        if (!parseString(data, len, strScratch_)) return false;
        handler_.onString(data, len);
        return true;
    }

    bool parseNumber() {
        const char* start = p_;
        bool negative = false;
        if (*p_ == '-') { negative = true; ++p_; }
        if (negative && opt_.allowNaNInf && matchKeyword("Infinity")) {
            handler_.onDouble(-INFINITY);
            return true;
        }

        const char* intBegin = p_;
        uint64_t acc = 0;
        bool overflow = false;
        if (p_ < end_ && *p_ == '0') {
            ++p_;
        } else {
            while (p_ < end_ && isDigit(*p_)) {
                uint64_t d = static_cast<uint64_t>(*p_ - '0');
                // 溢出检测免除法：UINT64_MAX = 1844674407370955161 × 10 + 5
                if (acc > 1844674407370955161ULL ||
                    (acc == 1844674407370955161ULL && d > 5))
                    overflow = true;
                else acc = acc * 10 + d;
                ++p_;
            }
        }
        if (p_ == intBegin) return fail(ParseErrorCode::ExpectedValue, "数字缺少整数部分");

        bool isDouble = false;
        if (p_ < end_ && *p_ == '.') {
            isDouble = true;
            ++p_;
            const char* fracBegin = p_;
            while (p_ < end_ && isDigit(*p_)) ++p_;
            if (p_ == fracBegin) return fail(ParseErrorCode::ExpectedValue, "小数部分缺少数字");
        }
        if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
            isDouble = true;
            ++p_;
            if (p_ < end_ && (*p_ == '+' || *p_ == '-')) ++p_;
            if (p_ >= end_ || !isDigit(*p_)) return fail(ParseErrorCode::ExpectedValue, "数字指数部分非法");
            while (p_ < end_ && isDigit(*p_)) ++p_;
        }
        if (!atDelimiter())
            return fail(ParseErrorCode::ExpectedValue, "数字后有非法字符");

        if (!isDouble && !overflow) {
            if (negative) {
                if (acc <= static_cast<uint64_t>(INT64_MAX) + 1) {
                    int64_t v = (acc == static_cast<uint64_t>(INT64_MAX) + 1)
                        ? INT64_MIN : -static_cast<int64_t>(acc);
                    handler_.onInt(v);
                    return true;
                }
            } else {
                if (acc <= static_cast<uint64_t>(INT64_MAX)) {
                    handler_.onInt(static_cast<int64_t>(acc));
                    return true;
                }
                handler_.onUInt(acc);
                return true;
            }
        }
        // 溢出降级 / 含小数或指数 → double（快速路径位精确；其余退回 strtod，
        // strtod 受全局 locale 影响，见设计文档 §8）
        size_t tokLen = static_cast<size_t>(p_ - start);
        double dv = 0;
        if (tokLen < 64 && detail::fastDouble(start, tokLen, dv)) {
            handler_.onDouble(dv);
        } else if (tokLen < 64) {
            char buf[64];
            std::memcpy(buf, start, tokLen);
            buf[tokLen] = '\0';
            handler_.onDouble(std::strtod(buf, nullptr));
        } else {
            std::string token(start, tokLen);
            handler_.onDouble(std::strtod(token.c_str(), nullptr));
        }
        return true;
    }

    bool parseObject(int depth) {
        ++p_;  // '{'
        handler_.onStartObject();
        if (!skipWs()) return false;
        if (p_ < end_ && *p_ == '}') {
            ++p_;
            handler_.onEndObject();
            return true;
        }
        for (;;) {
            if (!skipWs()) return false;
            const char* keyData = nullptr;
            size_t keyLen = 0;
            if (p_ < end_ && (*p_ == '"' || (*p_ == '\'' && opt_.allowSingleQuotes))) {
                if (!parseString(keyData, keyLen, keyScratch_)) return false;
            } else if (opt_.allowUnquotedKeys) {
                const char* ks = p_;
                while (p_ < end_) {
                    char c = *p_;
                    if (isWs(c) || c == ':' || c == ',' || c == '}') break;
                    ++p_;
                }
                if (p_ == ks) return fail(ParseErrorCode::ExpectedKey, "对象键缺失");
                keyData = ks;
                keyLen = static_cast<size_t>(p_ - ks);
            } else {
                return fail(ParseErrorCode::ExpectedKey, "对象键必须是字符串");
            }
            handler_.onKey(keyData, keyLen);

            if (!skipWs()) return false;
            if (p_ >= end_ || *p_ != ':') return fail(ParseErrorCode::ExpectedColon, "期望 ':'");
            ++p_;

            if (!parseValue(depth + 1)) return false;

            if (!skipWs()) return false;
            if (p_ < end_ && *p_ == ',') {
                ++p_;
                if (opt_.allowTrailingCommas) {
                    if (!skipWs()) return false;
                    if (p_ < end_ && *p_ == '}') {
                        ++p_;
                        handler_.onEndObject();
                        return true;
                    }
                }
                continue;
            }
            if (p_ < end_ && *p_ == '}') {
                ++p_;
                handler_.onEndObject();
                return true;
            }
            return fail(ParseErrorCode::ExpectedCommaOrBrace, "期望 ',' 或 '}'");
        }
    }

    bool parseArray(int depth) {
        ++p_;  // '['
        handler_.onStartArray();
        if (!skipWs()) return false;
        if (p_ < end_ && *p_ == ']') {
            ++p_;
            handler_.onEndArray();
            return true;
        }
        for (;;) {
            if (!parseValue(depth + 1)) return false;
            if (!skipWs()) return false;
            if (p_ < end_ && *p_ == ',') {
                ++p_;
                if (opt_.allowTrailingCommas) {
                    if (!skipWs()) return false;
                    if (p_ < end_ && *p_ == ']') {
                        ++p_;
                        handler_.onEndArray();
                        return true;
                    }
                }
                continue;
            }
            if (p_ < end_ && *p_ == ']') {
                ++p_;
                handler_.onEndArray();
                return true;
            }
            return fail(ParseErrorCode::ExpectedCommaOrBrace, "期望 ',' 或 ']'");
        }
    }

    const char* data_;
    const char* end_;
    const char* p_ = nullptr;
    Handler& handler_;
    const ParseOptions& opt_;
    ParseResult result_;
    std::string strScratch_;   // 字符串解码缓冲（回调期间有效）
    std::string keyScratch_;   // 键解码缓冲（回调期间有效）
};

// ---------------------------------------------------------------- 入口

ParseResult parseSax(const char* data, size_t len, SaxHandler& handler,
                     const ParseOptions& opt) {
    Parser<SaxHandler> parser(data, len, handler, opt);
    return parser.run();
}

ParseResult parse(const char* data, size_t len, Value& out, const ParseOptions& opt) {
    DomBuilder builder;
    Parser<DomBuilder> parser(data, len, builder, opt);   // 直调 DomBuilder，去虚化
    ParseResult r = parser.run();
    if (r.ok) out = builder.take();
    return r;
}

ParseResult parse(const std::string& data, Value& out, const ParseOptions& opt) {
    return parse(data.data(), data.size(), out, opt);
}

} // namespace aujson
