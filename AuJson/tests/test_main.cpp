#include "test_util.hpp"

void testValue();
void testParse();
void testStringify();
void testPath();
void testSax();
void testPool();
#ifdef AUJSON_WITH_QT
void testQtLayer();
#endif

int main() {
    testValue();
    testParse();
    testStringify();
    testPath();
    testSax();
    testPool();
#ifdef AUJSON_WITH_QT
    testQtLayer();
#endif
    testSummary();
    return testFailures() ? 1 : 0;
}
