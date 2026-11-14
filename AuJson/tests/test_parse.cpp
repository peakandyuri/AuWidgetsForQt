#include "test_util.hpp"
#include "aujson.hpp"

#include <string>

static bool parseOk(const std::string& s, aujson::Value& out) {
    return aujson::parse(s, out).ok;
}

void testParse() {
    aujson::Value v;

    // ---- 合法文档 ----
    AUJSON_CHECK(parseOk("{}", v) && v.isObject() && v.empty());
    AUJSON_CHECK(parseOk("[]", v) && v.isArray() && v.empty());
    AUJSON_CHECK(parseOk("null", v) && v.isNull());
    AUJSON_CHECK(parseOk(" true ", v) && v.asBool() == true);
    AUJSON_CHECK(parseOk("false", v) && v.isBool() && !v.asBool());
    AUJSON_CHECK(parseOk("42", v) && v.isInt() && v.asInt() == 42);
    AUJSON_CHECK(parseOk("-42", v) && v.asInt() == -42);
    AUJSON_CHECK(parseOk("18446744073709551615", v) && v.isUInt());
    AUJSON_CHECK(parseOk("-9223372036854775808", v) && v.isInt() &&
                 v.asInt() == (-9223372036854775807LL - 1));
    AUJSON_CHECK(parseOk("3.14", v) && v.isDouble());
    AUJSON_CHECK(parseOk("1e3", v) && v.isDouble() && v.asDouble() == 1000.0);
    AUJSON_CHECK(parseOk("-0.5", v) && v.asDouble() == -0.5);
    AUJSON_CHECK(parseOk("\"\\u4e2d\\u6587\"", v) && v.asString() == "中文");
    AUJSON_CHECK(parseOk("\"\\ud83d\\ude00\"", v) &&
                 v.asString() == std::string("\xF0\x9F\x98\x80", 4));   // 😀

    // 原始 4 字节 UTF-8（天文平面字符，如 emoji）必须按合法 UTF-8 接受；
    // 回归：utf8Valid 曾把 4 字节序列的前导字节判错（(c>>3)==0xF 应为 (c>>4)==0xF），
    // 导致含 emoji 的 JSON 一律报"非法 UTF-8"。
    AUJSON_CHECK(parseOk("\"\xF0\x9F\x98\x80\"", v) &&
                 v.asString() == std::string("\xF0\x9F\x98\x80", 4));
    AUJSON_CHECK(parseOk("\"a\xE4\xB8\xAD\xF0\x9F\x98\x80" "b\"", v) &&
                 v.asString() == std::string("a\xE4\xB8\xAD\xF0\x9F\x98\x80" "b", 9));
    // 截断的 4 字节序列 / 坏续字节 / 代理区编码仍拒绝
    AUJSON_CHECK(!parseOk("\"\xF0\x9F\x98\"", v));          // 截断
    AUJSON_CHECK(!parseOk("\"\xF0\x9F\x98\x00\"", v));      // 续字节非法
    AUJSON_CHECK(!parseOk("\"\xED\xA0\x80\"", v));          // 代理区 U+D800
    AUJSON_CHECK(!parseOk("\"\xF5\x80\x80\x80\"", v));      // 超 U+10FFFF
    // 往返：stringify 输出再解析必须成功且等价
    {
        aujson::Value emoji;
        AUJSON_CHECK(parseOk("\"\xF0\x9F\x98\x80\"", emoji));
        const std::string out = aujson::stringify(emoji);
        aujson::Value back;
        AUJSON_CHECK(aujson::parse(out, back).ok && back.asString() == emoji.asString());
    }
    AUJSON_CHECK(parseOk("\"a\\/b\"", v) && v.asString() == "a/b");
    AUJSON_CHECK(parseOk("\"\\\\\\b\\f\\n\\r\\t\"", v) &&
                 v.asString() == std::string("\\\b\f\n\r\t", 6));
    AUJSON_CHECK(parseOk("{\"b\":[1,{\"c\":2}],\"a\":{\"d\":null}}", v));
    AUJSON_CHECK(v["b"][1]["c"].asInt() == 2);
    AUJSON_CHECK(v["a"]["d"].isNull());
    AUJSON_CHECK(v["b"].size() == 2);

    // 大整数精度（Qt 一律 double 会丢）
    AUJSON_CHECK(parseOk("{\"id\":9223372036854775807}", v));
    AUJSON_CHECK(v["id"].isInt() && v["id"].asInt() == 9223372036854775807LL);

    // 溢出降级 double
    AUJSON_CHECK(parseOk("99999999999999999999", v) && v.isDouble());

    // 重复键：后者覆盖，位置取首次
    AUJSON_CHECK(parseOk("{\"a\":1,\"b\":2,\"a\":3}", v));
    AUJSON_CHECK(v.size() == 2 && v["a"].asInt() == 3);
    std::string order;
    for (auto it : v) order += it.key->c_str();
    AUJSON_CHECK(order == "ab");

    // BOM
    AUJSON_CHECK(parseOk("\xEF\xBB\xBF{\"a\":1}", v) && v["a"].asInt() == 1);

    // 前导零 / -0
    AUJSON_CHECK(parseOk("0", v) && v.asInt() == 0);
    AUJSON_CHECK(parseOk("-0", v) && v.asInt() == 0);

    // ---- 非法文档 ----
    const char* bads[] = {
        "", " ", "x", "{", "}", "[", "]", "{]", "{\"a\"}", "{\"a\":}", "{\"a\":1",
        "[1,]", "{\"a\":1,}", "01", "1.", ".5", "1e", "+1", "truefalse", "tru", "nul",
        "{\"a\":1} extra", "{\"a\" 1}", "[1 2]", "\"abc", "\"a\\q\"", "\"\\u12\"",
        "\"\\uD800\"", "\"\\uDC00\"", "\"\x01\"", "\"\xFF\"",
        "{\"a\":1 \"b\":2}", "NaN", "Infinity",
    };
    for (const char* s : bads) {
        aujson::Value bad;
        aujson::ParseResult r = aujson::parse(s, bad);
        AUJSON_CHECK(!r.ok);
    }

    // 失败时 out 不被修改
    aujson::Value untouched = aujson::Value::makeObject();
    aujson::parse("bad", untouched);
    AUJSON_CHECK(untouched.isObject());

    // ---- 错误信息带行列 ----
    std::string multi = "{\n  \"a\": 1,\n  \"b\": x\n}";
    aujson::ParseResult r = aujson::parse(multi, v);
    AUJSON_CHECK(!r.ok);
    AUJSON_CHECK(r.error.line == 3);
    AUJSON_CHECK(r.error.column == 8);
    AUJSON_CHECK(!r.error.message.empty());

    // ---- 嵌套深度 ----
    aujson::Value deep;
    AUJSON_CHECK(parseOk(std::string(100, '[') + "1" + std::string(100, ']'), deep));
    const aujson::Value* p = &deep;
    int depth = 0;
    while (p && p->isArray() && p->size() == 1) { p = &(*p)[0]; ++depth; }
    AUJSON_CHECK(depth == 100 && p && p->asInt() == 1);
    std::string tooDeep = std::string(300, '[') + "1" + std::string(300, ']');
    AUJSON_CHECK(!aujson::parse(tooDeep, deep).ok);   // 默认 maxDepth=256

    // ---- 容错选项 ----
    aujson::ParseOptions opt;
    opt.allowComments = true;
    AUJSON_CHECK(aujson::parse("1 // 行注释\n", v, opt).ok && v.asInt() == 1);
    AUJSON_CHECK(aujson::parse("/* 块 */ 2", v, opt).ok && v.asInt() == 2);
    opt.allowTrailingCommas = true;
    AUJSON_CHECK(aujson::parse("[1,2,]", v, opt).ok && v.size() == 2);
    AUJSON_CHECK(aujson::parse("{\"a\":1,}", v, opt).ok);
    opt.allowSingleQuotes = true;
    AUJSON_CHECK(aujson::parse("{'a':1}", v, opt).ok && v["a"].asInt() == 1);
    opt.allowUnquotedKeys = true;
    AUJSON_CHECK(aujson::parse("{a:1, b:'x'}", v, opt).ok && v.size() == 2);
    opt.allowNaNInf = true;
    AUJSON_CHECK(aujson::parse("NaN", v, opt).ok);
    AUJSON_CHECK(aujson::parse("Infinity", v, opt).ok);
    AUJSON_CHECK(aujson::parse("-Infinity", v, opt).ok);
    opt.validateUtf8 = false;
    AUJSON_CHECK(aujson::parse("\"\xFF\"", v, opt).ok);
}
