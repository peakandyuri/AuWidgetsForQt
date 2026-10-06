add_executable(FeatureDemo
    main.cpp
    MainWindow.cpp
    pages/PageHelpers.cpp
    pages/GridPages.cpp
    pages/SelectPages.cpp
    pages/DatePickerPages.cpp
    pages/PlotPages.cpp
    MainWindow.h
    FeaturePage.h
    pages/PageHelpers.h
    pages/GridPages.h
    pages/SelectPages.h
    pages/DatePickerPages.h
    pages/PlotPages.h
)

target_include_directories(FeatureDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    pages
)

target_link_libraries(FeatureDemo PRIVATE
    AuGrid
    AuSelect
    AuDatePicker
    AuPlot
    AuCommon
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

au_copy_dlls(FeatureDemo
    "${CMAKE_SOURCE_DIR}/AuCommon/bin/AuCommon.dll"
    "${CMAKE_SOURCE_DIR}/AuGrid/bin/AuGrid.dll"
    "${CMAKE_SOURCE_DIR}/AuSelect/bin/AuSelect.dll"
    "${CMAKE_SOURCE_DIR}/AuDatePicker/bin/AuDatePicker.dll"
    "${CMAKE_SOURCE_DIR}/AuPlot/bin/AuPlot.dll"
)
