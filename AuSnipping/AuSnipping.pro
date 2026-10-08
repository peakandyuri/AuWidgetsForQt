#----------------------------------------------------------------
# AuSnipping - 独立截图小工具
#
#   功能：全屏截图 / 任意区域截图 / 窗口截图，延迟截图，
#         鼠标光标捕获，剪贴板，立即保存，标注画板（画笔 /
#         文字 / 矩形 / 椭圆 / 直线，撤销与恢复）。
#
#   本工程完全独立，不依赖仓库中其它组件，可单独拷出构建。
#
#   依赖：Qt Core/Gui/Widgets（Qt 5.10+，兼容 Qt 6）
#         Linux(X11) 下经 pkg-config 检测 Xlib，用于“窗口截图”
#         与“光标捕获”；不可用时自动降级，仍可编译运行。
#----------------------------------------------------------------

QT += core gui widgets

unix:!macx: QT += dbus

TEMPLATE = app
TARGET = AuSnipping

CONFIG += c++11
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG -= c++11
    CONFIG += c++17
}

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    src/Main.cpp \
    src/Settings.cpp \
    src/Notifier.cpp \
    src/GlobalHotkey.cpp \
    src/Capturer.cpp \
    src/SnipOverlay.cpp \
    src/AnnotationCanvas.cpp \
    src/EditWindow.cpp \
    src/Launcher.cpp

HEADERS += \
    src/Settings.h \
    src/Notifier.h \
    src/NotificationHub.h \
    src/GlobalHotkey.h \
    src/Capturer.h \
    src/SnipOverlay.h \
    src/AnnotationCanvas.h \
    src/EditWindow.h \
    src/Launcher.h

# MSVC: 源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
msvc: QMAKE_CXXFLAGS += /utf-8

# Windows: exe 文件内嵌图标（qmake 自动生成 .rc）
win32: RC_ICONS = logo.ico

# 运行时窗口 / 任务栏图标（见 Main.cpp 的 setWindowIcon）
RESOURCES += AuSnipping.qrc

# Linux(X11)：接入 Xlib 实现“窗口截图”与“光标捕获”。
# XFixes 运行期经 dlopen 加载，构建时无需 libxfixes-dev。
unix:!macx {
    packagesExist(x11) {
        CONFIG += link_pkgconfig
        PKGCONFIG += x11
        DEFINES += HAVE_X11
    }
}

win32: LIBS += -luser32 -lgdi32
