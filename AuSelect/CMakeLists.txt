add_library(AuSelect SHARED
    widget/AuSelect.cpp
    widget/AuSelectModel.cpp
    widget/AuSelectItemDelegate.cpp
    widget/AuSelectListView.cpp
    style/AuSelectStyle.cpp
    include/AuSelect_global.h
    include/AuSelect.h
    include/AuSelectStyle.h
    include/AuSelectItemDelegate.h
    widget/AuSelectModel.h
    widget/AuSelectListView.h
)

target_include_directories(AuSelect
    PUBLIC  include
    PRIVATE widget style
)

target_link_libraries(AuSelect
    PUBLIC  AuCommon
    PRIVATE Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Widgets
)

set_target_properties(AuSelect PROPERTIES DEFINE_SYMBOL AUSELECT_LIBRARY)

au_target_output_dirs(AuSelect)
