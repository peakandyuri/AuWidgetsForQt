QT += core widgets

TEMPLATE = lib
DEFINES += AUPLOT_LIBRARY

CONFIG += c++11
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG -= c++11
    CONFIG += c++17
}

DESTDIR = $$PWD/bin

INCLUDEPATH += \
    $$PWD/include \
    $$PWD/widget

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    widget/AuPlot.cpp \
    widget/AuPlotAxis.cpp \
    widget/AuPlotCurve.cpp \
    widget/AuPlotItem.cpp \
    widget/AuPlotPrimitive.cpp \
    widget/AuPlotStyle.cpp

HEADERS += \
    include/AuPlot_global.h \
    include/AuPlot.h \
    include/AuPlotAxis.h \
    include/AuPlotCurve.h \
    include/AuPlotItem.h \
    include/AuPlotPrimitive.h \
    include/AuPlotStyle.h \
    include/AuPlotTypes.h \
    widget/AuPlotInternal.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
