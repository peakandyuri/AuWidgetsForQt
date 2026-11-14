#----------------------------------------------------------------
# AuPortableDocEditor - 独立 PDF 页面编辑器
#
#   功能：打开 PDF 后进行页级编辑——插入空白页（之前/之后，
#         尺寸继承相邻页）、删除页（支持多选）、替换页（从
#         另一个 PDF 选一页深拷贝换入，保留矢量与文字）。
#
#   本工程完全独立，不依赖仓库中其它组件，可单独拷出构建。
#
#   依赖：Qt Core/Gui/Widgets（Qt 5.6+，兼容 Qt 6）
#         QtPdf 模块可选（qtHaveModule(pdf) 自动探测）：
#         有则渲染真实缩略图，无则显示占位图。
#----------------------------------------------------------------

QT += core gui widgets

TEMPLATE = app
TARGET = AuPortableDocEditor

CONFIG += c++11
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG -= c++11
    CONFIG += c++17
}

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += src

SOURCES += \
    src/Main.cpp \
    src/MainWindow.cpp \
    src/PagePickerDialog.cpp \
    src/PdfPreviewRenderer.cpp \
    src/pdf/PdfObject.cpp \
    src/pdf/PdfParser.cpp \
    src/pdf/PdfWriter.cpp \
    src/pdf/PdfPageEditor.cpp

HEADERS += \
    src/MainWindow.h \
    src/PagePickerDialog.h \
    src/PdfPreviewRenderer.h \
    src/pdf/PdfObject.h \
    src/pdf/PdfParser.h \
    src/pdf/PdfWriter.h \
    src/pdf/PdfPageEditor.h

# MSVC: 源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
msvc: QMAKE_CXXFLAGS += /utf-8

# Windows: exe 文件内嵌图标（qmake 自动生成 .rc）
win32: RC_ICONS = logo.ico

# 运行时窗口 / 任务栏图标（见 Main.cpp 的 setWindowIcon）
RESOURCES += AuPortableDocEditor.qrc

# QtPdf 模块可选：仅用于页面缩略图真实渲染
qtHaveModule(pdf) {
    QT += pdf
    DEFINES += AUPDE_WITH_PREVIEW
}

# 内置 pdfium 动态库（third_party/pdfium，来源与许可见该目录 README）：
# 无 QtPdf 时用于渲染缩略图，随构建拷贝到程序目录
win32: PDFIUM_DLL = $$PWD/third_party/pdfium/pdfium.dll
win32: exists($$PDFIUM_DLL) {
    debug_and_release {
        CONFIG(release, debug|release): PDFIUM_DEST = $$OUT_PWD/release
        else: PDFIUM_DEST = $$OUT_PWD/debug
    } else {
        PDFIUM_DEST = $$OUT_PWD
    }
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$PDFIUM_DLL) $$shell_path($$PDFIUM_DEST) $$escape_expand(\n\t)
}
