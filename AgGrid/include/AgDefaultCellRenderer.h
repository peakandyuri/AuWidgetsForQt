#ifndef AGDEFAULTCELLRENDERER_H
#define AGDEFAULTCELLRENDERER_H

#include "AgCellRenderer.h"

class AGGRID_EXPORT AgDefaultCellRenderer : public AgCellRenderer
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

    int preferredHeight(const QRect &rect,
                        const QVariant &value,
                        const AgColumnDef &columnDef,
                        int row,
                        int column,
                        const AgGridModel *model,
                        const AgGridStyle *style) const override;

    QVariant copyContent(const QVariant &value,
                         const AgColumnDef &columnDef,
                         int row,
                         int column,
                         const AgGridModel *model) const override;
};

#endif // AGDEFAULTCELLRENDERER_H
