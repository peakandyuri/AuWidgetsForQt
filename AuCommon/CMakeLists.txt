add_library(AuCommon SHARED
    widget/AuScrollBar.cpp
    widget/AuPopupPanel.cpp
    include/AuCommon_global.h
    include/AuScrollBar.h
    include/AuPopupPanel.h
)

target_include_directories(AuCommon
    PUBLIC  include
    PRIVATE widget
)

target_link_libraries(AuCommon PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

# 与 qmake 工程一致：AuCommon_global.h 依据该宏决定导出/导入
set_target_properties(AuCommon PROPERTIES DEFINE_SYMBOL AUCOMMON_LIBRARY)

au_target_output_dirs(AuCommon)
