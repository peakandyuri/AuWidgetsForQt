#include "AgDefaultCellRenderer.h"

#include "AgColumnDef.h"
#include "AgGridModel.h"
#include "AgGridStyle.h"

#include <QFontMetrics>
#include <QPolygonF>
#include <QTextOption>

void AgDefaultCellRenderer::paint(QPainter *painter,
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

void AgDefaultCellRenderer::paint(QPainter *painter,
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
    if (!painter)
        return;

    const QColor cellBg = style->value(AgGridStyle::CellBackgroundColor, model, row, column).value<QColor>();
    if (cellBg.isValid()) {
        painter->fillRect(rect, cellBg);

        // 单元格有自定义背景色时，再叠加一层 hover 色，避免 hover 效果被完全盖住。
        if (context.hovered) {
            QColor hover = style->color(AgGridStyle::HoverRowBackgroundColor, model);
            if (hover.isValid()) {
                if (hover.alpha() == 255)
                    hover.setAlpha(60);
                painter->fillRect(rect, hover);
            }
        }
    }

    const QFont defaultFont = painter->font();
    const QFont cellFont = style->font(model, row, column);
    painter->setFont(cellFont != QFont() ? cellFont : defaultFont);

    Qt::Alignment alignment = columnDef.alignment() | Qt::AlignVCenter;
    const QVariant alignVar = style->value(AgGridStyle::CellTextAlignment, model, row, column);
    if (alignVar.isValid() && alignVar.toInt() != 0)
        alignment = static_cast<Qt::Alignment>(alignVar.toInt()) | Qt::AlignVCenter;

    const QColor cellFg = style->value(AgGridStyle::CellForegroundColor, model, row, column).value<QColor>();
    painter->setPen(cellFg.isValid() ? cellFg : textColor);

    const QString text = value.toString();
    const int hPadding = style->integer(AgGridStyle::CellHorizontalPadding, model, row, column);
    const int vPadding = style->integer(AgGridStyle::CellVerticalPadding, model, row, column);

    // 树形首列：先画层级缩进与展开箭头，文字让出占位宽度。
    // 箭头盒 x = rect.left() + hPadding + depth * 缩进宽（控件热区命中使用同一公式）。
    int treeLeading = 0;
    if (context.treeRow) {
        const int indentWidth = style->integer(AgGridStyle::TreeIndentWidth, model, row, column);
        const int expanderSize = style->integer(AgGridStyle::TreeExpanderSize, model, row, column);
        const int expanderGap = qMax(4, expanderSize / 4);
        const int expanderX = rect.left() + hPadding + context.treeDepth * indentWidth;
        const QRect expanderRect(expanderX,
                                 rect.center().y() - expanderSize / 2,
                                 expanderSize, expanderSize);
        treeLeading = context.treeDepth * indentWidth + expanderSize + expanderGap;

        if (context.treeHasChildren) {
            QColor expanderColor = context.treeExpanderHovered
                ? style->color(AgGridStyle::TreeExpanderHoveredColor, model, row, column)
                : style->color(AgGridStyle::TreeExpanderColor, model, row, column);
            if (expanderColor.isValid()) {
                painter->save();
                painter->setRenderHint(QPainter::Antialiasing, true);
                painter->setPen(Qt::NoPen);
                painter->setBrush(expanderColor);
                const qreal cx = expanderRect.center().x();
                const qreal cy = expanderRect.center().y();
                const qreal r = expanderRect.width() / 2.0;
                QPolygonF triangle;
                if (context.treeExpanded) {
                    triangle << QPointF(cx - r * 0.60, cy - r * 0.35)
                             << QPointF(cx + r * 0.60, cy - r * 0.35)
                             << QPointF(cx, cy + r * 0.45);
                } else {
                    triangle << QPointF(cx - r * 0.35, cy - r * 0.60)
                             << QPointF(cx + r * 0.45, cy)
                             << QPointF(cx - r * 0.35, cy + r * 0.60);
                }
                painter->drawPolygon(triangle);
                painter->restore();
            }
        }
    }

    const QRect textRect = rect.adjusted(hPadding + treeLeading, vPadding, -hPadding, -vPadding);

    switch (columnDef.textWrapMode()) {
    case AgColumnDef::TextWrapMode::WordWrap: {
        QFontMetrics fm(painter->font());
        const QRect br = fm.boundingRect(QRect(0, 0, textRect.width(), INT_MAX),
                                         Qt::TextWordWrap, text);
        QRect drawRect = textRect;
        if (br.height() < textRect.height()) {
            const int yOffset = (textRect.height() - br.height()) / 2;
            drawRect.adjust(0, yOffset, 0, yOffset);
        }
        QTextOption option(alignment & Qt::AlignHorizontal_Mask);
        option.setWrapMode(QTextOption::WordWrap);
        painter->drawText(drawRect, text, option);
        break;
    }
    case AgColumnDef::TextWrapMode::NoWrapTruncate: {
        QFontMetrics fm(painter->font());
        const QString elided = fm.elidedText(text, Qt::ElideRight, textRect.width());
        painter->drawText(textRect, alignment | Qt::TextSingleLine, elided);
        break;
    }
    case AgColumnDef::TextWrapMode::NoWrapOverflow:
        painter->drawText(textRect, alignment | Qt::TextSingleLine | Qt::TextDontClip, text);
        break;
    case AgColumnDef::TextWrapMode::NoWrap:
    default:
        painter->drawText(textRect, alignment | Qt::TextSingleLine, text);
        break;
    }
}

int AgDefaultCellRenderer::preferredHeight(const QRect &rect,
                                           const QVariant &value,
                                           const AgColumnDef &columnDef,
                                           int row,
                                           int column,
                                           const AgGridModel *model,
                                           const AgGridStyle *style) const
{
    AgCellRendererContext ctx;
    return preferredHeight(rect, value, columnDef, row, column, model, style, ctx);
}

int AgDefaultCellRenderer::preferredHeight(const QRect &rect,
                                           const QVariant &value,
                                           const AgColumnDef &columnDef,
                                           int row,
                                           int column,
                                           const AgGridModel *model,
                                           const AgGridStyle *style,
                                           const AgCellRendererContext &context) const
{
    if (columnDef.textWrapMode() != AgColumnDef::TextWrapMode::WordWrap)
        return -1;

    const QString text = value.toString();
    if (text.isEmpty())
        return -1;

    const int hPadding = style->integer(AgGridStyle::CellHorizontalPadding, model, row, column);

    // 树形首列扣减缩进 + 箭头占位（与 paint 的 textRect 让位保持一致）
    int treeLeading = 0;
    if (context.treeRow) {
        const int indentWidth = style->integer(AgGridStyle::TreeIndentWidth, model, row, column);
        const int expanderSize = style->integer(AgGridStyle::TreeExpanderSize, model, row, column);
        treeLeading = context.treeDepth * indentWidth + expanderSize + qMax(4, expanderSize / 4);
    }

    const int availableWidth = rect.width() - 2 * hPadding - treeLeading;
    if (availableWidth <= 0)
        return -1;

    const QFont cellFont = style->font(model, row, column);
    const QFontMetrics fm(cellFont);
    const QRect br = fm.boundingRect(QRect(0, 0, availableWidth, INT_MAX),
                                     Qt::TextWordWrap, text);
    const int vPadding = style->integer(AgGridStyle::CellVerticalPadding, model, row, column);
    return qMax(br.height(), fm.height()) + 2 * vPadding;
}

QVariant AgDefaultCellRenderer::copyContent(const QVariant &value,
                                            const AgColumnDef & /*columnDef*/,
                                            int /*row*/,
                                            int /*column*/,
                                            const AgGridModel * /*model*/) const
{
    return value;
}
