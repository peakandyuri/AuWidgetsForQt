#include "test_util.hpp"
#include "aujson.hpp"

#include <string>

void testValue() {
    // ---- 基本类型 ----
    aujson::Value v0;
    AUJSON_CHECK(v0.isNull() && v0.type() == aujson::Type::Null);

    aujson::Value vb = true;
    AUJSON_CHECK(vb.isBool() && vb.asBool());

    aujson::Value vi = 42;
    AUJSON_CHECK(vi.isInt() && vi.asInt() == 42);

    aujson::Value vu = 18446744073709551615ULL;
    AUJSON_CHECK(vu.isUInt() && vu.asUInt() == 18446744073709551615ULL);

    aujson::Value vd = 3.14;
    AUJSON_CHECK(vd.isDouble() && vd.asDouble() == 3.14);

    aujson::Value vs = "hello";
    AUJSON_CHECK(vs.isString() && vs.asString() == "hello" && vs.asCString() != nullptr);

    aujson::Value vn = nullptr;
    AUJSON_CHECK(vn.isNull());
    AUJSON_CHECK(std::string(vn.typeName()) == "null");

    // ---- asX 默认值语义（类型不符不抛异常，返回默认值）----
    AUJSON_CHECK(vs.asInt(7) == 7);
    AUJSON_CHECK(vi.asString().empty());
    AUJSON_CHECK(vi.asCString() == nullptr);
    AUJSON_CHECK(vb.asInt() == 1);
    AUJSON_CHECK(vd.asInt() == 3);            // double → int 四舍五入
    AUJSON_CHECK(vn.asBool(true) == true);
    AUJSON_CHECK(vn.asString().empty());

    // ---- 显式转换（仅精确类型成功）----
    int64_t iout = 0;
    AUJSON_CHECK(vi.toInt(&iout) && iout == 42);
    AUJSON_CHECK(!vd.toInt(&iout));
    double dout = 0;
    AUJSON_CHECK(vi.toDouble(&dout) && dout == 42.0);
    bool bout = false;
    AUJSON_CHECK(vb.toBool(&bout) && bout);
    AUJSON_CHECK(!vi.toBool(&bout));

    // ---- 数组 ----
    aujson::Value arr = aujson::Value::makeArray();
    AUJSON_CHECK(arr.isArray() && arr.empty());
    arr.push_back(1);
    arr.push_back(2);
    AUJSON_CHECK(arr.size() == 2);
    AUJSON_CHECK(arr[0].asInt() == 1 && arr[1].asInt() == 2);
    // 越界：读返回 Null、不扩容；写入落入陷阱节点（无效）
    AUJSON_CHECK(arr[99].isNull());
    AUJSON_CHECK(arr.size() == 2);
    arr[99] = 9;
    AUJSON_CHECK(arr.size() == 2 && arr[1].asInt() == 2);
    // 显式扩容
    arr.resize(4);
    AUJSON_CHECK(arr.size() == 4 && arr[2].isNull() && arr[3].isNull());
    const aujson::Value& carr = arr;
    AUJSON_CHECK(carr[1].asInt() == 2);
    AUJSON_CHECK(carr[99].isNull());
    AUJSON_CHECK(carr.contains(0) && !carr.contains(99));
    AUJSON_CHECK(arr.remove(1) && arr.size() == 3 && arr[1].isNull());
    AUJSON_CHECK(!arr.remove(99));

    // ---- 对象 ----
    aujson::Value obj = aujson::Value::makeObject();
    obj["name"] = "张三";
    obj["age"] = 30;
    obj["nested"]["flag"] = true;          // 自动逐级创建
    AUJSON_CHECK(obj.contains("name"));
    AUJSON_CHECK(!obj.contains("missing"));
    AUJSON_CHECK(obj["missing"].isNull());
    AUJSON_CHECK(obj["nested"]["flag"].asBool());
    AUJSON_CHECK(obj.find("age") != nullptr);
    AUJSON_CHECK(obj.find("nope") == nullptr);
    const aujson::Value& cobj = obj;
    AUJSON_CHECK(cobj["name"].asString() == "张三");
    AUJSON_CHECK(cobj["missing"].isNull());

    // ---- 保序迭代 ----
    aujson::Value o = aujson::Value::makeObject();
    o["z"] = 1; o["a"] = 2; o["m"] = 3;
    std::string keys;
    for (auto item : o) keys += item.key->c_str();
    AUJSON_CHECK(keys == "zam");

    int sum = 0;
    for (auto item : arr) sum += static_cast<int>(item.value->asInt());
    AUJSON_CHECK(sum == 1);                // [1, null, null]（remove 后剩 3 个元素）
    AUJSON_CHECK(arr[0].asInt() == 1 && arr[5].asInt() == 9);

    // const 迭代
    int csum = 0;
    for (auto item : o) csum += static_cast<int>(item.value->asInt());
    AUJSON_CHECK(csum == 6);
    const aujson::Value& co = o;
    int csum2 = 0;
    for (auto item : co) csum2 += static_cast<int>(item.value->asInt());
    AUJSON_CHECK(csum2 == 6);
    AUJSON_CHECK(o.size() == 3 && !o.empty());

    // ---- set 链式写 ----
    aujson::Value s = aujson::Value::makeObject();
    s.set("a", 1).set("b", 2);
    AUJSON_CHECK(s["a"].asInt() == 1 && s["b"].asInt() == 2);

    // ---- remove / clear ----
    AUJSON_CHECK(s.remove("a") && !s.contains("a") && s.size() == 1);
    AUJSON_CHECK(!s.remove("a"));
    aujson::Value a2 = aujson::Value::makeArray({10, 20, 30});
    AUJSON_CHECK(a2.remove(1) && a2.size() == 2 && a2[1].asInt() == 30);
    a2.clear();
    AUJSON_CHECK(a2.size() == 0);

    // ---- 初始化列表构造 ----
    aujson::Value o2 = aujson::Value::makeObject({{"x", 1}, {"y", true}});
    AUJSON_CHECK(o2.size() == 2 && o2["x"].asInt() == 1 && o2["y"].asBool());
    aujson::Value a3 = aujson::Value::makeArray({1, "two", 3.5});
    AUJSON_CHECK(a3.size() == 3 && a3[1].asString() == "two");

    // ---- 深拷贝 / 移动 ----
    aujson::Value cp = o;
    cp["z"] = 100;
    AUJSON_CHECK(o["z"].asInt() == 1);     // 深拷贝，互不影响
    aujson::Value mv = std::move(cp);
    AUJSON_CHECK(mv["z"].asInt() == 100);
    AUJSON_CHECK(cp.isNull());             // 被移动后置 Null

    // ---- 赋值语义 ----
    aujson::Value x = 1;
    x = "str";
    AUJSON_CHECK(x.isString() && x.asString() == "str");
    x = nullptr;
    AUJSON_CHECK(x.isNull());
    x = 2.5;
    AUJSON_CHECK(x.isDouble() && x.asDouble() == 2.5);
}
