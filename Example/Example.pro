QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

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
    demos/AgSelectDemoWindow.cpp \
    CustomAgGrid/ButtonRenderer.cpp \
    CustomAgGrid/HeaderButtonRenderer.cpp \
    CustomAgGrid/HeaderCheckBoxRenderer.cpp \
    CustomAgGrid/DemoValidator.cpp \
    CustomAgGrid/DemoStyle.cpp

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
    demos/AgSelectDemoWindow.h \
    CustomAgGrid/ButtonRenderer.h \
    CustomAgGrid/HeaderButtonRenderer.h \
    CustomAgGrid/HeaderCheckBoxRenderer.h \
    CustomAgGrid/DemoValidator.h \
    CustomAgGrid/DemoStyle.h

FORMS += \
    mainwindow.ui

INCLUDEPATH += \
    $$PWD/../AgCommon/include \
    $$PWD/../AgGrid/include \
    $$PWD/../AgSelect/include \
    $$PWD/CustomAgGrid \
    $$PWD/demos
DEPENDPATH += $$PWD/../AgCommon $$PWD/../AgGrid $$PWD/../AgSelect

win32: LIBS += -L$$PWD/../AgCommon/bin/ -lAgCommon
else:unix: LIBS += -L$$PWD/../AgCommon/bin/ -lAgCommon
win32: LIBS += -L$$PWD/../AgGrid/bin/ -lAgGrid
else:unix: LIBS += -L$$PWD/../AgGrid/bin/ -lAgGrid
win32: LIBS += -L$$PWD/../AgSelect/bin/ -lAgSelect
else:unix: LIBS += -L$$PWD/../AgSelect/bin/ -lAgSelect

# 让目标依赖导入库：库重编后自动触发重链接 + POST_LINK 拷贝
win32: PRE_TARGETDEPS += \
    $$PWD/../AgCommon/bin/libAgCommon.a \
    $$PWD/../AgGrid/bin/libAgGrid.a \
    $$PWD/../AgSelect/bin/libAgSelect.a

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

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
