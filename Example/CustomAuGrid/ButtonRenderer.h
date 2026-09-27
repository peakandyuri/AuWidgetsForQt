#ifndef BUTTONRENDERER_H
#define BUTTONRENDERER_H

#include "AuCellRenderer.h"

#include <QPainter>
#include <QRect>
#include <QVariant>
#include <QColor>

class AuColumnDef;
class AuGridModel;
class AuGridStyle;

class ButtonRenderer : public AuCellRenderer
{
public:
    void paint(QPainter *painter,
               const QRect &rect,
               const QVariant &value,
               const AuColumnDef &columnDef,
               int row,
               int column,
               const AuGridModel *model,
               const AuGridStyle *style,
               const QColor &textColor) const override;

    void paint(QPainter *painter,
               const QRect &rect,
               const QVariant &value,
               const AuColumnDef &columnDef,
               int row,
               int column,
               const AuGridModel *model,
               const AuGridStyle *style,
               const QColor &textColor,
               const AuCellRendererContext &context) const override;

    bool wantsHoverCursor(int row, int column, const QPoint &cellPos) const override;
};

#endif // BUTTONRENDERER_H
