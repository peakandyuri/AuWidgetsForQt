cmake_minimum_required(VERSION 3.16)

# AuPortableDocEditor - 独立 PDF 页面编辑器（插入空白页 / 删除页 / 替换页）
#
# 本工程完全独立，不依赖仓库中其它组件，可单独拷出构建：
#   cmake -B build -S .
#   cmake --build build
#
# PDF 编辑引擎为自研轻量实现（仅依赖 QtCore），详见 src/pdf/。

project(AuPortableDocEditor LANGUAGES CXX)

# 同时支持 Qt5 / Qt6：优先使用 Qt6
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)
message(STATUS "AuPortableDocEditor: Using Qt ${QT_VERSION} (${QT_VERSION_MAJOR})")

# QtPdf 模块可选：仅用于页面缩略图真实渲染，缺失时显示占位图
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Pdf QUIET)

# Qt6 强制 C++17（与 qmake 工程一致）；Qt5 维持 C++11
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(QT_VERSION_MAJOR EQUAL 6)
    set(CMAKE_CXX_STANDARD 17)
else()
    set(CMAKE_CXX_STANDARD 11)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# MSVC：源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
if(MSVC)
    add_compile_options(/utf-8)
endif()

set(AUPDE_SOURCES
    src/Main.cpp
    src/MainWindow.cpp
    src/PagePickerDialog.cpp
    src/PdfPreviewRenderer.cpp
    src/pdf/PdfObject.cpp
    src/pdf/PdfParser.cpp
    src/pdf/PdfWriter.cpp
    src/pdf/PdfPageEditor.cpp
)
set(AUPDE_HEADERS
    src/MainWindow.h
    src/PagePickerDialog.h
    src/PdfPreviewRenderer.h
    src/pdf/PdfObject.h
    src/pdf/PdfParser.h
    src/pdf/PdfWriter.h
    src/pdf/PdfPageEditor.h
)

# 运行时窗口 / 任务栏图标资源（Main.cpp 里 setWindowIcon 引用）
set(AUPDE_RESOURCES
    AuPortableDocEditor.qrc
)

add_executable(AuPortableDocEditor WIN32 ${AUPDE_SOURCES} ${AUPDE_HEADERS} ${AUPDE_RESOURCES})

# Windows: exe 文件内嵌图标
if(WIN32)
    target_sources(AuPortableDocEditor PRIVATE app.rc)
endif()

target_include_directories(AuPortableDocEditor PRIVATE src)

target_link_libraries(AuPortableDocEditor PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

if(TARGET Qt${QT_VERSION_MAJOR}::Pdf)
    target_link_libraries(AuPortableDocEditor PRIVATE Qt${QT_VERSION_MAJOR}::Pdf)
    target_compile_definitions(AuPortableDocEditor PRIVATE AUPDE_WITH_PREVIEW)
    message(STATUS "AuPortableDocEditor: QtPdf enabled（页面缩略图真实渲染）")
else()
    message(STATUS "AuPortableDocEditor: QtPdf not found（缩略图使用内置 pdfium 渲染）")
endif()

# 内置 pdfium 动态库（third_party/pdfium，来源与许可见该目录 README）：
# 无 QtPdf 时用于渲染缩略图，随构建拷贝到可执行文件目录
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/pdfium.dll")
    add_custom_command(TARGET AuPortableDocEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/pdfium.dll"
                "$<TARGET_FILE_DIR:AuPortableDocEditor>"
        VERBATIM)
    message(STATUS "AuPortableDocEditor: pdfium.dll 已内置（third_party/pdfium）")
endif()
