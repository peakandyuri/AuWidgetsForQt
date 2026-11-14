#include "test_util.hpp"
#include "aujson.hpp"

#include <string>
#include <vector>

class CollectHandler : public aujson::SaxHandler {
public:
    std::string events;
    std::vector<std::string> strings;
    void onNull() override { events += "null;"; }
    void onBool(bool b) override { events += b ? "true;" : "false;"; }
    void onInt(int64_t i) override { events += "i" + std::to_string(i) + ";"; }
    void onUInt(uint64_t u) override { events += "u" + std::to_string(u) + ";"; }
    void onDouble(double) override { events += "d;"; }
    void onString(const char* data, size_t len) override {
        events += "s;";
        strings.emplace_back(data, len);
    }
    void onKey(const char* data, size_t len) override {
        events += "k;";
        strings.emplace_back(data, len);
    }
    void onStartArray() override { events += "["; }
    void onEndArray() override { events += "]"; }
    void onStartObject() override { events += "{"; }
    void onEndObject() override { events += "}"; }
};

void testSax() {
    CollectHandler h;
    const std::string json = R"({"name":"张三","tags":["a","b"],"n":1})";
    AUJSON_CHECK(aujson::parseSax(json.data(), json.size(), h).ok);
    AUJSON_CHECK(h.events == "{k;s;k;[s;s;]k;i1;}");   // onInt 带数值
    AUJSON_CHECK(h.strings.size() == 6);               // 3 键 + 3 字符串值
    AUJSON_CHECK(h.strings[0] == "name");
    AUJSON_CHECK(h.strings[1] == "张三");
    AUJSON_CHECK(h.strings[2] == "tags");
    AUJSON_CHECK(h.strings[3] == "a");
    AUJSON_CHECK(h.strings[4] == "b");
    AUJSON_CHECK(h.strings[5] == "n");

    // 零拷贝：未转义字符串回调直接指向输入缓冲
    class ZeroCopy : public aujson::SaxHandler {
    public:
        const char* ptr = nullptr;
        size_t len = 0;
        void onString(const char* data, size_t l) override { ptr = data; len = l; }
    };
    ZeroCopy zc;
    const std::string raw = R"("hello")";
    AUJSON_CHECK(aujson::parseSax(raw.data(), raw.size(), zc).ok);
    AUJSON_CHECK(zc.ptr >= raw.data() && zc.ptr + zc.len <= raw.data() + raw.size());
    AUJSON_CHECK(std::string(zc.ptr, zc.len) == "hello");

    // 转义串走解码路径
    CollectHandler h2;
    std::string esc = "\"a\\nb\"";   // JSON: "a\nb"（字面反斜杠 + n）
    AUJSON_CHECK(aujson::parseSax(esc.data(), esc.size(), h2).ok);
    AUJSON_CHECK(h2.events == "s;");
    AUJSON_CHECK(h2.strings.size() == 1 && h2.strings[0] == "a\nb");

    // SAX 同样报错（行列一致）
    CollectHandler h3;
    std::string badsax = "{\n  \"a\": x\n}";
    aujson::ParseResult r = aujson::parseSax(badsax.data(), badsax.size(), h3);
    AUJSON_CHECK(!r.ok && r.error.line == 2 && r.error.column == 8);

    // 大文件场景：只统计数组长度，不建 DOM
    class Counter : public aujson::SaxHandler {
    public:
        size_t count = 0;
        void onStartArray() override { ++count; }
    };
    Counter c;
    std::string big = "[1,2,3,4,5]";
    AUJSON_CHECK(aujson::parseSax(big.data(), big.size(), c).ok);
    AUJSON_CHECK(c.count == 1);
}
