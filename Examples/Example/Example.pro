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

# 仓库根目录（本工程位于 Examples/Example，需回溯两层）
REPO_ROOT = $$PWD/../..

INCLUDEPATH += \
    $$REPO_ROOT/AuCommon/include \
    $$REPO_ROOT/AuGrid/include \
    $$REPO_ROOT/AuSelect/include \
    $$REPO_ROOT/AuDatePicker/include \
    $$PWD/CustomAuGrid \
    $$PWD/demos
DEPENDPATH += $$REPO_ROOT/AuCommon $$REPO_ROOT/AuGrid $$REPO_ROOT/AuSelect $$REPO_ROOT/AuDatePicker

LIBS += -L$$REPO_ROOT/AuCommon/bin/ -lAuCommon
LIBS += -L$$REPO_ROOT/AuGrid/bin/ -lAuGrid
LIBS += -L$$REPO_ROOT/AuSelect/bin/ -lAuSelect
LIBS += -L$$REPO_ROOT/AuDatePicker/bin/ -lAuDatePicker


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# MSVC: 源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
msvc: QMAKE_CXXFLAGS += /utf-8
