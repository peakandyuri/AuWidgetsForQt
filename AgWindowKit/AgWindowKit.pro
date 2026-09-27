QT += core gui widgets

TEMPLATE = lib
DEFINES += AGWINDOWKIT_LIBRARY

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

SOURCES += \
    widget/AgWindow.cpp \
    widget/AgTitleBar.cpp \
    style/AgWindowStyle.cpp

HEADERS += \
    include/AgWindowKit_global.h \
    include/AgWindow.h \
    include/AgTitleBar.h \
    include/AgWindowStyle.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
