add_library(AuPlot SHARED
    widget/AuPlot.cpp
    widget/AuPlotAxis.cpp
    widget/AuPlotCurve.cpp
    widget/AuPlotItem.cpp
    widget/AuPlotPrimitive.cpp
    widget/AuPlotStyle.cpp
    include/AuPlot_global.h
    include/AuPlot.h
    include/AuPlotAxis.h
    include/AuPlotCurve.h
    include/AuPlotItem.h
    include/AuPlotPrimitive.h
    include/AuPlotStyle.h
    include/AuPlotTypes.h
    widget/AuPlotInternal.h
)

target_include_directories(AuPlot
    PUBLIC  include
    PRIVATE widget
)

target_link_libraries(AuPlot PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)

set_target_properties(AuPlot PROPERTIES DEFINE_SYMBOL AUPLOT_LIBRARY)

au_target_output_dirs(AuPlot)
