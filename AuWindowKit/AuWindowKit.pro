QT += core gui widgets gui-private

TEMPLATE = lib
DEFINES += AUWINDOWKIT_LIBRARY

CONFIG += c++11

DESTDIR = $$PWD/bin

INCLUDEPATH += \
    $$PWD/include \
    $$PWD/widget \
    $$PWD/style

DEFINES += QT_DEPRECATED_WARNINGS

# X11：_NET_WM_MOVERESIZE 原生拖动 / 缩放 + 混成检测（直接使用 Xlib）
unix:!macx {
    LIBS += -lX11
}

# Windows：圆角区域 GDI 函数（CreateRoundRectRgn / CombineRgn 等）
win32 {
    LIBS += -lgdi32
}

SOURCES += \
    widget/AuWindow.cpp \
    widget/AuTitleBar.cpp \
    style/AuWindowStyle.cpp

HEADERS += \
    include/AuWindowKit_global.h \
    include/AuWindow.h \
    include/AuTitleBar.h \
    include/AuWindowStyle.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
