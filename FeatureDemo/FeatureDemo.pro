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
    pages/PageHelpers.cpp \
    pages/GridPages.cpp \
    pages/SelectPages.cpp \
    pages/DatePickerPages.cpp \
    pages/PlotPages.cpp

HEADERS += \
    MainWindow.h \
    FeaturePage.h \
    pages/PageHelpers.h \
    pages/GridPages.h \
    pages/SelectPages.h \
    pages/DatePickerPages.h \
    pages/PlotPages.h

INCLUDEPATH += \
    $$PWD/../AuCommon/include \
    $$PWD/../AuGrid/include \
    $$PWD/../AuSelect/include \
    $$PWD/../AuDatePicker/include \
    $$PWD/../AuPlot/include \
    $$PWD/pages

DEPENDPATH += $$PWD/../AuCommon $$PWD/../AuGrid $$PWD/../AuSelect $$PWD/../AuDatePicker $$PWD/../AuPlot

win32: LIBS += -L$$PWD/../AuCommon/bin/ -lAuCommon
else:unix: LIBS += -L$$PWD/../AuCommon/bin/ -lAuCommon
win32: LIBS += -L$$PWD/../AuGrid/bin/ -lAuGrid
else:unix: LIBS += -L$$PWD/../AuGrid/bin/ -lAuGrid
win32: LIBS += -L$$PWD/../AuSelect/bin/ -lAuSelect
else:unix: LIBS += -L$$PWD/../AuSelect/bin/ -lAuSelect
win32: LIBS += -L$$PWD/../AuDatePicker/bin/ -lAuDatePicker
else:unix: LIBS += -L$$PWD/../AuDatePicker/bin/ -lAuDatePicker
win32: LIBS += -L$$PWD/../AuPlot/bin/ -lAuPlot
else:unix: LIBS += -L$$PWD/../AuPlot/bin/ -lAuPlot

# 让目标依赖导入库：库重编后自动触发重链接 + POST_LINK 拷贝
# MinGW 导入库为 libXxx.a，MSVC 为 Xxx.lib
win32-g++: PRE_TARGETDEPS += \
    $$PWD/../AuCommon/bin/libAuCommon.a \
    $$PWD/../AuGrid/bin/libAuGrid.a \
    $$PWD/../AuSelect/bin/libAuSelect.a \
    $$PWD/../AuDatePicker/bin/libAuDatePicker.a \
    $$PWD/../AuPlot/bin/libAuPlot.a
else:win32: PRE_TARGETDEPS += \
    $$PWD/../AuCommon/bin/AuCommon.lib \
    $$PWD/../AuGrid/bin/AuGrid.lib \
    $$PWD/../AuSelect/bin/AuSelect.lib \
    $$PWD/../AuDatePicker/bin/AuDatePicker.lib \
    $$PWD/../AuPlot/bin/AuPlot.lib

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

copyAuDll($$PWD/../AuCommon/bin/AuCommon.dll)
copyAuDll($$PWD/../AuGrid/bin/AuGrid.dll)
copyAuDll($$PWD/../AuSelect/bin/AuSelect.dll)
copyAuDll($$PWD/../AuDatePicker/bin/AuDatePicker.dll)
copyAuDll($$PWD/../AuPlot/bin/AuPlot.dll)

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# MSVC: 源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
msvc: QMAKE_CXXFLAGS += /utf-8
