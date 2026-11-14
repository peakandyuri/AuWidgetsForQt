# AuJson

高性能 + 易用的 JSON 库，用于替换 Qt 原生 `QJsonDocument` 系 API。两层结构：

- **核心层 `aujson`**：纯原生 C++11，零 Qt 依赖（链接层面不依赖 Qt5Gui/QtCore，
  仅 QtCore 用于适配层）；单一 `Value` 类型，int64/uint64/double 分存保精度，Object 保序。
- **Qt 适配层 `aujson::qt`**：`QByteArray` / `QVariant` / `QJsonDocument` 互转 +
  `JsonDocument` 薄封装，现有 Qt 代码可逐步迁移。

设计文档与 Qt 对比见 [docs/AuJson-设计文档.md](../docs/AuJson-设计文档.md)；
使用手册见 [docs/AuJson-帮助文档.md](../docs/AuJson-帮助文档.md)。

## 快速上手

```cpp
#include "aujson.hpp"                 // 核心层（非 Qt 项目直接用，一层路径）

aujson::Value v;
v["user"]["name"] = "张三";           // operator[] 缺键自动建 object
v["user"]["age"]  = 30;
v["tags"].push_back("c++");

aujson::ParseResult r = aujson::parse(data, len, v);
if (!r.ok) qWarning("%s（行 %zu，列 %zu）", r.error.message.c_str(),
                    r.error.line, r.error.column);
std::string out = aujson::stringify(v);          // 紧凑
aujson::WriteOptions wo; wo.pretty = true;       // 格式化 / ensureAscii / sortKeys ...
int64_t age = v["user"]["age"].asInt(0);         // 类型不符返回默认值，不抛异常
const aujson::Value& n = v.at("user.name");      // 点分路径
```

Qt 工程里用适配层（互转 + QJsonDocument 风格封装）：

```cpp
#include "aujson_qt.hpp"

aujson::Value root;
QString err;
if (!aujson::qt::parse(jsonBytes, root, &err)) { /* err 含行列 */ }
root["user"]["age"] = root["user"]["age"].asInt() + 1;   // 直接写，无 toObject() 副本陷阱
QByteArray out = aujson::qt::stringify(root);
QVariant qv = aujson::qt::toVariant(root);               // 旧 QVariant 代码无缝
QJsonDocument doc = aujson::qt::toQJsonDocument(root);   // 对接第三方 Qt 代码

// 薄封装（QJsonDocument 风格，迁移成本最低）
aujson::qt::JsonDocument d = aujson::qt::JsonDocument::fromJson(jsonBytes);
if (!d.isNull() && d.isObject()) { ... }
```

## 构建 / 测试 / 基准

- 已挂根构建：CMake `add_subdirectory(AuJson)` + qmake `SUBDIRS += AuJson`（与仓库其它模块一致）。
- 测试：CMake 目标 `aujson_tests`（232 项检查，`ctest` 运行）；自包含 harness，无框架依赖。
- 基准：`cmake -B build -DAUJSON_BUILD_BENCHMARK=ON` 后构建 `aujson_benchmark`，
  同机对比 `QJsonDocument`，输出 Markdown 表（实测数据见设计文档 §5）。

## 目录

```
AuJson/
  include/AuJson_global.h      导出宏（同 AuCommon 风格）
  include/aujson.hpp           核心层总入口（对外唯一核心头，含全部公共 API）
  include/aujson_qt.hpp        Qt 适配层入口
  src/detail/number.hpp        内部实现头（不入 include/）
  src/                         实现
  tests/ benchmark/            测试与基准
```
