QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    demos/DialogWindows.cpp \
    demos/MyWin.cpp

HEADERS += \
    MainWindow.h \
    demos/DialogWindows.h \
    demos/MyWin.h

FORMS += \
    demos/MyWinContent.ui

RESOURCES += \
    AgWindowKitDemo.qrc

INCLUDEPATH += \
    $$PWD/../AgWindowKit/include \
    $$PWD/demos

DEPENDPATH += $$PWD/../AgWindowKit

win32: LIBS += -L$$PWD/../AgWindowKit/bin/ -lAgWindowKit
else:unix: LIBS += -L$$PWD/../AgWindowKit/bin/ -lAgWindowKit
