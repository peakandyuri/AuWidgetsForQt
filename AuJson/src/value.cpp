#include "aujson.hpp"

#include <cstring>

namespace aujson {

namespace {
const Value& nullValue() { static const Value n; return n; }
const std::string& emptyString() { static const std::string s; return s; }
Value& arrayOobTrap() { static Value trap; return trap; }   // 数组越界陷阱节点（Null）
} // namespace

// ---------------------------------------------------------------- 构造 / 析构

Value::Value() : type_(Type::Null) {}
Value::Value(std::nullptr_t) : type_(Type::Null) {}
Value::Value(bool b) : type_(Type::Bool) { storage_.b = b; }
Value::Value(int n) : type_(Type::Int) { storage_.i = n; }
Value::Value(long n) : type_(Type::Int) { storage_.i = n; }
Value::Value(long long n) : type_(Type::Int) { storage_.i = n; }
Value::Value(unsigned int n) : type_(Type::UInt) { storage_.u = n; }
Value::Value(unsigned long n) : type_(Type::UInt) { storage_.u = n; }
Value::Value(unsigned long long n) : type_(Type::UInt) { storage_.u = n; }
Value::Value(double d) : type_(Type::Double) { storage_.d = d; }
Value::Value(const char* s) : type_(Type::String) {
    new (&storage_.s) std::string(s ? s : "");
}
Value::Value(const std::string& s) : type_(Type::String) {
    new (&storage_.s) std::string(s);
}
Value::Value(const char* s, size_t len) : type_(Type::String) {
    new (&storage_.s) std::string(s ? s : "", s ? len : 0U);
}

Value::Value(const Value& other) : type_(other.type_) {
    switch (type_) {
    case Type::Bool:   storage_.b = other.storage_.b; break;
    case Type::Int:    storage_.i = other.storage_.i; break;
    case Type::UInt:   storage_.u = other.storage_.u; break;
    case Type::Double: storage_.d = other.storage_.d; break;
    case Type::String: new (&storage_.s) std::string(other.storage_.s); break;
    case Type::Array:  new (&storage_.a) ArrayVec(other.storage_.a); break;
    case Type::Object: new (&storage_.o) ObjectVec(other.storage_.o); break;
    default: break; // Null
    }
}

Value::Value(Value&& other) noexcept : type_(other.type_) {
    switch (type_) {
    case Type::Bool:   storage_.b = other.storage_.b; break;
    case Type::Int:    storage_.i = other.storage_.i; break;
    case Type::UInt:   storage_.u = other.storage_.u; break;
    case Type::Double: storage_.d = other.storage_.d; break;
    case Type::String: new (&storage_.s) std::string(std::move(other.storage_.s)); break;
    case Type::Array:  new (&storage_.a) ArrayVec(std::move(other.storage_.a)); break;
    case Type::Object: new (&storage_.o) ObjectVec(std::move(other.storage_.o)); break;
    default: break; // Null
    }
    other.type_ = Type::Null;
}

Value::~Value() { destroy(); }

Value& Value::operator=(const Value& other) {
    if (this != &other) {
        destroy();
        type_ = other.type_;
        switch (type_) {
        case Type::Bool:   storage_.b = other.storage_.b; break;
        case Type::Int:    storage_.i = other.storage_.i; break;
        case Type::UInt:   storage_.u = other.storage_.u; break;
        case Type::Double: storage_.d = other.storage_.d; break;
        case Type::String: new (&storage_.s) std::string(other.storage_.s); break;
        case Type::Array:  new (&storage_.a) ArrayVec(other.storage_.a); break;
        case Type::Object: new (&storage_.o) ObjectVec(other.storage_.o); break;
        default: break;
        }
    }
    return *this;
}

Value& Value::operator=(Value&& other) noexcept {
    if (this != &other) {
        destroy();
        type_ = other.type_;
        switch (type_) {
        case Type::Bool:   storage_.b = other.storage_.b; break;
        case Type::Int:    storage_.i = other.storage_.i; break;
        case Type::UInt:   storage_.u = other.storage_.u; break;
        case Type::Double: storage_.d = other.storage_.d; break;
        case Type::String: new (&storage_.s) std::string(std::move(other.storage_.s)); break;
        case Type::Array:  new (&storage_.a) ArrayVec(std::move(other.storage_.a)); break;
        case Type::Object: new (&storage_.o) ObjectVec(std::move(other.storage_.o)); break;
        default: break;
        }
        other.type_ = Type::Null;
    }
    return *this;
}

Value Value::makeArray() {
    Value v;
    v.type_ = Type::Array;
    new (&v.storage_.a) ArrayVec();
    return v;
}

Value Value::makeObject() {
    Value v;
    v.type_ = Type::Object;
    new (&v.storage_.o) ObjectVec();
    return v;
}

Value Value::makeArray(std::initializer_list<Value> items) {
    Value v = makeArray();
    v.storage_.a.reserve(items.size());
    for (const Value& item : items) v.storage_.a.push_back(item);
    return v;
}

Value Value::makeObject(std::initializer_list<std::pair<std::string, Value>> items) {
    Value v = makeObject();
    v.storage_.o.reserve(items.size());
    for (const auto& item : items) v.storage_.o.emplace_back(item.first, item.second);
    return v;
}

// ---------------------------------------------------------------- 内部工具

void Value::destroy() {
    switch (type_) {
    case Type::String: storage_.s.~basic_string(); break;
    case Type::Array:  storage_.a.~vector(); break;
    case Type::Object: storage_.o.~vector(); break;
    default: break;
    }
    type_ = Type::Null;
}

void Value::become(Type t) {
    destroy();
    type_ = t;
    if (t == Type::String)      new (&storage_.s) std::string();
    else if (t == Type::Array)  new (&storage_.a) ArrayVec();
    else if (t == Type::Object) new (&storage_.o) ObjectVec();
}

// ---------------------------------------------------------------- 类型判别

Type Value::type() const { return type_; }
bool Value::isNull()   const { return type_ == Type::Null; }
bool Value::isBool()   const { return type_ == Type::Bool; }
bool Value::isInt()    const { return type_ == Type::Int; }
bool Value::isUInt()   const { return type_ == Type::UInt; }
bool Value::isDouble() const { return type_ == Type::Double; }
bool Value::isNumber() const {
    return type_ == Type::Int || type_ == Type::UInt || type_ == Type::Double;
}
bool Value::isString() const { return type_ == Type::String; }
bool Value::isArray()  const { return type_ == Type::Array; }
bool Value::isObject() const { return type_ == Type::Object; }

const char* Value::typeName() const {
    switch (type_) {
    case Type::Null:   return "null";
    case Type::Bool:   return "bool";
    case Type::Int:    return "int";
    case Type::UInt:   return "uint";
    case Type::Double: return "double";
    case Type::String: return "string";
    case Type::Array:  return "array";
    case Type::Object: return "object";
    }
    return "null";
}

// ---------------------------------------------------------------- 安全取值（类型不符返回默认值）

bool Value::asBool(bool def) const {
    switch (type_) {
    case Type::Bool:   return storage_.b;
    case Type::Int:    return storage_.i != 0;
    case Type::UInt:   return storage_.u != 0;
    case Type::Double: return storage_.d != 0.0;
    default:           return def;
    }
}

int64_t Value::asInt(int64_t def) const {
    switch (type_) {
    case Type::Int:    return storage_.i;
    case Type::UInt:   return storage_.u <= static_cast<uint64_t>(INT64_MAX)
                                 ? static_cast<int64_t>(storage_.u) : def;
    case Type::Double:
        if (storage_.d >= -9223372036854775808.0 && storage_.d < 9223372036854775808.0)
            return static_cast<int64_t>(storage_.d + (storage_.d >= 0 ? 0.5 : -0.5));
        return def;
    case Type::Bool:   return storage_.b ? 1 : 0;
    default:           return def;
    }
}

uint64_t Value::asUInt(uint64_t def) const {
    switch (type_) {
    case Type::UInt:   return storage_.u;
    case Type::Int:    return storage_.i >= 0 ? static_cast<uint64_t>(storage_.i) : def;
    case Type::Double:
        if (storage_.d >= 0.0 && storage_.d < 18446744073709551616.0)
            return static_cast<uint64_t>(storage_.d + 0.5);
        return def;
    case Type::Bool:   return storage_.b ? 1 : 0;
    default:           return def;
    }
}

double Value::asDouble(double def) const {
    switch (type_) {
    case Type::Double: return storage_.d;
    case Type::Int:    return static_cast<double>(storage_.i);
    case Type::UInt:   return static_cast<double>(storage_.u);
    case Type::Bool:   return storage_.b ? 1.0 : 0.0;
    default:           return def;
    }
}

const std::string& Value::asString() const {
    return type_ == Type::String ? storage_.s : emptyString();
}

const char* Value::asCString() const {
    return type_ == Type::String ? storage_.s.c_str() : nullptr;
}

// ---------------------------------------------------------------- 显式转换（仅精确类型成功）

bool Value::toBool(bool* out) const {
    if (type_ != Type::Bool) return false;
    if (out) *out = storage_.b;
    return true;
}

bool Value::toInt(int64_t* out) const {
    if (type_ != Type::Int) return false;
    if (out) *out = storage_.i;
    return true;
}

bool Value::toUInt(uint64_t* out) const {
    if (type_ != Type::UInt) return false;
    if (out) *out = storage_.u;
    return true;
}

bool Value::toDouble(double* out) const {
    switch (type_) {
    case Type::Double: if (out) *out = storage_.d; return true;
    case Type::Int:    if (out) *out = static_cast<double>(storage_.i); return true;
    case Type::UInt:   if (out) *out = static_cast<double>(storage_.u); return true;
    default:           return false;
    }
}

// ---------------------------------------------------------------- 容器访问

size_t Value::size() const {
    switch (type_) {
    case Type::Array:  return storage_.a.size();
    case Type::Object: return storage_.o.size();
    case Type::String: return storage_.s.size();
    default:           return 0;
    }
}

bool Value::empty() const { return size() == 0; }

void Value::clear() {
    if (type_ == Type::Array)       storage_.a.clear();
    else if (type_ == Type::Object) storage_.o.clear();
}

Value& Value::operator[](size_t index) {
    if (type_ != Type::Array) become(Type::Array);
    if (index >= storage_.a.size()) return arrayOobTrap();  // 越界：陷阱 Null（写入无效）
    return storage_.a[index];
}

const Value& Value::operator[](size_t index) const {
    if (type_ == Type::Array && index < storage_.a.size()) return storage_.a[index];
    return nullValue();
}

Value& Value::operator[](const char* key) {
    if (type_ != Type::Object) become(Type::Object);
    for (auto& kv : storage_.o)
        if (kv.first == key) return kv.second;
    storage_.o.emplace_back(key, Value());
    return storage_.o.back().second;
}

Value& Value::operator[](const std::string& key) {
    if (type_ != Type::Object) become(Type::Object);
    for (auto& kv : storage_.o)
        if (kv.first == key) return kv.second;
    storage_.o.emplace_back(key, Value());
    return storage_.o.back().second;
}

const Value& Value::operator[](const char* key) const {
    if (type_ == Type::Object)
        for (const auto& kv : storage_.o)
            if (kv.first == key) return kv.second;
    return nullValue();
}

const Value& Value::operator[](const std::string& key) const {
    if (type_ == Type::Object)
        for (const auto& kv : storage_.o)
            if (kv.first == key) return kv.second;
    return nullValue();
}

bool Value::contains(const char* key) const {
    return find(key) != nullptr;
}

bool Value::contains(size_t index) const {
    return type_ == Type::Array && index < storage_.a.size();
}

Value* Value::find(const char* key) {
    if (type_ != Type::Object) return nullptr;
    for (auto& kv : storage_.o)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

const Value* Value::find(const char* key) const {
    if (type_ != Type::Object) return nullptr;
    for (const auto& kv : storage_.o)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

Value& Value::push_back(Value v) {
    if (type_ != Type::Array) become(Type::Array);
    storage_.a.push_back(std::move(v));
    return storage_.a.back();
}

void Value::resize(size_t n) {
    if (type_ != Type::Array) become(Type::Array);
    if (n > storage_.a.size()) storage_.a.resize(n);
}

void Value::reserve(size_t n) {
    if (type_ == Type::Array) {
        if (n > storage_.a.size()) storage_.a.reserve(n);
    } else if (type_ == Type::Object) {
        if (n > storage_.o.size()) storage_.o.reserve(n);
    }
}

Value& Value::set(const char* key, const Value& v) {
    (*this)[key] = v;
    return *this;
}

Value& Value::insertKey(std::string&& key, Value v) {
    if (type_ != Type::Object) become(Type::Object);
    for (auto& kv : storage_.o)
        if (kv.first == key) { kv.second = std::move(v); return kv.second; }
    storage_.o.emplace_back(std::move(key), std::move(v));
    return storage_.o.back().second;
}

bool Value::remove(const char* key) {
    if (type_ != Type::Object) return false;
    for (auto it = storage_.o.begin(); it != storage_.o.end(); ++it) {
        if (it->first == key) {
            storage_.o.erase(it);
            return true;
        }
    }
    return false;
}

bool Value::remove(size_t index) {
    if (type_ != Type::Array || index >= storage_.a.size()) return false;
    storage_.a.erase(storage_.a.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

// ---------------------------------------------------------------- 迭代

Value::Item Value::Iterator::operator*() const {
    Item item;
    if (mode_ == Array) {
        item.key = nullptr;
        item.value = &arr_->at(i_);
    } else if (mode_ == Object) {
        item.key = &obj_->at(i_).first;
        item.value = &obj_->at(i_).second;
    }
    return item;
}

Value::Iterator& Value::Iterator::operator++() {
    ++i_;
    return *this;
}

bool Value::Iterator::operator!=(const Iterator& other) const {
    return mode_ != other.mode_ || i_ != other.i_;
}

bool Value::Iterator::operator==(const Iterator& other) const {
    return !(*this != other);
}

Value::Iterator::Iterator(Mode m, detail::ArrayVec* a,
                          detail::ObjectVec* o, size_t i)
    : mode_(m), arr_(a), obj_(o), i_(i) {}

Value::ConstItem Value::ConstIterator::operator*() const {
    ConstItem item;
    if (mode_ == Array) {
        item.key = nullptr;
        item.value = &arr_->at(i_);
    } else if (mode_ == Object) {
        item.key = &obj_->at(i_).first;
        item.value = &obj_->at(i_).second;
    }
    return item;
}

Value::ConstIterator& Value::ConstIterator::operator++() {
    ++i_;
    return *this;
}

bool Value::ConstIterator::operator!=(const ConstIterator& other) const {
    return mode_ != other.mode_ || i_ != other.i_;
}

bool Value::ConstIterator::operator==(const ConstIterator& other) const {
    return !(*this != other);
}

Value::ConstIterator::ConstIterator(Mode m, const detail::ArrayVec* a,
                                    const detail::ObjectVec* o, size_t i)
    : mode_(m), arr_(a), obj_(o), i_(i) {}

Value::Iterator Value::begin() {
    if (type_ == Type::Array)  return Iterator(Iterator::Array, &storage_.a, nullptr, 0);
    if (type_ == Type::Object) return Iterator(Iterator::Object, nullptr, &storage_.o, 0);
    return Iterator();
}

Value::Iterator Value::end() {
    if (type_ == Type::Array)  return Iterator(Iterator::Array, &storage_.a, nullptr, storage_.a.size());
    if (type_ == Type::Object) return Iterator(Iterator::Object, nullptr, &storage_.o, storage_.o.size());
    return Iterator();
}

Value::ConstIterator Value::begin() const {
    if (type_ == Type::Array)  return ConstIterator(ConstIterator::Array, &storage_.a, nullptr, 0);
    if (type_ == Type::Object) return ConstIterator(ConstIterator::Object, nullptr, &storage_.o, 0);
    return ConstIterator();
}

Value::ConstIterator Value::end() const {
    if (type_ == Type::Array)  return ConstIterator(ConstIterator::Array, &storage_.a, nullptr, storage_.a.size());
    if (type_ == Type::Object) return ConstIterator(ConstIterator::Object, nullptr, &storage_.o, storage_.o.size());
    return ConstIterator();
}

} // namespace aujson
