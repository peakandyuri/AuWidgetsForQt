# AuJson 设计文档（v1.0，已实现）

> 状态：**已实现**（核心层 + Qt 适配层 + 测试 + 基准；决策点见 §10，实测数据见 §5）。
> 背景：用自研 JSON 库替换 Qt 原生 `QJsonDocument` 系 API。当前仓库尚无任何 JSON 使用（纯新引入，无历史包袱）。

## 1. 目标与定位

- **高性能**：单遍解析、连续存储、零拷贝 SAX，目标解析/序列化 ≥3x 于 `QJsonDocument`（以实测为准）。
- **使用简易**：单一 `Value` 类型 + `operator[]` 链式访问 + 全类型带默认值取值，无异常、不抛错。
- **两条使用路径**：
  - **原生层 `aujson`**：零 Qt 依赖（C++11 标准库），未来非 Qt 项目直接使用。
  - **Qt 层 `aujson::qt`**：与 Qt 生态互通（`QByteArray` / `QVariant` / `QJsonDocument`），现有 Qt 代码迁移成本最低。
- **非目标**：不做二进制格式（QCbor）、不做 JSON Schema 校验、不做 JSONPath（v1.1 再评估）。

## 2. 总体架构

```
使用方代码
   │
   ├─ 非 Qt 项目 ──→ aujson（核心层：纯 C++11，零 Qt 依赖）
   │                  Value / parse / stringify / SAX / 选项 / 错误
   │
   └─ Qt 项目 ─────→ aujson::qt（适配层：仅依赖 QtCore + 核心层）
                      QByteArray / QVariant / QJsonDocument 互转 + QJsonDocument 风格薄封装
```

- 核心层不 include 任何 Qt 头；适配层只做边界转换，不改核心语义。
- 许可证跟随仓库（MIT）。

## 3. 核心层设计（aujson）

### 3.1 数据模型

单一 `aujson::Value`（对比 Qt 的 QJsonDocument/QJsonObject/QJsonArray/QJsonValue 四件套）：

- 类型：`Null / Bool / Int(int64) / UInt(uint64) / Double / String(UTF-8) / Array / Object`。
- 数字三分（int64/uint64/double 分存）→ **>2^53 整数不丢精度**（Qt 全 double，会丢）。
- `Array = std::vector<Value>`；`Object = 保序 vector<pair<string,Value>> + 线性查找`
  （配置类负载对象键数小，连续存储缓存友好且**序列化保持输入顺序**；Qt 内部哈希序不保序）。
  超大对象（数百键以上）场景 v1.1 可加索引，v1 保持简单。
- **无异常**：所有 API 返回错误码/默认值，不 throw、不依赖 RTTI；Qt 工程里关异常也能用。
- 移动语义 noexcept；复制即深拷贝（无 COW，别处持有没有线程安全坑）。

### 3.2 解析

- DOM：`ParseResult parse(data, len, Value&, opts)`。
- SAX：`parseSax(data, len, Handler&, opts)`，Handler 继承 `SaxHandler` 基类按需 override；
  未转义字符串回调直接指向输入缓冲 → **零拷贝**。
- 错误：`{ code, message, offset, line, column }`（Qt 只有字节 offset + 消息，无行列）。
- 选项 `ParseOptions`：`allowComments / allowTrailingCommas / allowSingleQuotes /
  allowUnquotedKeys / allowNaNInf / validateUtf8 / maxDepth(默认256)`；默认全关即严格模式
  （严格度与 Qt 一致）。
- 细节：跳过 UTF-8 BOM；重复键后者覆盖、位置取首次（与 `QJsonObject` 一致）；
  整数溢出 int64/uint64 自动降级 double；非法 UTF-8 默认报错。

### 3.3 序列化

`stringify(v, opts)`；`WriteOptions{ pretty, indent, ensureAscii, escapeSlash, sortKeys, newlineAtEnd }`。

- 浮点：`%.15g → %.16g → %.17g` 逐级试回读，取最短 round-trip（nlohmann 同款思路）。
- 非有限值（NaN/Inf）输出 `null`，不产生非法 JSON。
- 输出预分配 + 倍增扩容。

### 3.4 访问与修改（易用性核心）

```cpp
aujson::Value v;
v["user"]["name"] = "张三";            // operator[] 缺键自动建 object（非 object 先转为 object）
v["user"]["age"]  = 30;
v["tags"].push_back("c++");
v.contains("user");  v.find("user");   // find 返回 Value*，不存在返回 nullptr
std::string name = v["user"]["name"].asString();   // 类型不符 → 返回默认值，不抛异常
int64_t   age  = v["user"]["age"].asInt(0);        // asX(def) 全类型带默认值
bool      ok   = v["user"]["age"].toInt(&out);     // toX(&out) 显式转换，带成功标志
const aujson::Value& n = v.at("user.name");        // 点分路径（v1）；键本身含 "." 时用 find()
for (auto& item : v["user"].asObject()) { item.key; item.value; }   // C++11 可迭代
for (auto& el : v["tags"]) { ... }
```

- const 访问越界/缺键 → 静态 Null 单例（读安全）；非 const `operator[]` 缺键插入 Null。
- 数组下标越界不静默扩容（读返回陷阱 Null、写入无效）；构造期扩容用 `push_back` /
  `resize` / `at()` 路径（`at()` 数组段越界自动补 Null）。

### 3.5 签名级速览

```cpp
namespace aujson {
enum class Type { Null, Bool, Int, UInt, Double, String, Array, Object };

class Value { /* 类型判别 + asX/toX + operator[] + push_back/set/remove/clear + 迭代 */ };

struct ParseError { int code; const char* message; size_t offset, line, column; };
struct ParseResult  { bool ok; ParseError error; };
struct ParseOptions { bool allowComments, allowTrailingCommas, allowSingleQuotes,
                      allowUnquotedKeys, allowNaNInf, validateUtf8; int maxDepth; };
struct WriteOptions { bool pretty, ensureAscii, escapeSlash, sortKeys, newlineAtEnd; int indent; };

ParseResult parse(const char* data, size_t len, Value& out, const ParseOptions& = {});

// SAX：onNull/onBool/onInt/onUInt/onDouble/onString(data,len)/onKey(data,len)/
//     onStartArray/onEndArray/onStartObject/onEndObject
ParseResult parseSax(const char*, size_t, SaxHandler&, const ParseOptions& = {});

std::string stringify(const Value& v, const WriteOptions& = {});
}
```

### 3.6 迭代

```cpp
for (auto& item : obj) {          // Object：item.key / item.value
    if (item.key) qDebug() << item.key->c_str();
}
for (auto& item : arr) {          // Array：item.key 为 nullptr
}
// const 版本返回 ConstIterator（ConstItem 同构，value 为 const Value*）
```

## 4. Qt 适配层（aujson::qt）

```cpp
namespace aujson::qt {
bool       parse(const QByteArray&, aujson::Value&, QString* err = nullptr,
                 const aujson::ParseOptions& = {});
QByteArray stringify(const aujson::Value&, const aujson::WriteOptions& = {});

aujson::Value fromVariant(const QVariant&);      QVariant      toVariant(const aujson::Value&);
aujson::Value fromQJsonValue(const QJsonValue&); QJsonValue     toQJsonValue(const aujson::Value&);
QJsonDocument   toQJsonDocument(const aujson::Value&);
aujson::Value   fromQJsonDocument(const QJsonDocument&);

class JsonDocument { /* 模仿 QJsonDocument 的薄封装：fromJson/toJson/isArray/isObject/root() */ };
}
```

- `QVariant` 边界注明：`uint64 > 2^63-1` 落 QVariant 会降级 double（精度损失）。
- `toQJsonValue` 大整数转 double 会丢精度（Qt 限制，文档注明）。
- 头文件 `aujson_qt.hpp`（与核心层总入口 `aujson.hpp` 同层），仅 QtCore 依赖。

## 5. 性能设计（为什么快）

1. 单遍递归下降，无回溯，O(n)；空白/分隔符查表 + memchr。
2. 字符串转义一次遍历；SAX 模式未转义串零拷贝。
3. 数字双快速路径：解析 ≤15 位有效数字走自研十进制→double（位精确同 strtod，否则退回
   strtod）；序列化整数形态 / 短十进制小数（≤9 位小数）手写输出 + 快速回读验证。
4. vector 连续存储，缓存友好；序列化输出预分配、缩进串按级缓存。
5. 无异常 / 无 RTTI / 热路径无虚函数（SAX 回调仅在解析期）。
6. **slab 内存池（默认开启）**：容器 buffer 走线程本地 slab 池——按 16B 粒度分桶复用、
   空桶从 64KB slab 连续切割（局部性）、slab 记录 切出/归还 计数（全部归还才整体回收）。
   分配器无状态挂 `std::vector`，Value 语义与 40 字节大小完全不变；
   `setSlabPoolEnabled(false)` 随时退化为直接 malloc/free（低内存设备），跨开关指针归属正确路由。
   回收 O(1)：每块前置 8B 头记录归属（slab 下标 / malloc 标记），空闲块按桶内嵌自由链复用，
   免去回收期线性查找指针归属（v1 曾按 slab 区间线性扫描，对象密集负载回收开销占比显著）。
   长 DOM 组装提速：`Value(const char*, size_t)` 长字符串单次拷贝，避免临时 string 二次分配。
   池对象 POD 化（无构造/析构/NSDMI，全零即空池）：thread_local 免去 emutls 初始化守卫与
   线程退出析构注册；在支持原生 TLS 的工具链上（GCC 13+ Windows、Linux/macOS ELF）直接走
   原生 TLS，跨平台零 OS 依赖。代价：线程退出不自动释放本线程 slab 缓存（进程退出由系统
   回收；即时回收用 `releasePoolCache()`），与 tcmalloc/jemalloc 线程缓存策略一致。

### 5.1 实测（MinGW GCC 7.3 / Qt 5.12.9 / Release，多次运行取最小值）

| 负载 | 操作 | QJsonDocument (μs) | AuJson (μs) | 倍率 |
| --- | --- | --- | --- | --- |
| config ~1KB | 解析 | 1.7 | 1.7 | 1.00x |
| config ~1KB | 序列化（紧凑/pretty） | 8.6 / 9.5 | 1.5 / 2.7 | **5.73x / 3.52x** |
| config ~1KB | 遍历 | 1.4 | 0.3 | **4.67x** |
| list 5k 对象 | 解析 | 3252.0 | 2447.5 | **1.33x** |
| list 5k 对象 | 序列化（紧凑/pretty） | 17908 / 19533 | 2699 / 4261 | **6.63x / 4.58x** |
| numbers 10w | 解析 | 14712 | 5545 | **2.65x** |
| numbers 10w | 序列化（紧凑/pretty） | 31884 / 33452 | 7782 / 9950 | **4.10x / 3.36x** |
| build 10w 元素 | 建 DOM | 462506 | 23483 | **19.70x** |

- 序列化、遍历、建 DOM 全面达标（3–20x）；数字密集解析 2.65x；对象密集解析 **1.33x**；
  字符串密集约 0.86–0.91x 未达标（每字符串一次 std::string 分配 vs Qt 单缓冲，
  彻底解决需 v1.1 arena；可信输入可传 `validateUtf8=false` 免校验 pass）。
- 历史记录：池线性回收时代对象密集仅 0.61–0.95x；O(1) 回收 + 长字符串单次拷贝后 1.09x；
  池 POD 化（见 §5.6）后 1.33x。-O3 实测相对 -O2 无可见收益，维持默认。
- 过程记录：早期探针显示"每对象 3 次分配"，逐层二分定位后发现是基准负载生成代码的
  链式 `push_back` bug（tags 被写成嵌套数组），库本身每对象即设计值 2 次分配。
- `sizeof(aujson::Value)` = 40 字节。

## 6. 与 Qt 原生 JSON 对比

| 维度 | Qt `QJsonDocument` | AuJson 核心层 | AuJson Qt 层 |
| --- | --- | --- | --- |
| 依赖 | QtCore | 无（C++11 标准库） | QtCore + 核心 |
| 数字精度 | 一律 double，>2^53 整数丢精度 | int64/uint64/double 分存 | 同核心 |
| 编码 | 内部 UTF-16，UTF-8 进出要转码 | 原生 UTF-8 | 边界转码 |
| 类型 | Document/Object/Array/Value 四件套 | 单一 `Value` | 同核心 |
| 链式访问 | `obj["a"].toObject()["b"]`；`toObject()` 返回**副本**（写回陷阱） | `v["a"]["b"]` 直接读写 | 同核心 |
| 顺序 | 哈希序，不保序 | 插入序（round-trip 保形） | 同核心 |
| 错误 | offset + 消息，无行列 | code + 消息 + offset/line/column | 同核心 |
| 流式/SAX | 无 | Handler 回调，可零拷贝 | 同核心 |
| 容错 | 固定严格（注释/尾逗号/单引号全拒） | 选项可配 | 同核心 |
| 性能（公开基准定性） | 明显慢于主流快速库 | 目标 ≥3x Qt（以实测为准） | 同核心 |
| 迁移成本 | — | 新写 | 互转 + 薄封装，可逐步迁移 |

精度案例：`{"id": 9223372036854775807}` → Qt 存 double 舍入为 `9223372036854775808`；本库 int64 原样保留。

迁移示例（Qt 现状 → 新 API）：

```cpp
// 现在（Qt）：注意 toObject() 是副本，修改要写回
QJsonParseError e;
QJsonDocument doc = QJsonDocument::fromJson(data, &e);
if (e.error != QJsonParseError::NoError) { ... }
QJsonObject obj = doc.object();
int age = obj["user"].toObject()["age"].toInt();
obj["user"].toObject()["age"] = age + 1;   // 常见 bug：改的是副本
QByteArray out = QJsonDocument(obj).toJson(QJsonDocument::Indented);

// 以后（Qt 层）：直接读写，错误带行列
aujson::Value root;
QString err;
if (!aujson::qt::parse(data, root, &err)) { qWarning() << err; }
root["user"]["age"] = root["user"]["age"].asInt() + 1;
aujson::WriteOptions wo; wo.pretty = true; wo.indent = 2;
QByteArray out = aujson::qt::stringify(root, wo);
QVariant v = aujson::qt::toVariant(root);              // 旧 QVariant 代码无缝
QJsonDocument d = aujson::qt::toQJsonDocument(root);   // 对接第三方 Qt 代码
```

## 7. 构建集成

- 新模块 `AuJson/`：挂根 `CMakeLists.txt` 的 `add_subdirectory` + 根 `AuWidgetsForQt.pro` 的 `SUBDIRS`。
- 核心编译为共享库 `AuJson`（与 AuCommon 等 DLL 惯例一致），导出宏 `AUJSON_API`
  （`include/AuJson_global.h`，同 AuCommon_global.h 风格）；核心源码不 include 任何 Qt 头
  （已验证：DLL 无 Qt5Gui 符号，qmake 侧 `QT -= gui`）。
- 头文件布局（include/ 只放对外头；对外 API 合并进单头，nlohmann 同款模式；
  内部实现头在 `src/detail/`）：`#include "aujson.hpp"` / `#include "aujson_qt.hpp"`。
- Qt 层编入同一库，由 `qtHaveModule(core)` / CMake `find_package(Qt… Core)` 控制是否启用。
- C++ 基线 C++11（Qt5 构建路径可用）。
- MSVC `/utf-8`，与仓库一致。

## 8. 测试与符合性

- `tests/`：自包含测试（无框架依赖），CTest 集成。覆盖：类型/存取/路径/默认值语义、
  解析正反例（嵌套深度、非法 UTF-8、转义、surrogate pair、大整数、指数、前导零、`-0`、BOM）、
  序列化 round-trip、保序、容错选项、SAX、Qt 互转（含 QVariant 边界）。
- 符合性语料：JSONTestSuite testdata 风格的手写等价用例（离线可用）。
- `benchmark/`：同机对比 `QJsonDocument`，输出 Markdown 表，验证 §5 目标。
- 已知风险：`strtod` 受全局 locale 影响（JSON 按 C locale 语义；MSVC 可用 `_strtod_l`，v1 文档注明，v1.1 评估 fast_float 风格实现）；深度嵌套靠 `maxDepth` 防爆栈；自研边界 case 靠语料覆盖。

## 9. v1 范围 / v1.1 候选

- **v1**：Value + DOM 解析 + 序列化 + 选项 + 错误 + 点分路径 + SAX + Qt 适配层 + 测试 + 基准。
- **v1.1 候选**：SIMD 扫描、fast_float、JSON Pointer（RFC 6901）、超大对象索引化、池内存水位自动收缩。

## 10. 决策点（2026-11-06 已定）

| # | 决策 | 结果 |
| --- | --- | --- |
| D1 | 库名 / 命名空间 | `AuJson` + `aujson` |
| D2 | C++ 基线 | C++11 |
| D3 | 交付形态 | 编译型共享库（AuJson.dll，导出宏 `AUJSON_API`） |
| D4 | 解析默认严格度 | 默认严格 + 选项放宽 |
| D5 | Object 结构 | 保序 vector + 线性查找（默认执行） |
| D6 | SAX 是否入 v1 | 入（默认执行） |
| D7 | Qt 层是否含 QJsonDocument 风格薄封装 | 含（默认执行） |
| D8 | 性能目标 | 实测：序列化/遍历/建 DOM 3–20x 达标；数字密集解析 2.65x；对象密集解析 1.33x 反超 Qt、配置 1.00x；字符串密集约 0.86–0.91x 未达标（arena 列 v1.1，见 §5.1） |
