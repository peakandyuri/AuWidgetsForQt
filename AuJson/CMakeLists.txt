# AuJson - 高性能 JSON 库（原生 C++ 核心 + Qt 适配层）
#
# 设计文档：docs/AuJson-设计文档.md
#   - 核心层 aujson：零 Qt 依赖，C++11，编译为共享库；
#   - 适配层 aujson::qt：QByteArray / QVariant / QJsonDocument 互转，
#     找到 Qt 时自动编入同一库。

# 对外头文件（include/ 下只有公共 API）
set(AUJSON_HEADERS
    include/AuJson_global.h
    include/aujson.hpp
    include/aujson_qt.hpp
)
# 内部实现头（不入 include/）
set(AUJSON_PRIVATE_HEADERS
    src/detail/number.hpp
)
set(AUJSON_SOURCES
    src/value.cpp
    src/parse.cpp
    src/stringify.cpp
    src/path.cpp
    src/pool.cpp
)

add_library(AuJson SHARED ${AUJSON_SOURCES} ${AUJSON_HEADERS} ${AUJSON_PRIVATE_HEADERS})
target_include_directories(AuJson PUBLIC include)

# 导出宏（同 AuCommon_global.h 风格，见 include/AuJson_global.h）
set_target_properties(AuJson PROPERTIES DEFINE_SYMBOL AUJSON_LIBRARY)

# MSVC：源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
if(MSVC)
    target_compile_options(AuJson PRIVATE /utf-8)
endif()

# 与仓库其它库一致：DLL / 导入库输出到源码树 bin/，去 lib 前缀
au_target_output_dirs(AuJson)

# Qt 适配层（可选）：核心源码不依赖 Qt
if(NOT TARGET Qt${QT_VERSION_MAJOR}::Core)
    find_package(QT NAMES Qt6 Qt5 QUIET COMPONENTS Core)
    if(QT_FOUND)
        find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
    endif()
endif()
if(TARGET Qt${QT_VERSION_MAJOR}::Core)
    target_sources(AuJson PRIVATE src/qt.cpp include/aujson_qt.hpp)
    target_link_libraries(AuJson PUBLIC Qt${QT_VERSION_MAJOR}::Core)
    message(STATUS "AuJson: Qt 适配层已启用（Qt ${QT_VERSION}）")
else()
    message(STATUS "AuJson: 未找到 Qt，仅构建原生核心层")
endif()

# 测试（自包含，无测试框架依赖）
option(AUJSON_BUILD_TESTS "构建 AuJson 测试" ON)
if(AUJSON_BUILD_TESTS)
    enable_testing()
    set(AUJSON_TEST_SOURCES
        tests/test_main.cpp
        tests/test_value.cpp
        tests/test_parse.cpp
        tests/test_stringify.cpp
        tests/test_path.cpp
        tests/test_sax.cpp
        tests/test_pool.cpp
    )
    if(TARGET Qt${QT_VERSION_MAJOR}::Core)
        list(APPEND AUJSON_TEST_SOURCES tests/test_qt.cpp)
        set(AUJSON_TEST_DEFINITIONS AUJSON_WITH_QT)
    endif()
    add_executable(aujson_tests ${AUJSON_TEST_SOURCES})
    target_link_libraries(aujson_tests PRIVATE AuJson)
    target_include_directories(aujson_tests PRIVATE include)
    if(AUJSON_TEST_DEFINITIONS)
        target_compile_definitions(aujson_tests PRIVATE ${AUJSON_TEST_DEFINITIONS})
    endif()
    if(MSVC)
        target_compile_options(aujson_tests PRIVATE /utf-8)
    endif()
    add_test(NAME aujson_tests COMMAND aujson_tests)
endif()

# 基准：同机对比 QJsonDocument（需要 Qt）
option(AUJSON_BUILD_BENCHMARK "构建 AuJson 基准" OFF)
if(AUJSON_BUILD_BENCHMARK AND TARGET Qt${QT_VERSION_MAJOR}::Core)
    add_executable(aujson_benchmark benchmark/bench.cpp)
    target_link_libraries(aujson_benchmark PRIVATE AuJson Qt${QT_VERSION_MAJOR}::Core)
    target_include_directories(aujson_benchmark PRIVATE include)
    if(MSVC)
        target_compile_options(aujson_benchmark PRIVATE /utf-8)
    endif()
endif()
