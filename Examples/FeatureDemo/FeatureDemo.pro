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

# 仓库根目录（本工程位于 Examples/FeatureDemo，需回溯两层）
REPO_ROOT = $$PWD/../..

INCLUDEPATH += \
    $$REPO_ROOT/AuCommon/include \
    $$REPO_ROOT/AuGrid/include \
    $$REPO_ROOT/AuSelect/include \
    $$REPO_ROOT/AuDatePicker/include \
    $$REPO_ROOT/AuPlot/include \
    $$PWD/pages

DEPENDPATH += $$REPO_ROOT/AuCommon $$REPO_ROOT/AuGrid $$REPO_ROOT/AuSelect $$REPO_ROOT/AuDatePicker $$REPO_ROOT/AuPlot

LIBS += -L$$REPO_ROOT/AuCommon/bin/ -lAuCommon
LIBS += -L$$REPO_ROOT/AuGrid/bin/ -lAuGrid
LIBS += -L$$REPO_ROOT/AuSelect/bin/ -lAuSelect
LIBS += -L$$REPO_ROOT/AuDatePicker/bin/ -lAuDatePicker
LIBS += -L$$REPO_ROOT/AuPlot/bin/ -lAuPlot


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# MSVC: 源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
msvc: QMAKE_CXXFLAGS += /utf-8
