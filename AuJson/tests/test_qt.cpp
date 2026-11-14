#include "test_util.hpp"
#include "aujson_qt.hpp"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QVariant>

void testQtLayer() {
    QByteArray json = R"({"name":"张三","age":30,"tags":["a","b"],"score":3.5})";

    // ---- 解析 ----
    aujson::Value v;
    QString err;
    AUJSON_CHECK(aujson::qt::parse(json, v, &err));
    AUJSON_CHECK(v["name"].asString() == "张三");
    AUJSON_CHECK(v["age"].asInt() == 30);
    AUJSON_CHECK(v["tags"][1].asString() == "b");
    AUJSON_CHECK(v["score"].asDouble() == 3.5);

    // 错误信息含行列
    aujson::Value bad;
    AUJSON_CHECK(!aujson::qt::parse("{\n  \"a\": x\n}", bad, &err));
    AUJSON_CHECK(err.contains(QStringLiteral("行 2")));

    // ---- 序列化 ----
    aujson::WriteOptions wo; wo.pretty = true;
    QByteArray out = aujson::qt::stringify(v, wo);
    AUJSON_CHECK(out.contains("张三"));
    AUJSON_CHECK(aujson::qt::stringify(v).contains("\"name\":\"张三\""));

    // ---- QVariant 互转 ----
    QVariant qv = aujson::qt::toVariant(v);
    AUJSON_CHECK(static_cast<int>(qv.type()) == QMetaType::QVariantMap);
    QVariantMap m = qv.toMap();
    AUJSON_CHECK(m.contains("name") && m["name"].toString() == "张三");
    AUJSON_CHECK(m["age"].toInt() == 30);
    aujson::Value back = aujson::qt::fromVariant(qv);
    AUJSON_CHECK(back["name"].asString() == "张三");
    AUJSON_CHECK(back["age"].asInt() == 30);
    AUJSON_CHECK(back["tags"][0].asString() == "a");

    // QVariantList
    QVariant lv = aujson::qt::toVariant(v["tags"]);
    AUJSON_CHECK(static_cast<int>(lv.type()) == QMetaType::QVariantList);
    AUJSON_CHECK(lv.toList().size() == 2);

    // ---- QJsonDocument 互转 ----
    QJsonDocument doc = aujson::qt::toQJsonDocument(v);
    AUJSON_CHECK(doc.isObject());
    aujson::Value v2 = aujson::qt::fromQJsonDocument(doc);
    AUJSON_CHECK(v2["tags"][0].asString() == "a");
    AUJSON_CHECK(v2["age"].isInt());   // Qt 的 double 整数值还原为 Int

    QJsonDocument src = QJsonDocument::fromJson(R"({"k":[1,2]})");
    aujson::Value fromQt = aujson::qt::fromQJsonDocument(src);
    AUJSON_CHECK(fromQt["k"][1].asInt() == 2);

    // ---- JsonDocument 薄封装 ----
    aujson::qt::JsonDocument d = aujson::qt::JsonDocument::fromJson(json);
    AUJSON_CHECK(!d.isNull() && d.isObject());
    AUJSON_CHECK(d.root()["age"].asInt() == 30);
    AUJSON_CHECK(d.toJson(false).contains("张三"));
    AUJSON_CHECK(!d.toJson(true).isEmpty());

    aujson::qt::JsonDocument bad2 = aujson::qt::JsonDocument::fromJson("{oops");
    AUJSON_CHECK(bad2.isNull());
    AUJSON_CHECK(!bad2.errorString().isEmpty());

    // ---- 大整数精度（核心层保留）----
    aujson::Value big;
    AUJSON_CHECK(aujson::qt::parse("{\"id\":9223372036854775807}", big));
    AUJSON_CHECK(big["id"].asInt() == 9223372036854775807LL);
}
