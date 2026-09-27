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
    AuWindowKitDemo.qrc

INCLUDEPATH += \
    $$PWD/../AuWindowKit/include \
    $$PWD/demos

DEPENDPATH += $$PWD/../AuWindowKit

win32: LIBS += -L$$PWD/../AuWindowKit/bin/ -lAuWindowKit
else:unix: LIBS += -L$$PWD/../AuWindowKit/bin/ -lAuWindowKit
