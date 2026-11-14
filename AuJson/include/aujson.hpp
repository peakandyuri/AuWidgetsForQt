#ifndef AUJSON_HPP
#define AUJSON_HPP

// AuJson 核心层总入口（对外唯一核心头，nlohmann 同款单头模式）。
// 使用方只需 #include "aujson.hpp"；内部实现头在 src/detail/，请勿直接引用。
//
// 设计要点（见 docs/AuJson-设计文档.md）：
//   - 单一 Value 类型（对比 Qt 的 QJsonDocument/QJsonObject/QJsonArray/QJsonValue 四件套）；
//   - 数字按 int64 / uint64 / double 分类型存储，>2^53 整数不丢精度；
//   - 字符串为 UTF-8 std::string；Object 保序（序列化还原输入顺序）；
//   - 无异常：所有 API 返回错误码 / 默认值，不 throw、不依赖 RTTI。

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace aujson {

class Value;

namespace detail {

// ---- slab 池分配器（内部）----
// 线程本地 slab 池：容器 buffer 按 16 字节粒度分桶复用（≤1KB），空桶时从
// 64KB slab 连续切割（保证局部性）；>1KB 直接 malloc。无状态分配器，
// Value 的深拷贝 / 移动 / 大小语义完全不变。
// 归由路由 O(1)：每块前置 8B 头记录归属（slab 下标 / malloc 标记），
// 空闲块按桶内嵌自由链复用，回收无需线性查找指针归属。
// 开关见 aujson::setSlabPoolEnabled()；低内存设备可关闭退化为直接 malloc/free，
// 关闭状态下分配 / 回收按指针归属正确路由，可随时切换。

void* poolAlloc(size_t bytes);
void  poolDealloc(void* p, size_t bytes) noexcept;

template <class T>
struct PoolAlloc {
    typedef T value_type;
    PoolAlloc() noexcept {}
    template <class U> PoolAlloc(const PoolAlloc<U>&) noexcept {}
    T* allocate(size_t n) { return static_cast<T*>(poolAlloc(n * sizeof(T))); }
    void deallocate(T* p, size_t n) noexcept { poolDealloc(p, n * sizeof(T)); }
};
template <class T, class U>
inline bool operator==(const PoolAlloc<T>&, const PoolAlloc<U>&) noexcept { return true; }
template <class T, class U>
inline bool operator!=(const PoolAlloc<T>&, const PoolAlloc<U>&) noexcept { return false; }

// 容器类型（不完整 Value 的 vector 实例化依赖主流 STL 的既有支持，见设计文档）
using ArrayVec  = std::vector<Value, PoolAlloc<Value>>;
using ObjectVec = std::vector<std::pair<std::string, Value>,
                              PoolAlloc<std::pair<std::string, Value>>>;

} // namespace detail

// ============================================================ 数据模型 Value

// 值类型（与 JSON 一一对应；整数按符号分 Int/UInt 存储）
enum class Type { Null, Bool, Int, UInt, Double, String, Array, Object };

class Value {
public:
    // ---- 构造 / 析构（单参构造函数非 explicit，支持字面量直接赋值）----
    Value();                                     // Null
    Value(std::nullptr_t);                       // Null
    Value(bool b);
    Value(int n);
    Value(long n);
    Value(long long n);
    Value(unsigned int n);
    Value(unsigned long n);
    Value(unsigned long long n);
    Value(double d);
    Value(const char* s);                        // UTF-8
    Value(const std::string& s);
    Value(const char* s, size_t len);            // UTF-8，指定长度（解析热路径免 strlen）
    Value(const Value& other);
    Value(Value&& other) noexcept;
    ~Value();

    static Value makeArray();
    static Value makeObject();
    static Value makeArray(std::initializer_list<Value> items);
    static Value makeObject(std::initializer_list<std::pair<std::string, Value>> items);

    Value& operator=(const Value& other);
    Value& operator=(Value&& other) noexcept;

    // ---- 类型判别 ----
    Type type() const;
    bool isNull()   const;
    bool isBool()   const;
    bool isInt()    const;
    bool isUInt()   const;
    bool isDouble() const;
    bool isNumber() const;   // Int / UInt / Double
    bool isString() const;
    bool isArray()  const;
    bool isObject() const;
    const char* typeName() const;

    // ---- 安全取值（类型不符返回默认值，不抛异常）----
    bool                 asBool(bool def = false) const;
    int64_t              asInt(int64_t def = 0) const;
    uint64_t             asUInt(uint64_t def = 0) const;
    double               asDouble(double def = 0) const;
    const std::string&   asString() const;       // 非 String → 静态空串
    const char*          asCString() const;      // 非 String → nullptr

    // ---- 显式转换（仅精确类型成功，返回是否成功）----
    bool toBool(bool* out) const;
    bool toInt(int64_t* out) const;
    bool toUInt(uint64_t* out) const;
    bool toDouble(double* out) const;

    // ---- 容器访问 ----
    size_t size() const;    // Array/Object 元素数；String 字节数；其余 0
    bool   empty() const;
    void   clear();         // 仅对 Array/Object 生效（清空元素）

    // 数组下标：越界返回内部陷阱 Null（读安全、写入无效，不静默扩容）。
    // 构造期扩容用 push_back / resize / at() 路径。
    // 整型模板版本精确匹配整数字面量，规避 0 字面量与指针重载的歧义；负索引按 0 处理（不抛异常）。
    Value&       operator[](size_t index);
    const Value& operator[](size_t index) const;
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    Value& operator[](T index) {
        return (*this)[static_cast<size_t>(index < 0 ? 0 : index)];
    }
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    const Value& operator[](T index) const {
        return (*this)[static_cast<size_t>(index < 0 ? 0 : index)];
    }

    // 对象键：非常量版缺键自动插入 Null（非 Object 先转为 Object，原值丢弃）；常量版缺键返回静态 Null
    Value&       operator[](const char* key);
    Value&       operator[](const std::string& key);
    const Value& operator[](const char* key) const;
    const Value& operator[](const std::string& key) const;

    bool   contains(const char* key) const;
    bool   contains(size_t index) const;
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    bool contains(T index) const {
        return contains(static_cast<size_t>(index < 0 ? 0 : index));
    }
    Value* find(const char* key);
    const Value* find(const char* key) const;

    Value& push_back(Value v);                     // 追加数组元素（非 Array 先转 Array）
    void   resize(size_t n);                       // 数组扩容（补 Null）；非 Array 先转 Array
    void   reserve(size_t n);                      // 容器预留（解析期减少 realloc；其余类型无操作）
    Value& set(const char* key, const Value& v);   // 链式写：v.set("a",1)("b",2)
    Value& insertKey(std::string&& key, Value v);  // 对象插入（移动键，避免拷贝；存在则覆盖）
    bool   remove(const char* key);                // 删除对象键；不存在返回 false
    bool   remove(size_t index);                   // 删除数组元素；越界返回 false

    // 点分路径访问（"a.b.0.c"），见下方 atPath()
    Value&       at(const char* path);
    const Value& at(const char* path) const;

    // ---- 迭代 ----
    // Item：key 为数组元素时是 nullptr；value 指向节点本身
    struct Item {
        const std::string* key = nullptr;
        Value*             value = nullptr;
    };
    struct ConstItem {
        const std::string* key = nullptr;
        const Value*       value = nullptr;
    };

    class Iterator {
    public:
        Iterator() = default;
        Item operator*() const;
        Iterator& operator++();
        bool operator!=(const Iterator& other) const;
        bool operator==(const Iterator& other) const;
    private:
        enum Mode { None, Array, Object };
        Iterator(Mode m, detail::ArrayVec* a, detail::ObjectVec* o, size_t i);
        Mode mode_ = None;
        detail::ArrayVec* arr_ = nullptr;
        detail::ObjectVec* obj_ = nullptr;
        size_t i_ = 0;
        friend class Value;
    };

    class ConstIterator {
    public:
        ConstIterator() = default;
        ConstItem operator*() const;
        ConstIterator& operator++();
        bool operator!=(const ConstIterator& other) const;
        bool operator==(const ConstIterator& other) const;
    private:
        enum Mode { None, Array, Object };
        ConstIterator(Mode m, const detail::ArrayVec* a,
                      const detail::ObjectVec* o, size_t i);
        Mode mode_ = None;
        const detail::ArrayVec* arr_ = nullptr;
        const detail::ObjectVec* obj_ = nullptr;
        size_t i_ = 0;
        friend class Value;
    };

    Iterator      begin();
    Iterator      end();
    ConstIterator begin()  const;
    ConstIterator end()    const;

private:
    using ArrayVec  = detail::ArrayVec;
    using ObjectVec = detail::ObjectVec;

    // 联合存储：活跃成员由 type_ 决定，手动构造 / 析构
    union Storage {
        bool   b;
        int64_t i;
        uint64_t u;
        double d;
        std::string s;
        ArrayVec  a;
        ObjectVec o;
        Storage() {}
        ~Storage() {}
    } storage_;
    Type type_ = Type::Null;

    void destroy();
    void become(Type t);
};

// ============================================================ 解析

// 解析选项（默认全关 = 严格 JSON，严格度与 Qt 一致）
struct ParseOptions {
    bool allowComments = false;        // // 行注释与 /* */ 块注释
    bool allowTrailingCommas = false;  // 数组 / 对象尾逗号
    bool allowSingleQuotes = false;    // 单引号字符串
    bool allowUnquotedKeys = false;    // 无引号键（JS 风格）
    bool allowNaNInf = false;          // NaN / Infinity 字面量（非标准）
    bool validateUtf8 = true;          // 校验字符串内 UTF-8 合法性
    int  maxDepth = 256;               // 嵌套深度上限（防爆栈）
};

enum class ParseErrorCode {
    ExpectedValue,         // 期望一个值
    ExpectedKey,           // 对象内期望字符串键
    ExpectedColon,         // 期望 ':'
    ExpectedCommaOrBrace,  // 期望 ',' 或 '}' / ']'
    UnterminatedString,    // 字符串未闭合
    InvalidEscape,         // 非法转义
    InvalidUnicode,        // 非法 \uXXXX（含孤立代理）
    InvalidUtf8,           // 非法 UTF-8 字节
    InvalidLiteral,        // true / false / null 写法不合法
    UnclosedComment,       // 注释未闭合
    DepthExceeded,         // 嵌套深度超过 maxDepth
    TrailingCharacters,    // 值之后有多余字符
};

struct ParseError {
    ParseErrorCode code = ParseErrorCode::ExpectedValue;
    std::string message;   // 中文描述
    size_t offset = 0;     // 字节偏移（0-based）
    size_t line = 1;       // 1-based
    size_t column = 1;     // 1-based
};

struct ParseResult {
    bool ok = false;
    ParseError error;
};

// DOM 解析：失败时 out 保持原值不被修改
ParseResult parse(const char* data, size_t len, Value& out,
                  const ParseOptions& opt = ParseOptions());
ParseResult parse(const std::string& data, Value& out,
                  const ParseOptions& opt = ParseOptions());

// ============================================================ SAX 流式解析

// SAX 回调基类（全空实现，用户按需 override）。
// 适合大文件 / 只取局部的场景：未转义字符串回调直接指向输入缓冲（零拷贝），
// 回调返回后即失效，需要保留时请自行拷贝。
class SaxHandler {
public:
    virtual ~SaxHandler();
    virtual void onNull() {}
    virtual void onBool(bool /*v*/) {}
    virtual void onInt(int64_t /*v*/) {}
    virtual void onUInt(uint64_t /*v*/) {}
    virtual void onDouble(double /*v*/) {}
    virtual void onString(const char* /*data*/, size_t /*len*/) {}  // 指向输入缓冲
    virtual void onKey(const char* /*data*/, size_t /*len*/) {}     // 同上
    virtual void onStartArray() {}
    virtual void onEndArray() {}
    virtual void onStartObject() {}
    virtual void onEndObject() {}
};

// SAX 解析入口（内部实现在 parse.cpp；DOM 解析即基于同一扫描器 + 构建器）
ParseResult parseSax(const char* data, size_t len, SaxHandler& handler,
                     const ParseOptions& opt = ParseOptions());

// ============================================================ 序列化

struct WriteOptions {
    bool pretty = false;          // 缩进格式化
    int  indent = 2;              // pretty 时每级缩进空格数
    bool ensureAscii = false;     // 非 ASCII 转 \uXXXX
    bool escapeSlash = false;     // 转义 /
    bool sortKeys = false;        // 对象键排序输出（不改存储顺序）
    bool newlineAtEnd = false;    // 末尾追加 \n
};

std::string stringify(const Value& v, const WriteOptions& opt = WriteOptions());

// ============================================================ 点分路径访问（v1）

//   - 语法 "a.b.0.c"：对象按键、数组按数字段索引；
//   - 非常量版自动创建路径（缺键插入 Null，数组越界自动补 Null）；
//   - 常量版路径无效时返回静态 Null；
//   - 键本身含 '.' 时请用 find() / operator[]（路径语法不支持转义，v1 限制）；
//   - JSON Pointer（RFC 6901）列入 v1.1。

Value&       atPath(Value& root, const char* path);
const Value& atPath(const Value& root, const char* path);

// ============================================================ slab 池开关

// 容器内存池开关（默认开启）：关闭后容器分配退化为直接 malloc/free。
// 低内存设备可关闭；池会持有已释放块待复用（高水位内存），关闭即释放该行为。
// 线程安全：任意时刻可切换（分配 / 回收按指针归属正确路由）。
void setSlabPoolEnabled(bool on);
bool slabPoolEnabled();

// 释放当前线程池中缓存的内存块与 slab（解析大文件后可手动调用归还内存）。
// 池为 POD：线程退出不自动释放本线程缓存（进程退出由系统回收），依赖即时回收时请调用本函数
void releasePoolCache();

} // namespace aujson

#endif // AUJSON_HPP
