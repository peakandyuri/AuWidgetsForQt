#include "AgDefaultCellRenderer.h"

#include "AgColumnDef.h"
#include "AgGridModel.h"
#include "AgGridStyle.h"

#include <QFontMetrics>
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
    const QRect textRect = rect.adjusted(hPadding, vPadding, -hPadding, -vPadding);

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
    if (columnDef.textWrapMode() != AgColumnDef::TextWrapMode::WordWrap)
        return -1;

    const QString text = value.toString();
    if (text.isEmpty())
        return -1;

    const int hPadding = style->integer(AgGridStyle::CellHorizontalPadding, model, row, column);
    const int availableWidth = rect.width() - 2 * hPadding;
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
