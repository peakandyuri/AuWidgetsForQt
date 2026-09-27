#include "ButtonRenderer.h"

void ButtonRenderer::paint(QPainter *painter,
                           const QRect &rect,
                           const QVariant &value,
                           const AuColumnDef &columnDef,
                           int row,
                           int column,
                           const AuGridModel *model,
                           const AuGridStyle *style,
                           const QColor &textColor) const
{
    AuCellRendererContext ctx;
    paint(painter, rect, value, columnDef, row, column, model, style, textColor, ctx);
}

void ButtonRenderer::paint(QPainter *painter,
                           const QRect &rect,
                           const QVariant &value,
                           const AuColumnDef &,
                           int, int,
                           const AuGridModel *,
                           const AuGridStyle *,
                           const QColor &textColor,
                           const AuCellRendererContext &context) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const QColor borderColor = context.hovered ? QColor(70, 130, 220) : QColor(100, 149, 237);
    const QColor bgColor = context.hovered ? QColor(200, 225, 255) : QColor(220, 235, 255);

    painter->setPen(borderColor);
    painter->setBrush(bgColor);
    painter->drawRoundedRect(rect.adjusted(4, 4, -4, -4), 6, 6);
    painter->setPen(textColor);
    painter->drawText(rect, Qt::AlignCenter, value.toString());
    painter->restore();
}

bool ButtonRenderer::wantsHoverCursor(int, int, const QPoint &) const
{
    return true;
}
