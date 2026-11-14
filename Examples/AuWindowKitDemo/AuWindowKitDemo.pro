QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG -= c++11
    CONFIG += c++17
}

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    demos/DialogWindows.cpp \
    demos/MyWin.cpp

HEADERS += \
    MainWindow.h \
    demos/DialogWindows.h \
    demos/MyWin.h

FORMS += \
    demos/MyWinContent.ui

RESOURCES += \
    AuWindowKitDemo.qrc

# 仓库根目录（本工程位于 Examples/AuWindowKitDemo，需回溯两层）
REPO_ROOT = $$PWD/../..

INCLUDEPATH += \
    $$REPO_ROOT/AuWindowKit/include \
    $$PWD/demos

DEPENDPATH += $$REPO_ROOT/AuWindowKit

LIBS += -L$$REPO_ROOT/AuWindowKit/bin/ -lAuWindowKit


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# MSVC: 源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
msvc: QMAKE_CXXFLAGS += /utf-8
