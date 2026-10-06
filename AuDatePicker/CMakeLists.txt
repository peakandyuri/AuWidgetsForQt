add_library(AuDatePicker SHARED
    widget/AuDatePicker.cpp
    widget/AuDatePickerPanel.cpp
    widget/AuDateRangePanel.cpp
    widget/AuTimePanel.cpp
    style/AuDatePickerStyle.cpp
    include/AuDatePicker_global.h
    include/AuDatePicker.h
    include/AuDatePickerStyle.h
    include/AuTimePanel.h
    widget/AuDatePickerPanel.h
    widget/AuDateRangePanel.h
)

target_include_directories(AuDatePicker
    PUBLIC  include
    PRIVATE widget style
)

target_link_libraries(AuDatePicker
    PUBLIC  AuCommon
    PRIVATE Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Widgets
)

set_target_properties(AuDatePicker PROPERTIES DEFINE_SYMBOL AUDATEPICKER_LIBRARY)

au_target_output_dirs(AuDatePicker)
