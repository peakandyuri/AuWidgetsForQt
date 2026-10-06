QT += core widgets

TEMPLATE = lib
DEFINES += AUDATEPICKER_LIBRARY

CONFIG += c++11
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG -= c++11
    CONFIG += c++17
}

DESTDIR = $$PWD/bin

INCLUDEPATH += \
    $$PWD/include \
    $$PWD/widget \
    $$PWD/style \
    $$PWD/../AuCommon/include

DEPENDPATH += $$PWD/../AuCommon

win32: LIBS += -L$$PWD/../AuCommon/bin/ -lAuCommon
else:unix: LIBS += -L$$PWD/../AuCommon/bin/ -lAuCommon

# 让目标依赖 AuCommon 导入库：公共库重编后自动触发重链接
win32: PRE_TARGETDEPS += $$PWD/../AuCommon/bin/libAuCommon.a

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation for the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# You can even select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0

SOURCES += \
    widget/AuDatePicker.cpp \
    widget/AuDatePickerPanel.cpp \
    widget/AuDateRangePanel.cpp \
    widget/AuTimePanel.cpp \
    style/AuDatePickerStyle.cpp

HEADERS += \
    include/AuDatePicker_global.h \
    include/AuDatePicker.h \
    include/AuDatePickerStyle.h \
    include/AuTimePanel.h \
    widget/AuDatePickerPanel.h \
    widget/AuDateRangePanel.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
