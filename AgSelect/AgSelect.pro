QT += core widgets

TEMPLATE = lib
DEFINES += AGSELECT_LIBRARY

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
# depend on your compiler). Please consult the documentation
# for the deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    widget/AgSelect.cpp \
    widget/AgSelectModel.cpp \
    widget/AgSelectItemDelegate.cpp \
    widget/AgSelectListView.cpp \
    style/AgSelectStyle.cpp

HEADERS += \
    include/AgSelect_global.h \
    include/AgSelect.h \
    include/AgSelectStyle.h \
    include/AgSelectItemDelegate.h \
    widget/AgSelectModel.h \
    widget/AgSelectListView.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
