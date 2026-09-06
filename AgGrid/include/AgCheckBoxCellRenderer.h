#ifndef AGCHECKBOXCELLRENDERER_H
#define AGCHECKBOXCELLRENDERER_H

#include "AgCellRenderer.h"

/**
 * @brief 内置复选框单元格渲染器：把 bool 值绘制为复选框。
 *
 * 配合 AgColumnDef::setCheckbox(true) 使用：勾选该列后控件自动选用
 * 本渲染器，点击 / 空格直接切换值并发 cellCheckStateChanged 信号
 * （列需 setEditable(true)）。整格响应点击，wantsHoverCursor 恒为 true。
 * 样式键 CheckboxSize / CheckboxCheckedColor 等控制外观。
 */
class AGGRID_EXPORT AgCheckBoxCellRenderer : public AgCellRenderer
{
public:
    void paint(QPainter *painter,
               const QRect &rect,
               const QVariant &value,
               const AgColumnDef &columnDef,
               int row,
               int column,
               const AgGridModel *model,
               const AgGridStyle *style,
               const QColor &textColor) const override;

    void paint(QPainter *painter,
               const QRect &rect,
               const QVariant &value,
               const AgColumnDef &columnDef,
               int row,
               int column,
               const AgGridModel *model,
               const AgGridStyle *style,
               const QColor &textColor,
               const AgCellRendererContext &context) const override;

    bool wantsHoverCursor(int row, int column, const QPoint &cellPos) const override;
};

#endif // AGCHECKBOXCELLRENDERER_H
