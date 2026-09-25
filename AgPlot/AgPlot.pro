QT += core widgets

TEMPLATE = lib
DEFINES += AGPLOT_LIBRARY

CONFIG += c++11

DESTDIR = $$PWD/bin

INCLUDEPATH += \
    $$PWD/include \
    $$PWD/widget

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    widget/AgPlot.cpp \
    widget/AgPlotAxis.cpp \
    widget/AgPlotCurve.cpp \
    widget/AgPlotItem.cpp \
    widget/AgPlotPrimitive.cpp \
    widget/AgPlotStyle.cpp

HEADERS += \
    include/AgPlot_global.h \
    include/AgPlot.h \
    include/AgPlotAxis.h \
    include/AgPlotCurve.h \
    include/AgPlotItem.h \
    include/AgPlotPrimitive.h \
    include/AgPlotStyle.h \
    include/AgPlotTypes.h \
    widget/AgPlotInternal.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
