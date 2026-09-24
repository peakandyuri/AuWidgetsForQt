QT += core widgets

TEMPLATE = lib
DEFINES += AGDATEPICKER_LIBRARY

CONFIG += c++11

DESTDIR = $$PWD/bin

INCLUDEPATH += \
    $$PWD/include \
    $$PWD/widget \
    $$PWD/style \
    $$PWD/../AgCommon/include

DEPENDPATH += $$PWD/../AgCommon

win32: LIBS += -L$$PWD/../AgCommon/bin/ -lAgCommon
else:unix: LIBS += -L$$PWD/../AgCommon/bin/ -lAgCommon

# 让目标依赖 AgCommon 导入库：公共库重编后自动触发重链接
win32: PRE_TARGETDEPS += $$PWD/../AgCommon/bin/libAgCommon.a

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation for the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# You can even select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0

SOURCES += \
    widget/AgDatePicker.cpp \
    widget/AgDatePickerPanel.cpp \
    widget/AgDateRangePanel.cpp \
    style/AgDatePickerStyle.cpp

HEADERS += \
    include/AgDatePicker_global.h \
    include/AgDatePicker.h \
    include/AgDatePickerStyle.h \
    widget/AgDatePickerPanel.h \
    widget/AgDateRangePanel.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
