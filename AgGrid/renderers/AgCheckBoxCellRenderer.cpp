#include "AgCheckBoxCellRenderer.h"

#include "AgColumnDef.h"
#include "AgGridModel.h"
#include "AgGridStyle.h"

void AgCheckBoxCellRenderer::paint(QPainter *painter,
                                   const QRect &rect,
                                   const QVariant &value,
                                   const AgColumnDef &columnDef,
                                   int row,
                                   int column,
                                   const AgGridModel *model,
                                   const AgGridStyle *style,
                                   const QColor &textColor) const
{
    AgCellRendererContext ctx;
    paint(painter, rect, value, columnDef, row, column, model, style, textColor, ctx);
}

void AgCheckBoxCellRenderer::paint(QPainter *painter,
                                   const QRect &rect,
                                   const QVariant &value,
                                   const AgColumnDef &,
                                   int, int,
                                   const AgGridModel *model,
                                   const AgGridStyle *style,
                                   const QColor &,
                                   const AgCellRendererContext &) const
{
    const bool checked = value.toBool();
    const int size = style ? style->integer(AgGridStyle::CheckboxSize, model) : 16;
    const QRect box(rect.center().x() - size / 2, rect.center().y() - size / 2, size, size);

    QColor checkedColor = style->color(AgGridStyle::CheckboxCheckedColor, model);
    QColor uncheckedColor = style->color(AgGridStyle::CheckboxUncheckedColor, model);
    QColor borderColor = style->color(AgGridStyle::CheckboxBorderColor, model);
    QColor checkmarkColor = style->color(AgGridStyle::CheckboxCheckmarkColor, model);

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

bool AgCheckBoxCellRenderer::wantsHoverCursor(int, int, const QPoint &) const
{
    return true;
}
