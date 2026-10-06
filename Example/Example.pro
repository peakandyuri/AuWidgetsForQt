QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG -= c++11
    CONFIG += c++17
}

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation for the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    demos/DemoWindowBase.cpp \
    demos/BasicDemoWindow.cpp \
    demos/SelectionCheckDemoWindow.cpp \
    demos/HeaderCheckDemoWindow.cpp \
    demos/SortFilterDemoWindow.cpp \
    demos/ColumnDemoWindow.cpp \
    demos/RowDragDemoWindow.cpp \
    demos/SpanDemoWindow.cpp \
    demos/ScrollDemoWindow.cpp \
    demos/TreeDemoWindow.cpp \
    demos/SyncDemoWindow.cpp \
    demos/StyleDemoWindow.cpp \
    demos/LoadingDemoWindow.cpp \
    demos/RowHeightDemoWindow.cpp \
    demos/DataAnimationDemoWindow.cpp \
    demos/AuSelectDemoWindow.cpp \
    demos/AuDatePickerDemoWindow.cpp \
    CustomAuGrid/ButtonRenderer.cpp \
    CustomAuGrid/HeaderButtonRenderer.cpp \
    CustomAuGrid/HeaderCheckBoxRenderer.cpp \
    CustomAuGrid/DemoValidator.cpp \
    CustomAuGrid/DemoStyle.cpp

HEADERS += \
    mainwindow.h \
    demos/DemoWindowBase.h \
    demos/BasicDemoWindow.h \
    demos/SelectionCheckDemoWindow.h \
    demos/HeaderCheckDemoWindow.h \
    demos/SortFilterDemoWindow.h \
    demos/ColumnDemoWindow.h \
    demos/RowDragDemoWindow.h \
    demos/SpanDemoWindow.h \
    demos/ScrollDemoWindow.h \
    demos/TreeDemoWindow.h \
    demos/SyncDemoWindow.h \
    demos/StyleDemoWindow.h \
    demos/LoadingDemoWindow.h \
    demos/RowHeightDemoWindow.h \
    demos/DataAnimationDemoWindow.h \
    demos/AuSelectDemoWindow.h \
    demos/AuDatePickerDemoWindow.h \
    CustomAuGrid/ButtonRenderer.h \
    CustomAuGrid/HeaderButtonRenderer.h \
    CustomAuGrid/HeaderCheckBoxRenderer.h \
    CustomAuGrid/DemoValidator.h \
    CustomAuGrid/DemoStyle.h

FORMS += \
    mainwindow.ui

INCLUDEPATH += \
    $$PWD/../AuCommon/include \
    $$PWD/../AuGrid/include \
    $$PWD/../AuSelect/include \
    $$PWD/../AuDatePicker/include \
    $$PWD/CustomAuGrid \
    $$PWD/demos
DEPENDPATH += $$PWD/../AuCommon $$PWD/../AuGrid $$PWD/../AuSelect $$PWD/../AuDatePicker

win32: LIBS += -L$$PWD/../AuCommon/bin/ -lAuCommon
else:unix: LIBS += -L$$PWD/../AuCommon/bin/ -lAuCommon
win32: LIBS += -L$$PWD/../AuGrid/bin/ -lAuGrid
else:unix: LIBS += -L$$PWD/../AuGrid/bin/ -lAuGrid
win32: LIBS += -L$$PWD/../AuSelect/bin/ -lAuSelect
else:unix: LIBS += -L$$PWD/../AuSelect/bin/ -lAuSelect
win32: LIBS += -L$$PWD/../AuDatePicker/bin/ -lAuDatePicker
else:unix: LIBS += -L$$PWD/../AuDatePicker/bin/ -lAuDatePicker

# 让目标依赖导入库：库重编后自动触发重链接 + POST_LINK 拷贝
# MinGW 导入库为 libXxx.a，MSVC 为 Xxx.lib
win32-g++: PRE_TARGETDEPS += \
    $$PWD/../AuCommon/bin/libAuCommon.a \
    $$PWD/../AuGrid/bin/libAuGrid.a \
    $$PWD/../AuSelect/bin/libAuSelect.a \
    $$PWD/../AuDatePicker/bin/libAuDatePicker.a
else:win32: PRE_TARGETDEPS += \
    $$PWD/../AuCommon/bin/AuCommon.lib \
    $$PWD/../AuGrid/bin/AuGrid.lib \
    $$PWD/../AuSelect/bin/AuSelect.lib \
    $$PWD/../AuDatePicker/bin/AuDatePicker.lib

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

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# MSVC: 源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
msvc: QMAKE_CXXFLAGS += /utf-8
