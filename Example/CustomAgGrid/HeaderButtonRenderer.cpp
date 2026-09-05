#include "HeaderButtonRenderer.h"

#include "AgColumnDef.h"

#include <QPainter>
#include <QDebug>

static QRect buttonRect(const QRect &nodeRect)
{
    const int size = 18;
    const int margin = 6;
    const int x = nodeRect.right() - margin - size;
    const int y = nodeRect.top() + (nodeRect.height() - size) / 2;
    return QRect(x, y, size, size);
}

void HeaderButtonRenderer::paint(QPainter *painter, const QRect &rect,
                                 const AgColumnDef &columnDef, int, int,
                                 const AgGridModel *, const AgGridStyle *,
                                 const QColor &textColor,
                                 const AgHeaderRendererContext &context) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // 表头文字（左侧），右侧预留按钮空间
    const QRect node(0, 0, rect.width(), rect.height());
    QRect textRect = rect.adjusted(6, 0, -6, 0);
    textRect.setRight(rect.left() + buttonRect(node).left() - 4);
    painter->setPen(textColor);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, columnDef.headerName());

    // 按钮（右侧）
    const QRect btn = buttonRect(rect);
    const bool over = context.hovered && buttonRect(node).contains(context.headerPos);

    painter->setPen(over ? QColor(70, 130, 220) : QColor(140, 140, 140));
    painter->setBrush(over ? QColor(200, 225, 255) : QColor(235, 235, 235));
    painter->drawRoundedRect(btn, 4, 4);

    // “+”号
    painter->setPen(over ? QColor(70, 130, 220) : QColor(90, 90, 90));
    const int cx = btn.center().x();
    const int cy = btn.center().y();
    const int r = 4;
    painter->drawLine(cx - r, cy, cx + r, cy);
    painter->drawLine(cx, cy - r, cx, cy + r);

    painter->restore();
}

bool HeaderButtonRenderer::wantsHoverCursor(int, int, const QRect &rect, const QPoint &headerPos) const
{
    return buttonRect(QRect(0, 0, rect.width(), rect.height())).contains(headerPos);
}

bool HeaderButtonRenderer::hitTest(int, int, const QRect &rect, const QPoint &headerPos) const
{
    return buttonRect(QRect(0, 0, rect.width(), rect.height())).contains(headerPos);
}

void HeaderButtonRenderer::headerMouseClick(int column, int, const QRect &, const QPoint &)
{
    qDebug() << "Header button clicked on column" << column;
}
