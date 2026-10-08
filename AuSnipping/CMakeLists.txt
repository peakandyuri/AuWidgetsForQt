cmake_minimum_required(VERSION 3.16)

# AuSnipping - 独立截图小工具（全屏 / 区域 / 窗口截图 + 标注编辑）
#
# 本工程完全独立，不依赖仓库中其它组件，可单独拷出构建：
#   cmake -B build -S .
#   cmake --build build

project(AuSnipping LANGUAGES CXX)

# 同时支持 Qt5 / Qt6：优先使用 Qt6
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)
message(STATUS "AuSnipping: Using Qt ${QT_VERSION} (${QT_VERSION_MAJOR})")

# Qt6 强制 C++17（与 qmake 工程一致）；Qt5 维持 C++11
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(QT_VERSION_MAJOR EQUAL 6)
    set(CMAKE_CXX_STANDARD 17)
else()
    set(CMAKE_CXX_STANDARD 11)
endif()

set(CMAKE_AUTOMOC ON)

# MSVC：源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
if(MSVC)
    add_compile_options(/utf-8)
endif()

set(AUSNIP_SOURCES
    src/Main.cpp
    src/Settings.cpp
    src/Notifier.cpp
    src/GlobalHotkey.cpp
    src/Capturer.cpp
    src/SnipOverlay.cpp
    src/AnnotationCanvas.cpp
    src/EditWindow.cpp
    src/Launcher.cpp
)
set(AUSNIP_HEADERS
    src/Settings.h
    src/Notifier.h
    src/NotificationHub.h
    src/GlobalHotkey.h
    src/Capturer.h
    src/SnipOverlay.h
    src/AnnotationCanvas.h
    src/EditWindow.h
    src/Launcher.h
)

add_executable(AuSnipping WIN32 ${AUSNIP_SOURCES} ${AUSNIP_HEADERS})

target_link_libraries(AuSnipping PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

# ---- Linux(X11)：窗口截图与光标捕获 ----
if(UNIX AND NOT APPLE)
    # 桌面通知（D-Bus org.freedesktop.Notifications）；找不到该模块时自动降级
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS DBus QUIET)
    if(TARGET Qt${QT_VERSION_MAJOR}::DBus)
        target_link_libraries(AuSnipping PRIVATE Qt${QT_VERSION_MAJOR}::DBus)
        target_compile_definitions(AuSnipping PRIVATE AUSNIP_WITH_DBUS)
    endif()

    # 仅构建期需要 Xlib 开发包；XFixes 运行期经 dlopen 加载
    find_package(X11 QUIET)
    if(X11_FOUND)
        target_include_directories(AuSnipping PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(AuSnipping PRIVATE ${X11_LIBRARIES})
        target_compile_definitions(AuSnipping PRIVATE HAVE_X11)
        message(STATUS "AuSnipping: X11 enabled (窗口截图 / 光标捕获)")
    else()
        message(STATUS "AuSnipping: X11 not found, 窗口截图与光标捕获将退化为 Qt 兜底实现")
    endif()
endif()
