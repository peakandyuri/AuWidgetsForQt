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

INCLUDEPATH += \
    $$PWD/../AuWindowKit/include \
    $$PWD/demos

DEPENDPATH += $$PWD/../AuWindowKit

win32: LIBS += -L$$PWD/../AuWindowKit/bin/ -lAuWindowKit
else:unix: LIBS += -L$$PWD/../AuWindowKit/bin/ -lAuWindowKit

# 构建完成后把最新 DLL 拷贝到可执行文件目录，避免运行时加载到旧版 DLL
defineTest(copyAuDll) {
    dll = $$1
    win32 {
        CONFIG(release, debug|release) {
            QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$dll) $$shell_path($$OUT_PWD/release) $$escape_expand(\\n\\t)
        } else {
            QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$dll) $$shell_path($$OUT_PWD/debug) $$escape_expand(\\n\\t)
        }
    }
    # defineTest 内修改变量必须 export() 才会真正生效
    export(QMAKE_POST_LINK)
}

copyAuDll($$PWD/../AuWindowKit/bin/AuWindowKit.dll)

# MSVC: 源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
msvc: QMAKE_CXXFLAGS += /utf-8
