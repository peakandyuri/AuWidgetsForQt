QT += core widgets

TEMPLATE = lib
DEFINES += AUGRID_LIBRARY

CONFIG += c++11
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG -= c++11
    CONFIG += c++17
}

DESTDIR = $$PWD/bin

INCLUDEPATH += \
    $$PWD/include \
    $$PWD/core \
    $$PWD/widget \
    $$PWD/style \
    $$PWD/renderers \
    $$PWD/editors \
    $$PWD/validators \
    $$PWD/../AuCommon/include

DEPENDPATH += $$PWD/../AuCommon

win32: LIBS += -L$$PWD/../AuCommon/bin/ -lAuCommon
else:unix: LIBS += -L$$PWD/../AuCommon/bin/ -lAuCommon

# 让目标依赖 AuCommon 导入库：公共库重编后自动触发重链接
# MinGW 导入库为 libAuCommon.a，MSVC 为 AuCommon.lib
win32-g++: PRE_TARGETDEPS += $$PWD/../AuCommon/bin/libAuCommon.a
else:win32: PRE_TARGETDEPS += $$PWD/../AuCommon/bin/AuCommon.lib

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    core/AuColumnDef.cpp \
    core/AuGridModel.cpp \
    widget/AuGridWidget.cpp \
    widget/AuGridWidgetPaint.cpp \
    widget/AuGridWidgetInteraction.cpp \
    widget/AuGridWidgetEditing.cpp \
    widget/AuGridWidgetColumns.cpp \
    widget/AuGridWidgetRows.cpp \
    widget/AuGridWidgetAnimation.cpp \
    widget/AuGridSyncGroup.cpp \
    style/AuGridStyle.cpp \
    style/AuDarkGridStyle.cpp \
    renderers/AuCellRenderer.cpp \
    renderers/AuHeaderRenderer.cpp \
    renderers/AuDefaultCellRenderer.cpp \
    renderers/AuCheckBoxCellRenderer.cpp \
    renderers/AuButtonCellRenderer.cpp \
    renderers/AuCellButton.cpp \
    editors/AuCellEditor.cpp \
    editors/AuDefaultCellEditor.cpp \
    editors/AuMultiLineCellEditor.cpp \
    editors/AuComboBoxCellEditor.cpp \
    validators/AuCellEditValidator.cpp

HEADERS += \
    include/AuGrid_global.h \
    include/AuColumnDef.h \
    include/AuGridModel.h \
    include/AuTreeRow.h \
    include/AuGridWidget.h \
    widget/AuGridWidget_p.h \
    include/AuGridSyncGroup.h \
    include/AuGridStyle.h \
    include/AuDarkGridStyle.h \
    include/AuCellRenderer.h \
    include/AuHeaderRenderer.h \
    include/AuDefaultCellRenderer.h \
    include/AuCheckBoxCellRenderer.h \
    include/AuCellButton.h \
    include/AuButtonCellRenderer.h \
    include/AuCellEditor.h \
    include/AuDefaultCellEditor.h \
    include/AuMultiLineCellEditor.h \
    include/AuComboBoxCellEditor.h \
    include/AuCellEditValidator.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target

# MSVC: 源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
msvc: QMAKE_CXXFLAGS += /utf-8
