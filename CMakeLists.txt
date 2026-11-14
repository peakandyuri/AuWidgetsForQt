cmake_minimum_required(VERSION 3.16)

project(AuWidgetsForQt LANGUAGES CXX)

# 同时支持 Qt5 / Qt6：优先使用 Qt6
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)
message(STATUS "Using Qt ${QT_VERSION} (${QT_VERSION_MAJOR})")

# AuWindowKit 使用 QtGui 私有 API（<qpa/qwindowsysteminterface_p.h>），
# 对应 qmake 的 gui-private；Qt6 需显式加载私有模块包
if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS CorePrivate GuiPrivate)
endif()

# Qt6 强制 C++17（与 qmake 工程一致）；Qt5 维持 C++11
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(QT_VERSION_MAJOR EQUAL 6)
    set(CMAKE_CXX_STANDARD 17)
else()
    set(CMAKE_CXX_STANDARD 11)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 测试总开关（AuJson 模块挂 ctest）
enable_testing()

# MSVC：源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 库的 DLL / 导入库输出到各自源码树 bin/（与 qmake 的 DESTDIR 保持一致）；
# 去 lib 前缀使产物名与 qmake 一致（AuCommon.dll，而非 libAuCommon.dll）
function(au_target_output_dirs target)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
        PREFIX "")
endfunction()

# 构建完成后把最新库 DLL 拷贝到可执行文件目录，避免运行时加载到旧版 DLL
# （对应 qmake 工程的 copyAuDll）
function(au_copy_dlls target)
    if(WIN32)
        foreach(dll IN LISTS ARGN)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${dll}" "$<TARGET_FILE_DIR:${target}>"
                VERBATIM)
        endforeach()
    endif()
endfunction()

add_subdirectory(AuCommon)
add_subdirectory(AuGrid)
add_subdirectory(AuJson)
add_subdirectory(AuSelect)
add_subdirectory(AuDatePicker)
add_subdirectory(AuPlot)
add_subdirectory(AuWindowKit)
add_subdirectory(Tools/AuPortableDocEditor)
add_subdirectory(Examples/Example)
add_subdirectory(Examples/FeatureDemo)
add_subdirectory(Examples/JsonDemo)
add_subdirectory(Examples/AuWindowKitDemo)
