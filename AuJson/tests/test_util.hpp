#ifndef AUJSON_TEST_UTIL_HPP
#define AUJSON_TEST_UTIL_HPP

// 自包含测试 harness（无框架依赖）：AUJSON_CHECK / AUJSON_CHECK_EQ 宏 + 计数。

#include <cstdio>

inline int& testChecks() { static int n = 0; return n; }
inline int& testFailures() { static int n = 0; return n; }

#define AUJSON_CHECK(cond) do { \
    ++testChecks(); \
    if (!(cond)) { ++testFailures(); \
        std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define AUJSON_CHECK_EQ(a, b) do { \
    ++testChecks(); \
    if (!((a) == (b))) { ++testFailures(); \
        std::printf("FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); } \
} while (0)

inline void testSummary() {
    std::printf("%d checks, %d failures\n", testChecks(), testFailures());
}

#endif // AUJSON_TEST_UTIL_HPP
