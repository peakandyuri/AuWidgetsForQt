#include "aujson.hpp"

#include <cstdlib>
#include <cstring>

namespace aujson {

namespace {

bool isAllDigits(const std::string& seg) {
    if (seg.empty()) return false;
    for (size_t i = 0; i < seg.size(); ++i)
        if (seg[i] < '0' || seg[i] > '9') return false;
    return true;
}

} // namespace

Value& Value::at(const char* path) { return atPath(*this, path); }
const Value& Value::at(const char* path) const { return atPath(*this, path); }

Value& atPath(Value& root, const char* path) {
    static Value nullSingleton;  // 无效路径只读陷阱（写入无效）
    if (!path) return nullSingleton;

    Value* cur = &root;
    const char* p = path;
    while (*p) {
        const char* dot = std::strchr(p, '.');
        size_t len = dot ? static_cast<size_t>(dot - p) : std::strlen(p);
        if (len == 0) return nullSingleton;  // 空段（"a..b" / 尾随点）
        std::string seg(p, len);

        if (cur->type() == Type::Array) {
            if (!isAllDigits(seg)) return nullSingleton;
            size_t idx = static_cast<size_t>(std::strtoull(seg.c_str(), nullptr, 10));
            cur->resize(idx + 1);  // 越界自动补 Null
            cur = &(*cur)[idx];
        } else {
            if (!cur->isObject()) *cur = Value::makeObject();
            cur = &(*cur)[seg];
        }
        if (!dot) break;
        p = dot + 1;
    }
    return *cur;
}

const Value& atPath(const Value& root, const char* path) {
    static Value nullSingleton;
    if (!path) return nullSingleton;

    const Value* cur = &root;
    const char* p = path;
    while (*p) {
        const char* dot = std::strchr(p, '.');
        size_t len = dot ? static_cast<size_t>(dot - p) : std::strlen(p);
        if (len == 0) return nullSingleton;
        std::string seg(p, len);

        if (cur->type() == Type::Array) {
            if (!isAllDigits(seg)) return nullSingleton;
            size_t idx = static_cast<size_t>(std::strtoull(seg.c_str(), nullptr, 10));
            if (idx >= cur->size()) return nullSingleton;
            cur = &(*cur)[idx];
        } else if (cur->type() == Type::Object) {
            const Value* found = cur->find(seg.c_str());
            if (!found) return nullSingleton;
            cur = found;
        } else {
            return nullSingleton;
        }
        if (!dot) break;
        p = dot + 1;
    }
    return *cur;
}

} // namespace aujson
