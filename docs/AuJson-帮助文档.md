# AuJson 帮助文档

`AuJson` 是高性能 + 易用的 JSON 库，用于替换 Qt 原生 `QJsonDocument` 系 API。核心层
`aujson` 纯 C++11 零 Qt 依赖（非 Qt 项目可用），Qt 适配层 `aujson::qt` 提供
`QByteArray` / `QVariant` / `QJsonDocument` 互转。对外头文件仅 `include/aujson.hpp` 与
`include/aujson_qt.hpp`（内部实现头在 `src/detail/`，不对外）。设计与实测对比见
[AuJson-设计文档.md](AuJson-设计文档.md)。

## 1. 快速上手

```cpp
#include "aujson.hpp"        // 核心层；Qt 工程 include "aujson_qt.hpp"

aujson::Value v;
v["user"]["name"] = "张三";            // 缺键自动建 object，直接读写（无副本陷阱）
v["user"]["age"]  = 30;
v["tags"].push_back("c++");

aujson::ParseResult r = aujson::parse(data, len, v);   // 或 aujson::parse(std::string, v)
if (!r.ok)
    qWarning("%s（行 %zu，列 %zu）", r.error.message.c_str(),
             r.error.line, r.error.column);            // 错误带行列

std::string out = aujson::stringify(v);                // 紧凑输出
aujson::WriteOptions wo; wo.pretty = true; wo.indent = 2;
std::string pretty = aujson::stringify(v, wo);
```

Qt 工程（互转 + 薄封装）：

```cpp
#include "aujson_qt.hpp"

aujson::Value root;
QString err;
if (!aujson::qt::parse(jsonBytes, root, &err)) { qWarning() << err; }
root["user"]["age"] = root["user"]["age"].asInt() + 1;
QByteArray out    = aujson::qt::stringify(root);
QVariant    qv    = aujson::qt::toVariant(root);             // 旧 QVariant 代码无缝
QJsonDocument doc = aujson::qt::toQJsonDocument(root);       // 对接第三方 Qt 代码

aujson::qt::JsonDocument d = aujson::qt::JsonDocument::fromJson(jsonBytes); // QJsonDocument 风格
if (!d.isNull() && d.isObject()) { ... d.root()["k"] ... }
```

## 2. Value 类型与取值

类型：`Null / Bool / Int(int64) / UInt(uint64) / Double / String(UTF-8) / Array / Object`。
数字分类型存储——`9223372036854775807` 等大整数原样保留（Qt 一律 double，>2^53 丢精度）。

| API | 说明 |
| --- | --- |
| `type() / isXxx()` | 类型判别（`isNumber` 含三种数字） |
| `asBool/asInt/asUInt/asDouble(def)` | 安全取值，类型不符返回默认值；Double→Int 四舍五入 |
| `asString() / asCString()` | 非 String 返回静态空串 / nullptr |
| `toBool/toInt/toUInt/toDouble(&out)` | 仅精确类型成功，返回 bool |
| `size() / empty() / clear()` | 容器元素数；String 为字节数 |

无异常设计：任何 API 不 throw；const 访问越界/缺键返回静态 Null 单例（读安全）。

## 3. 容器访问与修改

```cpp
v.contains("k");  v.find("k");            // find 返回 Value*，不存在为 nullptr
v["arr"][0];  v["arr"][99];               // 数组下标越界：读安全（返回陷阱 Null，写入无效，不扩容）
v["arr"].push_back(1);  v["arr"].resize(4);   // 追加 / 显式扩容（补 Null）
v.set("a", 1)("b", 2);                    // 链式写
v.remove("k");  v.remove(0);              // 删键 / 删下标
v.at("user.tags.0");                      // 点分路径；缺键自动建 object、数组越界自动补 Null
                                          // （键本身含 "." 时请用 find()）

for (auto item : obj)  { item.key->c_str(); item.value->asInt(); }   // Object 保序迭代
for (auto item : arr)  { item.value->asString(); }                   // Array：item.key 为 nullptr
const aujson::Value& c = v;               // const 迭代同构（ConstItem）
```

构造辅助：`Value::makeArray({1,"two",3.5})`、`Value::makeObject({{"a",1},{"b",true}})`、
字面量直接赋值（`v = 42; v = 3.5; v = "s"; v = true; v = nullptr;`）。

## 4. 解析选项与错误

`ParseOptions`（默认全关 = 严格 JSON，与 Qt 严格度一致）：

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `allowComments` | false | `//` 与 `/* */` |
| `allowTrailingCommas` | false | 数组/对象尾逗号 |
| `allowSingleQuotes` | false | 单引号字符串（含键） |
| `allowUnquotedKeys` | false | 无引号键 |
| `allowNaNInf` | false | `NaN` / `Infinity` 字面量（非标准） |
| `validateUtf8` | true | 字符串内 UTF-8 校验 |
| `maxDepth` | 256 | 嵌套深度上限（防爆栈） |

错误：`ParseError{ code, message(中文), offset, line, column }`（Qt 只有 offset + 消息，无行列）。
细节：自动跳过 UTF-8 BOM；重复键后者覆盖、位置取首次；整数溢出自动降级 double。

## 5. 序列化选项

`WriteOptions`：`pretty`（缩进）、`indent`（默认 2）、`ensureAscii`（非 ASCII 转 \uXXXX）、
`escapeSlash`、`sortKeys`（仅输出排序，不改存储顺序）、`newlineAtEnd`。
浮点输出最短 round-trip（`0.1` 输出 `0.1` 而非 `0.10000000000000001`；`1.0` 输出 `1.0` 保持
Double 类型）；NaN/Inf 输出 `null`，不产生非法 JSON。

## 6. SAX 流式解析

```cpp
class MyHandler : public aujson::SaxHandler {
    void onInt(int64_t v) override { ... }
    void onString(const char* data, size_t len) override { ... }  // 指向输入缓冲（零拷贝），回调后失效
    // onNull/onBool/onUInt/onDouble/onKey/onStartArray/onEndArray/onStartObject/onEndObject
};
aujson::parseSax(data, len, handler);    // 大文件 / 只取局部，不建 DOM
```

## 7. Qt 适配层速查（aujson_qt.hpp）

| API | 说明 |
| --- | --- |
| `parse(QByteArray, Value&, QString* err)` | 失败时 err 含行列，如 `期望 ':'（行 2，列 5，偏移 7）` |
| `stringify(Value, WriteOptions)` | → QByteArray |
| `fromVariant / toVariant` | QVariantMap/List 递归互转（uint64 > 2^63-1 降级 double） |
| `fromQJsonValue / toQJsonValue` | Qt 整数值还原为 Int；转出大整数丢精度（Qt 限制） |
| `toQJsonDocument / fromQJsonDocument` | 标量根返回空文档（Qt 限制） |
| `JsonDocument::fromJson / toJson / root / errorString` | QJsonDocument 风格薄封装 |

## 8. 内存池开关（低内存设备）

容器内存默认走**线程本地 slab 池**（分桶复用 + 连续切割，解析/析构免逐节点 malloc），
`Value` 语义与大小不受影响：

```cpp
aujson::setSlabPoolEnabled(false);   // 关闭：退化为直接 malloc/free（低内存设备）
aujson::setSlabPoolEnabled(true);    // 重新开启
aujson::releasePoolCache();          // 手动归还当前线程缓存的内存块与 slab
```

- 可任意时刻切换（分配/回收按指针归属正确路由）；有存活 JSON 树时 `releasePoolCache()`
  保守不释放（slab 内槽位不能单独归还），树全部析构后调用即整体回收。
- 池持有已释放块待复用（高水位内存），这就是低内存设备关闭它的原因。

## 9. 构建 / 测试 / 基准

- 已挂根构建（CMake `add_subdirectory(AuJson)` + qmake `SUBDIRS`），链接 `AuJson` 即用。
- 测试：`aujson_tests`（246 项检查，ctest 运行），覆盖类型/存取/路径/SAX/解析正反例/
  UTF-8 与代理对/大整数/round-trip/内存池开关/Qt 互转。
- 基准：`cmake -B build -DAUJSON_BUILD_BENCHMARK=ON` 构建 `aujson_benchmark`，
  同机对比 QJsonDocument（实测数据见设计文档 §5.1）。
