#include "AuSelectItemDelegate.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

AuSelectItemDelegate::AuSelectItemDelegate(AuSelectStyle *style, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_style(style)
    , m_multiSelection(false)
{
}

AuSelectStyle *AuSelectItemDelegate::style() const
{
    return m_style;
}

void AuSelectItemDelegate::setStyle(AuSelectStyle *style)
{
    m_style = style;
}

void AuSelectItemDelegate::setFilterText(const QString &text)
{
    if (m_filterText == text)
        return;
    m_filterText = text;
}

QString AuSelectItemDelegate::filterText() const
{
    return m_filterText;
}

void AuSelectItemDelegate::setMultiSelection(bool multi)
{
    m_multiSelection = multi;
}

bool AuSelectItemDelegate::isMultiSelection() const
{
    return m_multiSelection;
}

QSize AuSelectItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
    Q_UNUSED(option);
    if (!m_style)
        return QSize(0, 0);
    // 行高包含间距（由 paint 中的背景矩形吸收间距），面板内容高度可据此精确计算
    const int type = index.data(AuSelectItemRole::RowType).toInt();
    const int height = (type == AuSelectItemRole::RowGroupHeader)
                           ? m_style->groupHeaderHeight()
                           : m_style->itemHeight();
    return QSize(QWIDGETSIZE_MAX, height + m_style->itemSpacing());
}

void AuSelectItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    if (!m_style)
        return;
    const AuSelectStyle *style = m_style;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const int type = index.data(AuSelectItemRole::RowType).toInt();
    if (type == AuSelectItemRole::RowGroupHeader) {
        // 组名行：明显区别于选项色，无 hover、无选中效果
        const QRect textRect = option.rect.adjusted(style->itemHorizontalPadding(), 0,
                                                    -style->itemHorizontalPadding(), 0);
        painter->setFont(option.font);
        painter->setPen(style->groupHeaderForegroundColor());
        const QString text = option.fontMetrics.elidedText(
            index.data(Qt::DisplayRole).toString(), Qt::ElideRight, textRect.width());
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
        painter->restore();
        return;
    }

    const bool enabled = index.data(AuSelectItemRole::ItemEnabled).toBool();
    const bool checked = index.data(AuSelectItemRole::ItemChecked).toBool();
    const bool highlighted = index.data(AuSelectItemRole::ItemHighlighted).toBool();
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);

    // 背景：选中（含 hover 加深）> hover / 键盘高亮 > 无；上下留出 itemSpacing 间距
    QColor background;
    if (enabled) {
        if (checked)
            background = hovered ? style->itemHoverSelectedBackgroundColor()
                                 : style->itemSelectedBackgroundColor();
        else if (hovered || highlighted)
            background = style->itemHoverBackgroundColor();
    }
    if (background.isValid() && background.alpha() > 0) {
        const int spacing = style->itemSpacing();
        const QRect backgroundRect = option.rect.adjusted(2, spacing / 2, -2, -(spacing - spacing / 2));
        painter->setPen(Qt::NoPen);
        painter->setBrush(background);
        painter->drawRoundedRect(backgroundRect, style->itemBorderRadius(), style->itemBorderRadius());
    }

    // 内容区：分组选项缩进；右侧为对勾预留空间
    const bool inGroup = index.data(AuSelectItemRole::ItemInGroup).toBool();
    const int padding = style->itemHorizontalPadding();
    const int indent = inGroup ? style->itemIndentWidth() : 0;
    const int checkSpace = (m_multiSelection && checked) ? style->itemHeight() : 0;
    QRect contentRect = option.rect.adjusted(padding + indent, 0, -(padding + checkSpace), 0);

    // 图标
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    if (!icon.isNull()) {
        const int iconSize = qBound(12, option.rect.height() - 12, 24);
        const QRect iconRect(contentRect.left(),
                             option.rect.center().y() - iconSize / 2,
                             iconSize, iconSize);
        icon.paint(painter, iconRect, Qt::AlignCenter,
                   enabled ? QIcon::Normal : QIcon::Disabled,
                   checked ? QIcon::On : QIcon::Off);
        contentRect.setLeft(iconRect.right() + 6);
    }

    // 文字：禁用 / 选中 跟随样式色，过滤命中部分高亮
    QColor textColor = style->itemForegroundColor();
    if (!enabled)
        textColor = style->itemDisabledForegroundColor();
    else if (checked)
        textColor = style->itemSelectedForegroundColor();
    drawItemText(painter, contentRect, index.data(Qt::DisplayRole).toString(),
                 option.font, textColor);

    // 多选对勾（最右侧）
    if (m_multiSelection && checked) {
        const int mark = qBound(8, option.rect.height() / 3, 14);
        const QRectF markRect(option.rect.right() - padding - mark * 2.0,
                              option.rect.center().y() - mark / 2.0, mark * 2.0, mark);
        drawCheckmark(painter, markRect, style->itemCheckmarkColor());
    }

    painter->restore();
}

void AuSelectItemDelegate::drawItemText(QPainter *painter, const QRect &rect,
                                        const QString &text, const QFont &font,
                                        const QColor &color) const
{
    if (rect.width() <= 0 || text.isEmpty())
        return;
    const QFontMetrics metrics(font);
    const QString elided = metrics.elidedText(text, Qt::ElideRight, rect.width());

    const QColor highlightColor = m_style->matchHighlightForegroundColor();
    if (!m_filterText.isEmpty() && highlightColor.isValid()) {
        const int matchIndex = elided.indexOf(m_filterText, 0, Qt::CaseInsensitive);
        if (matchIndex >= 0) {
            const int matchLength = m_filterText.length();
            const QString before = elided.left(matchIndex);
            const QString matched = elided.mid(matchIndex, matchLength);
            const QString after = elided.mid(matchIndex + matchLength);

            painter->setFont(font);
            const int y = rect.center().y() + (metrics.height() / 2) - metrics.descent();
            int x = rect.left();

            if (!before.isEmpty()) {
                painter->setPen(color);
                painter->drawText(x, y, before);
                x += metrics.horizontalAdvance(before);
            }
            painter->setPen(highlightColor.isValid() ? highlightColor : color);
            painter->drawText(x, y, matched);
            x += metrics.horizontalAdvance(matched);
            if (!after.isEmpty()) {
                painter->setPen(color);
                painter->drawText(x, y, after);
            }
            return;
        }
    }

    painter->setFont(font);
    painter->setPen(color);
    painter->drawText(rect, Qt::AlignVCenter | Qt::AlignLeft, elided);
}

void AuSelectItemDelegate::drawCheckmark(QPainter *painter, const QRectF &rect,
                                         const QColor &color) const
{
    QPen pen(color, 2.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    const qreal r = qMin(rect.width(), rect.height()) / 2.0;
    QPainterPath path;
    path.moveTo(cx - r * 0.7, cy + r * 0.05);
    path.lineTo(cx - r * 0.15, cy + r * 0.6);
    path.lineTo(cx + r * 0.8, cy - r * 0.55);
    painter->drawPath(path);
}
