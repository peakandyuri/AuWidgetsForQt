QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

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
    $$PWD/../AgCommon/include \
    $$PWD/../AgGrid/include \
    $$PWD/../AgSelect/include \
    $$PWD/../AgDatePicker/include \
    $$PWD/../AgPlot/include \
    $$PWD/pages

DEPENDPATH += $$PWD/../AgCommon $$PWD/../AgGrid $$PWD/../AgSelect $$PWD/../AgDatePicker $$PWD/../AgPlot

win32: LIBS += -L$$PWD/../AgCommon/bin/ -lAgCommon
else:unix: LIBS += -L$$PWD/../AgCommon/bin/ -lAgCommon
win32: LIBS += -L$$PWD/../AgGrid/bin/ -lAgGrid
else:unix: LIBS += -L$$PWD/../AgGrid/bin/ -lAgGrid
win32: LIBS += -L$$PWD/../AgSelect/bin/ -lAgSelect
else:unix: LIBS += -L$$PWD/../AgSelect/bin/ -lAgSelect
win32: LIBS += -L$$PWD/../AgDatePicker/bin/ -lAgDatePicker
else:unix: LIBS += -L$$PWD/../AgDatePicker/bin/ -lAgDatePicker
win32: LIBS += -L$$PWD/../AgPlot/bin/ -lAgPlot
else:unix: LIBS += -L$$PWD/../AgPlot/bin/ -lAgPlot

# 让目标依赖导入库：库重编后自动触发重链接 + POST_LINK 拷贝
win32: PRE_TARGETDEPS += \
    $$PWD/../AgCommon/bin/libAgCommon.a \
    $$PWD/../AgGrid/bin/libAgGrid.a \
    $$PWD/../AgSelect/bin/libAgSelect.a \
    $$PWD/../AgDatePicker/bin/libAgDatePicker.a \
    $$PWD/../AgPlot/bin/libAgPlot.a

# 构建完成后把最新 DLL 拷贝到可执行文件目录，避免运行时加载到旧版 DLL
defineTest(copyAgDll) {
    dll = $$1
    win32 {
        CONFIG(release, debug|release) {
            QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$dll) $$shell_path($$OUT_PWD/release) $$escape_expand(\\n\\t)
        } else {
            QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$dll) $$shell_path($$OUT_PWD/debug) $$escape_expand(\\n\\t)
        }
    }
}

copyAgDll($$PWD/../AgCommon/bin/AgCommon.dll)
copyAgDll($$PWD/../AgGrid/bin/AgGrid.dll)
copyAgDll($$PWD/../AgSelect/bin/AgSelect.dll)
copyAgDll($$PWD/../AgDatePicker/bin/AgDatePicker.dll)
copyAgDll($$PWD/../AgPlot/bin/AgPlot.dll)

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
