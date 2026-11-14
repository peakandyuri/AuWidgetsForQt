#ifndef AUJSON_QT_HPP
#define AUJSON_QT_HPP

// AuJson Qt 适配层：与 Qt 生态互通（QByteArray / QVariant / QJsonDocument）。
// 仅依赖 QtCore + 核心层；核心层语义与本头文件无关，改动请同步设计文档 §4。

#include "aujson.hpp"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QString>
#include <QVariant>

namespace aujson {
namespace qt {

// ---- 解析 / 序列化（QByteArray 边界）----
// 解析失败时 err 接收中文错误（含行列），例如 "期望 ':'（行 2，列 5，偏移 7）"
bool parse(const QByteArray& json, aujson::Value& out, QString* err = nullptr,
           const aujson::ParseOptions& opt = aujson::ParseOptions());
aujson::Value parse(const QByteArray& json, bool* ok = nullptr,
                    const aujson::ParseOptions& opt = aujson::ParseOptions());
QByteArray stringify(const aujson::Value& v,
                     const aujson::WriteOptions& opt = aujson::WriteOptions());

// ---- 与 QVariant 互转（list/map → Array/Object 递归）----
aujson::Value fromVariant(const QVariant& v);
QVariant      toVariant(const aujson::Value& v);

// ---- 与 QJsonValue / QJsonDocument 互转（对接第三方 Qt 代码）----
aujson::Value   fromQJsonValue(const QJsonValue& jv);
QJsonValue      toQJsonValue(const aujson::Value& v);
QJsonDocument   toQJsonDocument(const aujson::Value& v);
aujson::Value   fromQJsonDocument(const QJsonDocument& doc);

// ---- QJsonDocument 风格薄封装（迁移成本最低）----
class JsonDocument {
public:
    static JsonDocument fromJson(const QByteArray& json,
                                 const aujson::ParseOptions& opt = aujson::ParseOptions());
    QByteArray toJson(bool pretty = false) const;

    bool isNull() const;      // 解析失败（对应 QJsonDocument::isNull 的空文档语义）
    bool isArray() const;
    bool isObject() const;
    const aujson::Value& root() const;
    QString errorString() const;

private:
    aujson::Value root_;
    bool ok_ = false;
    QString err_;
};

} // namespace qt
} // namespace aujson

#endif // AUJSON_QT_HPP
