QT += core gui widgets

TEMPLATE = lib
DEFINES += AUCOMMON_LIBRARY

CONFIG += c++11
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG -= c++11
    CONFIG += c++17
}

DESTDIR = $$PWD/bin

INCLUDEPATH += \
    $$PWD/include \
    $$PWD/widget

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation
# for the deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    widget/AuScrollBar.cpp \
    widget/AuPopupPanel.cpp

HEADERS += \
    include/AuCommon_global.h \
    include/AuScrollBar.h \
    include/AuPopupPanel.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
