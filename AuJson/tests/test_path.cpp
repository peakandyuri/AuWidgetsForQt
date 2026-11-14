#include "test_util.hpp"
#include "aujson.hpp"

#include <string>

void testPath() {
    aujson::Value v;
    v["user"]["name"] = "张三";
    v["user"]["age"] = 30;
    v["tags"] = aujson::Value::makeArray();
    v["tags"].push_back("c++");
    v["tags"].push_back("qt");

    AUJSON_CHECK(aujson::atPath(v, "user.name").asString() == "张三");
    AUJSON_CHECK(aujson::atPath(v, "user.age").asInt() == 30);
    AUJSON_CHECK(aujson::atPath(v, "tags.0").asString() == "c++");
    AUJSON_CHECK(aujson::atPath(v, "tags.1").asString() == "qt");

    // 常量路径：无效段返回静态 Null
    const aujson::Value& cv = v;
    AUJSON_CHECK(aujson::atPath(cv, "user.name").asString() == "张三");
    AUJSON_CHECK(aujson::atPath(cv, "user.missing").isNull());
    AUJSON_CHECK(aujson::atPath(cv, "tags.9").isNull());
    AUJSON_CHECK(aujson::atPath(cv, "user.0").isNull());   // 对象上的数字段按键查
    AUJSON_CHECK(aujson::atPath(cv, "user.name.x").isNull());

    // Value::at 委托
    AUJSON_CHECK(v.at("user.name").asString() == "张三");
    AUJSON_CHECK(cv.at("tags.0").asString() == "c++");

    // 自动创建
    aujson::Value w;
    aujson::atPath(w, "a.b.c") = 1;
    AUJSON_CHECK(w["a"]["b"]["c"].asInt() == 1);

    // 数组自动补 Null（段仅在当前值已是 Array 时按索引）
    aujson::Value w2;
    w2["arr"] = aujson::Value::makeArray();
    aujson::atPath(w2, "arr.1") = 7;
    AUJSON_CHECK(w2["arr"].size() == 2);
    AUJSON_CHECK(w2["arr"][0].isNull());
    AUJSON_CHECK(w2["arr"][1].asInt() == 7);

    // 边界：空路径 = 根；空段 / 空指针 = 静态 Null
    AUJSON_CHECK(&aujson::atPath(v, "") == &v);
    AUJSON_CHECK(aujson::atPath(v, "user..name").isNull());
    AUJSON_CHECK(aujson::atPath(v, nullptr).isNull());
    AUJSON_CHECK(aujson::atPath(cv, "tags.x").isNull());   // 数组上的非数字段
}
