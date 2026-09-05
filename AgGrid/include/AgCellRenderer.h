#ifndef AGCELLRENDERER_H
#define AGCELLRENDERER_H

#include "AgGrid_global.h"

#include <QVariant>
#include <QPainter>
#include <QColor>
#include <QPoint>

class AgColumnDef;
class AgGridModel;
class AgGridStyle;

struct AgCellRendererContext
{
    bool hovered = false;
    bool selected = false;
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter;
};

class AGGRID_EXPORT AgCellRenderer
{
public:
    virtual ~AgCellRenderer();

    virtual void paint(QPainter *painter,
                       const QRect &rect,
                       const QVariant &value,
                       const AgColumnDef &columnDef,
                       int row,
                       int column,
                       const AgGridModel *model,
                       const AgGridStyle *style,
                       const QColor &textColor) const = 0;

    // 带 hover/selected 状态的绘制入口，默认转调旧接口以保持兼容。
    virtual void paint(QPainter *painter,
                       const QRect &rect,
                       const QVariant &value,
                       const AgColumnDef &columnDef,
                       int row,
                       int column,
                       const AgGridModel *model,
                       const AgGridStyle *style,
                       const QColor &textColor,
                       const AgCellRendererContext &context) const
    {
        Q_UNUSED(context)
        paint(painter, rect, value, columnDef, row, column, model, style, textColor);
    }

    // 鼠标进入/离开/在单元格内移动。cellPos 为单元格左上角为 (0,0) 的相对坐标。
    virtual void cellMouseEnter(int row, int column, const QPoint &cellPos) { Q_UNUSED(row) Q_UNUSED(column) Q_UNUSED(cellPos) }
    virtual void cellMouseLeave(int row, int column) { Q_UNUSED(row) Q_UNUSED(column) }
    virtual void cellMouseMove(int row, int column, const QPoint &cellPos) { Q_UNUSED(row) Q_UNUSED(column) Q_UNUSED(cellPos) }

    // 返回 true 表示当前单元格需要显示手型光标。
    virtual bool wantsHoverCursor(int row, int column, const QPoint &cellPos) const { Q_UNUSED(row) Q_UNUSED(column) Q_UNUSED(cellPos) return false; }

    // 返回单元格期望高度；返回 -1 表示使用表格默认行高。
    virtual int preferredHeight(const QRect &rect,
                                const QVariant &value,
                                const AgColumnDef &columnDef,
                                int row,
                                int column,
                                const AgGridModel *model,
                                const AgGridStyle *style) const { Q_UNUSED(rect) Q_UNUSED(value) Q_UNUSED(columnDef) Q_UNUSED(row) Q_UNUSED(column) Q_UNUSED(model) Q_UNUSED(style) return -1; }

    // 返回该单元格复制到剪贴板的内容。
    // 返回无效的 QVariant 表示该渲染器不提供复制内容，此时不会复制到剪贴板。
    virtual QVariant copyContent(const QVariant &value,
                                 const AgColumnDef &columnDef,
                                 int row,
                                 int column,
                                 const AgGridModel *model) const
    {
        Q_UNUSED(value) Q_UNUSED(columnDef) Q_UNUSED(row) Q_UNUSED(column) Q_UNUSED(model)
        return QVariant();
    }
};

#endif // AGCELLRENDERER_H
