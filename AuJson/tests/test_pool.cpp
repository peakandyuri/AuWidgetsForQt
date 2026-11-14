#include "test_util.hpp"
#include "aujson.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

void testPool() {
    // 默认开启
    AUJSON_CHECK(aujson::slabPoolEnabled());

    const std::string json = "{\"a\":[1,2,{\"b\":\"x\"}],\"c\":\"中文\"}";

    // 开启态解析 + 缓存释放后再解析
    aujson::Value v;
    AUJSON_CHECK(aujson::parse(json, v).ok);
    AUJSON_CHECK(v["a"][2]["b"].asString() == "x");
    aujson::releasePoolCache();
    AUJSON_CHECK(aujson::parse(json, v).ok && v["a"][2]["b"].asString() == "x");

    // 关闭态解析（退化为 malloc/free）再切回
    aujson::setSlabPoolEnabled(false);
    AUJSON_CHECK(!aujson::slabPoolEnabled());
    AUJSON_CHECK(aujson::parse(json, v).ok && v["a"][2]["b"].asString() == "x");
    aujson::setSlabPoolEnabled(true);
    AUJSON_CHECK(aujson::parse(json, v).ok && v["a"][2]["b"].asString() == "x");

    // 跨开关的指针归属：禁用态分配、启用态析构；反之亦然
    aujson::setSlabPoolEnabled(false);
    {
        aujson::Value v2 = aujson::Value::makeObject();
        v2["k"]["inner"] = 1;
        v2["list"].resize(3);
        v2["list"][1] = 5;
    }
    aujson::setSlabPoolEnabled(true);
    {
        aujson::Value v3;
        AUJSON_CHECK(aujson::parse(json, v3).ok);
    }
    aujson::releasePoolCache();

    // 池下的深拷贝 / 移动语义不变
    aujson::Value a = aujson::Value::makeObject();
    a["list"].push_back(1);
    a["list"].push_back(2);
    aujson::Value b = a;
    AUJSON_CHECK(b["list"][1].asInt() == 2 && a["list"][0].asInt() == 1);
    b["list"][0] = 9;
    AUJSON_CHECK(a["list"][0].asInt() == 1);   // 深拷贝互不影响
    aujson::Value c = std::move(b);
    AUJSON_CHECK(c["list"].size() == 2 && b.isNull());

    // 大缓冲（>1KB）路径与开关切换
    aujson::Value big = aujson::Value::makeArray();
    for (int i = 0; i < 100; ++i) big.push_back(i);   // 100×40B = 4KB buffer
    AUJSON_CHECK(big.size() == 100 && big[99].asInt() == 99);
    aujson::setSlabPoolEnabled(false);
    {
        aujson::Value v4;
        AUJSON_CHECK(aujson::parse(json, v4).ok);
        v4.clear();
        big.clear();
    }
    aujson::setSlabPoolEnabled(true);
    aujson::releasePoolCache();
    AUJSON_CHECK(big.size() == 0);

    // 金丝雀压测：随机分配/释放/开关切换，校验存活块内容不被复用块踩踏
    // （回归防护：slab 首块头写越界、自由链把存活块二次分配等类别的 bug）
    {
        std::srand(12345);
        struct Live { unsigned char* p; size_t n; unsigned id; };
        std::vector<Live> live;
        unsigned counter = 0;
        for (int iter = 0; iter < 50000; ++iter) {
            int op = std::rand() % 100;
            if (op < 55 || live.empty()) {
                size_t n = 1 + static_cast<size_t>(std::rand() % 1024);
                unsigned char* p = static_cast<unsigned char*>(aujson::detail::poolAlloc(n));
                AUJSON_CHECK(p != nullptr);
                unsigned id = counter++;
                std::memset(p, static_cast<int>(id & 0xFF), n);
                live.push_back({p, n, id});
            } else if (op < 90) {
                size_t idx = static_cast<size_t>(std::rand()) % live.size();
                Live& L = live[idx];
                bool intact = true;
                for (size_t i = 0; i < L.n && intact; ++i)
                    intact = L.p[i] == static_cast<unsigned char>(L.id & 0xFF);
                AUJSON_CHECK(intact);
                aujson::detail::poolDealloc(L.p, L.n);
                live[idx] = live.back();
                live.pop_back();
            } else {
                aujson::setSlabPoolEnabled(std::rand() % 2 == 0);
            }
        }
        for (const Live& L : live) aujson::detail::poolDealloc(L.p, L.n);
        aujson::setSlabPoolEnabled(true);
        aujson::releasePoolCache();
    }
}
