#ifndef AGCHECKBOXCELLRENDERER_H
#define AGCHECKBOXCELLRENDERER_H

#include "AgCellRenderer.h"

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
