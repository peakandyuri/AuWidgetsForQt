#ifndef BUTTONRENDERER_H
#define BUTTONRENDERER_H

#include "AgCellRenderer.h"

#include <QPainter>
#include <QRect>
#include <QVariant>
#include <QColor>

class AgColumnDef;
class AgGridModel;
class AgGridStyle;

class ButtonRenderer : public AgCellRenderer
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

#endif // BUTTONRENDERER_H
