#include "aujson_qt.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>
#include <QString>

#include <cmath>
#include <string>

namespace aujson {
namespace qt {

// ---------------------------------------------------------------- 解析 / 序列化

bool parse(const QByteArray& json, aujson::Value& out, QString* err,
           const aujson::ParseOptions& opt) {
    const char* data = json.constData();
    aujson::ParseResult r = ::aujson::parse(data ? data : "",
                                            static_cast<size_t>(json.size()), out, opt);
    if (!r.ok && err) {
        *err = QString::fromLatin1(r.error.message.c_str(),
                                   static_cast<int>(r.error.message.size())) +
               QStringLiteral("（行 %1，列 %2，偏移 %3）")
                   .arg(static_cast<qlonglong>(r.error.line))
                   .arg(static_cast<qlonglong>(r.error.column))
                   .arg(static_cast<qlonglong>(r.error.offset));
    }
    return r.ok;
}

aujson::Value parse(const QByteArray& json, bool* ok, const aujson::ParseOptions& opt) {
    aujson::Value v;
    bool r = ::aujson::qt::parse(json, v, nullptr, opt);
    if (ok) *ok = r;
    return v;
}

QByteArray stringify(const aujson::Value& v, const aujson::WriteOptions& opt) {
    std::string s = ::aujson::stringify(v, opt);
    return QByteArray(s.data(), static_cast<int>(s.size()));
}

// ---------------------------------------------------------------- QVariant 互转

aujson::Value fromVariant(const QVariant& var) {
    switch (static_cast<QMetaType::Type>(var.type())) {
    case QMetaType::Bool:      return aujson::Value(var.toBool());
    case QMetaType::Int:       return aujson::Value(static_cast<int64_t>(var.toInt()));
    case QMetaType::LongLong:  return aujson::Value(var.toLongLong());
    case QMetaType::UInt:      return aujson::Value(static_cast<uint64_t>(var.toUInt()));
    case QMetaType::ULongLong: return aujson::Value(var.toULongLong());
    case QMetaType::Double:    return aujson::Value(var.toDouble());
    case QMetaType::QString:   return aujson::Value(var.toString().toUtf8().constData());
    case QMetaType::QByteArray: {
        const QByteArray ba = var.toByteArray();
        return aujson::Value(std::string(ba.constData(), static_cast<size_t>(ba.size())));
    }
    case QMetaType::QStringList: {
        aujson::Value a = aujson::Value::makeArray();
        for (const QString& s : var.toStringList()) a.push_back(s.toUtf8().constData());
        return a;
    }
    case QMetaType::QVariantList: {
        aujson::Value a = aujson::Value::makeArray();
        for (const QVariant& e : var.toList()) a.push_back(fromVariant(e));
        return a;
    }
    case QMetaType::QVariantMap: {
        aujson::Value o = aujson::Value::makeObject();
        const QVariantMap map = var.toMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it)
            o[it.key().toUtf8().constData()] = fromVariant(it.value());
        return o;
    }
    default:                     return aujson::Value();  // 不支持的类型 → Null
    }
}

QVariant toVariant(const aujson::Value& v) {
    switch (v.type()) {
    case aujson::Type::Null:   return QVariant();
    case aujson::Type::Bool:   return QVariant(v.asBool());
    case aujson::Type::Int:    return QVariant(static_cast<qlonglong>(v.asInt()));
    case aujson::Type::UInt:   return QVariant(static_cast<qulonglong>(v.asUInt()));
    case aujson::Type::Double: return QVariant(v.asDouble());
    case aujson::Type::String: {
        const std::string& s = v.asString();
        return QVariant(QString::fromUtf8(s.data(), static_cast<int>(s.size())));
    }
    case aujson::Type::Array: {
        QVariantList list;
        for (auto item : v) list.append(toVariant(*item.value));
        return list;
    }
    case aujson::Type::Object: {
        QVariantMap map;
        for (auto item : v) {
            const std::string& key = *item.key;
            map.insert(QString::fromUtf8(key.data(), static_cast<int>(key.size())),
                       toVariant(*item.value));
        }
        return map;
    }
    }
    return QVariant();
}

// ---------------------------------------------------------------- QJsonValue 互转

aujson::Value fromQJsonValue(const QJsonValue& jv) {
    switch (jv.type()) {
    case QJsonValue::Null:   return aujson::Value();
    case QJsonValue::Bool:   return aujson::Value(jv.toBool());
    case QJsonValue::Double: {
        // Qt 数字一律按 double 存储；整数值还原为 Int（在 int64 范围内时）
        double d = jv.toDouble();
        if (d >= -9223372036854775808.0 && d <= 9223372036854775807.0 &&
            d == std::floor(d))
            return aujson::Value(static_cast<int64_t>(d));
        return aujson::Value(d);
    }
    case QJsonValue::String: {
        const QByteArray u = jv.toString().toUtf8();
        return aujson::Value(std::string(u.constData(), static_cast<size_t>(u.size())));
    }
    case QJsonValue::Array: {
        aujson::Value a = aujson::Value::makeArray();
        for (const QJsonValue& e : jv.toArray()) a.push_back(fromQJsonValue(e));
        return a;
    }
    case QJsonValue::Object: {
        const QJsonObject obj = jv.toObject();
        aujson::Value o = aujson::Value::makeObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
            o[it.key().toUtf8().constData()] = fromQJsonValue(it.value());
        return o;
    }
    case QJsonValue::Undefined:
    default:                   return aujson::Value();
    }
}

QJsonValue toQJsonValue(const aujson::Value& v) {
    switch (v.type()) {
    case aujson::Type::Null:   return QJsonValue(QJsonValue::Null);
    case aujson::Type::Bool:   return QJsonValue(v.asBool());
    case aujson::Type::Int:    return QJsonValue(static_cast<double>(v.asInt()));  // 大整数丢精度（Qt 限制）
    case aujson::Type::UInt:   return QJsonValue(static_cast<double>(v.asUInt()));
    case aujson::Type::Double: return QJsonValue(v.asDouble());
    case aujson::Type::String: {
        const std::string& s = v.asString();
        return QJsonValue(QString::fromUtf8(s.data(), static_cast<int>(s.size())));
    }
    case aujson::Type::Array: {
        QJsonArray arr;
        for (auto item : v) arr.append(toQJsonValue(*item.value));
        return arr;
    }
    case aujson::Type::Object: {
        QJsonObject obj;
        for (auto item : v) {
            const std::string& key = *item.key;
            obj.insert(QString::fromUtf8(key.data(), static_cast<int>(key.size())),
                       toQJsonValue(*item.value));
        }
        return obj;
    }
    }
    return QJsonValue(QJsonValue::Null);
}

QJsonDocument toQJsonDocument(const aujson::Value& v) {
    const QJsonValue jv = toQJsonValue(v);
    QJsonDocument doc;
    if (jv.isArray())       doc.setArray(jv.toArray());
    else if (jv.isObject()) doc.setObject(jv.toObject());
    // 标量根无法装入 QJsonDocument（Qt 限制），返回空文档
    return doc;
}

aujson::Value fromQJsonDocument(const QJsonDocument& doc) {
    if (doc.isArray())  return fromQJsonValue(QJsonValue(doc.array()));
    if (doc.isObject()) return fromQJsonValue(QJsonValue(doc.object()));
    return aujson::Value();
}

// ---------------------------------------------------------------- JsonDocument 薄封装

JsonDocument JsonDocument::fromJson(const QByteArray& json, const aujson::ParseOptions& opt) {
    JsonDocument d;
    d.ok_ = ::aujson::qt::parse(json, d.root_, &d.err_, opt);
    return d;
}

QByteArray JsonDocument::toJson(bool pretty) const {
    aujson::WriteOptions wo;
    wo.pretty = pretty;
    return ::aujson::qt::stringify(root_, wo);
}

bool JsonDocument::isNull() const { return !ok_; }
bool JsonDocument::isArray() const { return ok_ && root_.isArray(); }
bool JsonDocument::isObject() const { return ok_ && root_.isObject(); }
const aujson::Value& JsonDocument::root() const { return root_; }
QString JsonDocument::errorString() const { return err_; }

} // namespace qt
} // namespace aujson
