add_library(AuGrid SHARED
    core/AuColumnDef.cpp
    core/AuGridModel.cpp
    widget/AuGridWidget.cpp
    widget/AuGridWidgetPaint.cpp
    widget/AuGridWidgetInteraction.cpp
    widget/AuGridWidgetEditing.cpp
    widget/AuGridWidgetColumns.cpp
    widget/AuGridWidgetRows.cpp
    widget/AuGridWidgetAnimation.cpp
    widget/AuGridSyncGroup.cpp
    style/AuGridStyle.cpp
    style/AuDarkGridStyle.cpp
    renderers/AuCellRenderer.cpp
    renderers/AuHeaderRenderer.cpp
    renderers/AuDefaultCellRenderer.cpp
    renderers/AuCheckBoxCellRenderer.cpp
    editors/AuCellEditor.cpp
    editors/AuDefaultCellEditor.cpp
    editors/AuMultiLineCellEditor.cpp
    editors/AuComboBoxCellEditor.cpp
    validators/AuCellEditValidator.cpp
    include/AuGrid_global.h
    include/AuColumnDef.h
    include/AuGridModel.h
    include/AuTreeRow.h
    include/AuGridWidget.h
    widget/AuGridWidget_p.h
    include/AuGridSyncGroup.h
    include/AuGridStyle.h
    include/AuDarkGridStyle.h
    include/AuCellRenderer.h
    include/AuHeaderRenderer.h
    include/AuDefaultCellRenderer.h
    include/AuCheckBoxCellRenderer.h
    include/AuCellEditor.h
    include/AuDefaultCellEditor.h
    include/AuMultiLineCellEditor.h
    include/AuComboBoxCellEditor.h
    include/AuCellEditValidator.h
)

target_include_directories(AuGrid
    PUBLIC  include
    PRIVATE core widget style renderers editors validators
)

# 公共头 AuGridWidget.h 引用了 AuCommon 的公共头，PUBLIC 传递依赖
target_link_libraries(AuGrid
    PUBLIC  AuCommon
    PRIVATE Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Widgets
)

set_target_properties(AuGrid PROPERTIES DEFINE_SYMBOL AUGRID_LIBRARY)

au_target_output_dirs(AuGrid)
