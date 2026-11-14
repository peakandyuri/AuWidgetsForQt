#include "test_util.hpp"
#include "aujson.hpp"

#include <cmath>
#include <string>

void testStringify() {
    aujson::Value v;

    v = nullptr;   AUJSON_CHECK(aujson::stringify(v) == "null");
    v = true;      AUJSON_CHECK(aujson::stringify(v) == "true");
    v = false;     AUJSON_CHECK(aujson::stringify(v) == "false");
    v = 42;        AUJSON_CHECK(aujson::stringify(v) == "42");
    v = -42;       AUJSON_CHECK(aujson::stringify(v) == "-42");
    v = 18446744073709551615ULL;
    AUJSON_CHECK(aujson::stringify(v) == "18446744073709551615");
    v = 3.5;       AUJSON_CHECK(aujson::stringify(v) == "3.5");
    v = 1.0;       AUJSON_CHECK(aujson::stringify(v) == "1.0");
    v = 0.1;       AUJSON_CHECK(aujson::stringify(v) == "0.1");
    v = 1e300;     AUJSON_CHECK(aujson::stringify(v) == "1e+300");
    v = "hi";      AUJSON_CHECK(aujson::stringify(v) == "\"hi\"");
    v = "a\"b\\c";
    AUJSON_CHECK(aujson::stringify(v) == "\"a\\\"b\\\\c\"");
    v = std::string("\x01\x02", 2);
    AUJSON_CHECK(aujson::stringify(v) == "\"\\u0001\\u0002\"");

    // 中文原样 UTF-8 / ensureAscii
    v = "中文";
    AUJSON_CHECK(aujson::stringify(v) == "\"中文\"");
    aujson::WriteOptions ascii; ascii.ensureAscii = true;
    AUJSON_CHECK(aujson::stringify(v, ascii) == "\"\\u4e2d\\u6587\"");

    // 数组 / 对象
    aujson::Value arr = aujson::Value::makeArray({1, "two", nullptr});
    AUJSON_CHECK(aujson::stringify(arr) == "[1,\"two\",null]");

    aujson::Value obj = aujson::Value::makeObject({{"a", 1}, {"b", arr}});
    AUJSON_CHECK(aujson::stringify(obj) == "{\"a\":1,\"b\":[1,\"two\",null]}");

    // pretty
    aujson::WriteOptions pretty; pretty.pretty = true;
    std::string p = aujson::stringify(obj, pretty);
    AUJSON_CHECK(p == "{\n  \"a\": 1,\n  \"b\": [\n    1,\n    \"two\",\n    null\n  ]\n}");

    // sortKeys（只改输出，不改存储顺序）
    aujson::WriteOptions sorted; sorted.sortKeys = true;
    aujson::Value o2 = aujson::Value::makeObject({{"b", 1}, {"a", 2}});
    AUJSON_CHECK(aujson::stringify(o2, sorted) == "{\"a\":2,\"b\":1}");
    AUJSON_CHECK(o2["b"].asInt() == 1);

    // escapeSlash
    aujson::WriteOptions sl; sl.escapeSlash = true;
    v = "a/b";
    AUJSON_CHECK(aujson::stringify(v, sl) == "\"a\\/b\"");

    // newlineAtEnd
    aujson::WriteOptions nl; nl.newlineAtEnd = true;
    v = nullptr;
    AUJSON_CHECK(aujson::stringify(v, nl) == "null\n");

    // NaN / Inf → null（不产生非法 JSON）
    v = NAN;
    AUJSON_CHECK(aujson::stringify(v) == "null");
    v = HUGE_VAL;
    AUJSON_CHECK(aujson::stringify(v) == "null");

    // round-trip：解析 → 序列化 → 字节级一致
    std::string corpus =
        "{\"i\":9223372036854775807,\"u\":18446744073709551615,\"f\":3.14159,"
        "\"s\":\"中文\\\"\\\\\\n\",\"arr\":[1,2,3],\"o\":{\"x\":null,\"y\":[true,false]}}";
    aujson::Value rt;
    AUJSON_CHECK(aujson::parse(corpus, rt).ok);
    AUJSON_CHECK(aujson::stringify(rt) == corpus);
}
