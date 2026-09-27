#include "AuCheckBoxCellRenderer.h"

#include "AuColumnDef.h"
#include "AuGridModel.h"
#include "AuGridStyle.h"
#include <QPainterPath>

void AuCheckBoxCellRenderer::paint(QPainter *painter,
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

void AuCheckBoxCellRenderer::paint(QPainter *painter,
                                   const QRect &rect,
                                   const QVariant &value,
                                   const AuColumnDef &,
                                   int, int,
                                   const AuGridModel *model,
                                   const AuGridStyle *style,
                                   const QColor &,
                                   const AuCellRendererContext &) const
{
    const bool checked = value.toBool();
    const int size = style ? style->integer(AuGridStyle::CheckboxSize, model) : 16;
    const QRect box(rect.center().x() - size / 2, rect.center().y() - size / 2, size, size);

    QColor checkedColor = style->color(AuGridStyle::CheckboxCheckedColor, model);
    QColor uncheckedColor = style->color(AuGridStyle::CheckboxUncheckedColor, model);
    QColor borderColor = style->color(AuGridStyle::CheckboxBorderColor, model);
    QColor checkmarkColor = style->color(AuGridStyle::CheckboxCheckmarkColor, model);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const int radius = 3;
    if (checked) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(checkedColor.isValid() ? checkedColor : QColor(0, 120, 215));
        painter->drawRoundedRect(box, radius, radius);

        painter->setPen(QPen(checkmarkColor.isValid() ? checkmarkColor : Qt::white, 2));
        QPainterPath path;
        path.moveTo(box.left() + 4, box.center().y());
        path.lineTo(box.center().x() - 1, box.bottom() - 4);
        path.lineTo(box.right() - 4, box.top() + 4);
        painter->drawPath(path);
    } else {
        if (uncheckedColor.isValid()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(uncheckedColor);
            painter->drawRoundedRect(box, radius, radius);
        }
        if (borderColor.isValid()) {
            painter->setPen(QPen(borderColor, 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(box.adjusted(1, 1, -1, -1), radius, radius);
        }
    }

    painter->restore();
}

bool AuCheckBoxCellRenderer::wantsHoverCursor(int, int, const QPoint &) const
{
    return true;
}
