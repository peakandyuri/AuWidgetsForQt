QT += core widgets

TEMPLATE = lib
DEFINES += AGGRID_LIBRARY

CONFIG += c++11

DESTDIR = $$PWD/bin

INCLUDEPATH += \
    $$PWD/include \
    $$PWD/core \
    $$PWD/widget \
    $$PWD/style \
    $$PWD/renderers \
    $$PWD/editors \
    $$PWD/validators

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
    core/AgColumnDef.cpp \
    core/AgGridModel.cpp \
    widget/AgGridWidget.cpp \
    widget/AgGridScrollBar.cpp \
    widget/AgGridSyncGroup.cpp \
    style/AgGridStyle.cpp \
    style/AgDarkGridStyle.cpp \
    renderers/AgCellRenderer.cpp \
    renderers/AgHeaderRenderer.cpp \
    renderers/AgDefaultCellRenderer.cpp \
    renderers/AgCheckBoxCellRenderer.cpp \
    editors/AgCellEditor.cpp \
    editors/AgDefaultCellEditor.cpp \
    editors/AgMultiLineCellEditor.cpp \
    editors/AgComboBoxCellEditor.cpp \
    validators/AgCellEditValidator.cpp

HEADERS += \
    include/AgGrid_global.h \
    include/AgColumnDef.h \
    include/AgGridModel.h \
    include/AgTreeRow.h \
    include/AgGridWidget.h \
    include/AgGridSyncGroup.h \
    widget/AgGridScrollBar.h \
    include/AgGridStyle.h \
    include/AgDarkGridStyle.h \
    include/AgCellRenderer.h \
    include/AgHeaderRenderer.h \
    include/AgDefaultCellRenderer.h \
    include/AgCheckBoxCellRenderer.h \
    include/AgCellEditor.h \
    include/AgDefaultCellEditor.h \
    include/AgMultiLineCellEditor.h \
    include/AgComboBoxCellEditor.h \
    include/AgCellEditValidator.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
