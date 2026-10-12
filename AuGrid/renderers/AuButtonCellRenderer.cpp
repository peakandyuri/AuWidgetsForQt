// ============================================================================
// 内置按钮单元格渲染器：多按钮 / 链接绘制、三态样式派生、热区命中与按下态。
// 点击确认（按下 + 释放同按钮）由 AuGridWidget 依据 cellMousePress/Release
// 的协议转发为 cellButtonClicked 信号，本类只负责绘制与命中判定。
// ============================================================================

#include "AuButtonCellRenderer.h"

#include "AuColumnDef.h"
#include "AuGridModel.h"
#include "AuGridStyle.h"

#include <QApplication>
#include <QCursor>
#include <QPainter>
#include <QStringList>
#include <QToolTip>

namespace
{

// 缺省主色：Link 与未设色按钮的文字默认色
const QColor kDefaultColor(0x16, 0x77, 0xFF);
// 禁用态配色（灰显）
const QColor kDisabledColor(0x96, 0x96, 0x96);
const QColor kDisabledBackground(0xF2, 0xF2, 0xF2);
const QColor kDisabledBorder(0xD9, 0xD9, 0xD9);

QColor auShade(const QColor &color, bool lighter)
{
    if (!color.isValid())
        return color;
    return lighter ? color.lighter(112) : color.darker(112);
}

QColor auTint(const QColor &color, int alpha)
{
    if (!color.isValid())
        return QColor();
    QColor result = color;
    result.setAlpha(alpha);
    return result;
}

} // namespace

AuButtonCellRenderer::AuButtonCellRenderer(AuCellButton::Type defaultType)
    : m_defaultType(defaultType)
    , m_defaultColor(kDefaultColor)
{
}

void AuButtonCellRenderer::setButtons(const QVector<AuCellButton> &buttons)
{
    m_buttons = buttons;
}

void AuButtonCellRenderer::setButtonProvider(const ButtonProvider &provider)
{
    m_provider = provider;
}

void AuButtonCellRenderer::setSpacing(int spacing)
{
    m_spacing = spacing;
}

void AuButtonCellRenderer::setButtonHeight(int height)
{
    m_buttonHeight = height;
}

void AuButtonCellRenderer::setHorizontalPadding(int padding)
{
    m_hPadding = padding;
}

void AuButtonCellRenderer::setRadius(int radius)
{
    m_radius = radius;
}

void AuButtonCellRenderer::setDefaultColor(const QColor &color)
{
    m_defaultColor = color.isValid() ? color : kDefaultColor;
}

QVector<AuCellButton> AuButtonCellRenderer::buttonsFor(const QVariant &value, int row, int column,
                                                       const AuGridModel *model) const
{
    if (m_provider)
        return m_provider(row, column, model);
    if (!m_buttons.isEmpty())
        return m_buttons;

    // 数据驱动：QString 单按钮；QVariantMap 单按钮完整定义；QVariantList 多按钮
    QVector<AuCellButton> buttons;
    if (value.type() == QVariant::List) {
        const QVariantList list = value.toList();
        for (const QVariant &item : list) {
            if (item.type() == QVariant::Map)
                buttons.append(AuCellButton::fromMap(item.toMap()));
            else
                buttons.append(AuCellButton::plain(item.toString()));
        }
    } else if (value.type() == QVariant::Map) {
        buttons.append(AuCellButton::fromMap(value.toMap()));
    } else {
        AuCellButton button;
        button.text = value.toString();
        button.type = m_defaultType;
        if (button.isValid())
            buttons.append(button);
    }

    // Link 形态的内置实例：数据驱动按钮统一按 Link 绘制
    if (m_defaultType == AuCellButton::Type::Link) {
        for (AuCellButton &button : buttons)
            button.type = AuCellButton::Type::Link;
    }
    return buttons;
}

int AuButtonCellRenderer::buttonWidth(const AuCellButton &button, const QFontMetrics &fontMetrics) const
{
    const int padding = m_hPadding >= 0 ? m_hPadding
        : (button.type == AuCellButton::Type::Link ? 4 : 10);
    return fontMetrics.horizontalAdvance(button.text) + 2 * padding;
}

QVector<QRect> AuButtonCellRenderer::buttonRects(const QVector<AuCellButton> &buttons,
                                                 const QRect &rect,
                                                 const QFontMetrics &fontMetrics,
                                                 Qt::Alignment alignment) const
{
    QVector<QRect> rects;
    if (buttons.isEmpty() || rect.isEmpty())
        return rects;

    int total = 0;
    for (const AuCellButton &button : buttons)
        total += buttonWidth(button, fontMetrics);
    total += m_spacing * qMax(0, buttons.size() - 1);

    const int height = qMin(m_buttonHeight, rect.height());
    int x = rect.left();
    if (alignment & Qt::AlignRight)
        x = rect.right() + 1 - total;
    else if (alignment & Qt::AlignHCenter)
        x = rect.left() + (rect.width() - total) / 2;
    const int y = rect.top() + (rect.height() - height) / 2;

    for (const AuCellButton &button : buttons) {
        const int width = buttonWidth(button, fontMetrics);
        rects.append(QRect(x, y, width, height));
        x += width + m_spacing;
    }
    return rects;
}

int AuButtonCellRenderer::buttonIndexAt(const QVector<AuCellButton> &buttons,
                                        const QVector<QRect> &rects,
                                        const QPoint &cellPos) const
{
    for (int i = 0; i < buttons.size() && i < rects.size(); ++i) {
        if (buttons.at(i).enabled && rects.at(i).contains(cellPos))
            return i;
    }
    return -1;
}

QFontMetrics AuButtonCellRenderer::interactionFontMetrics(const AuCellInteractionContext &context) const
{
    // 与 paint 的取字体规则一致：context.font 由控件按“样式覆盖或控件字体”填充
    const QFont font = context.font != QFont() ? context.font : QApplication::font();
    return QFontMetrics(font);
}

void AuButtonCellRenderer::drawButton(QPainter *painter,
                                      const AuCellButton &button,
                                      const QRect &rect,
                                      const QFontMetrics &fontMetrics,
                                      const QColor &defaultTextColor,
                                      bool hovered,
                                      bool pressed) const
{
    if (button.type == AuCellButton::Type::Link) {
        QColor color = button.normal.textColor.isValid() ? button.normal.textColor : m_defaultColor;
        if (!button.enabled) {
            color = kDisabledColor;
        } else if (pressed) {
            color = button.pressed.textColor.isValid() ? button.pressed.textColor : auShade(color, false);
        } else if (hovered) {
            color = button.hover.textColor.isValid() ? button.hover.textColor : auShade(color, true);
        }
        painter->setPen(color);
        painter->drawText(rect, Qt::AlignCenter | Qt::TextSingleLine, button.text);
        if (button.enabled && hovered) {
            // hover 下划线：文字底部下方 2px，与文字等宽
            const int textWidth = fontMetrics.horizontalAdvance(button.text);
            const int underlineY = rect.center().y() + fontMetrics.ascent() / 2 + 2;
            painter->drawLine(QPoint(rect.center().x() - textWidth / 2, underlineY),
                              QPoint(rect.center().x() + textWidth / 2, underlineY));
        }
        return;
    }

    // 描边按钮：normal 基础上派生 hover / pressed / disabled
    QColor text = button.normal.textColor.isValid() ? button.normal.textColor : defaultTextColor;
    QColor background = button.normal.backgroundColor;
    QColor border = button.normal.borderColor.isValid() ? button.normal.borderColor : auTint(text, 110);
    if (!button.enabled) {
        text = kDisabledColor;
        background = kDisabledBackground;
        border = kDisabledBorder;
    } else if (pressed) {
        text = button.pressed.textColor.isValid() ? button.pressed.textColor : auShade(text, false);
        background = button.pressed.backgroundColor.isValid()
                         ? button.pressed.backgroundColor : auTint(auShade(text, false), 45);
        border = button.pressed.borderColor.isValid() ? button.pressed.borderColor : auShade(border, false);
    } else if (hovered) {
        text = button.hover.textColor.isValid() ? button.hover.textColor : auShade(text, true);
        background = button.hover.backgroundColor.isValid()
                         ? button.hover.backgroundColor : auTint(text, 25);
        border = button.hover.borderColor.isValid() ? button.hover.borderColor : auShade(border, true);
    }

    const int radius = button.radius >= 0 ? button.radius : m_radius;
    painter->setPen(QPen(border, 1));
    // 注意不要写 `cond ? background : Qt::NoBrush`：三元两侧统一为 QBrush 时
    // NoBrush 分支会走 QColor 转载路径得到黑色实心画刷，而非空画刷
    if (background.isValid())
        painter->setBrush(background);
    else
        painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(rect.adjusted(0, 0, -1, -1), radius, radius);
    painter->setPen(text);
    painter->drawText(rect, Qt::AlignCenter | Qt::TextSingleLine, button.text);
}

void AuButtonCellRenderer::paint(QPainter *painter,
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

void AuButtonCellRenderer::paint(QPainter *painter,
                                 const QRect &rect,
                                 const QVariant &value,
                                 const AuColumnDef &,
                                 int row,
                                 int column,
                                 const AuGridModel *model,
                                 const AuGridStyle *style,
                                 const QColor &textColor,
                                 const AuCellRendererContext &context) const
{
    const QVector<AuCellButton> buttons = buttonsFor(value, row, column, model);
    if (buttons.isEmpty())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    // 列宽不足时裁剪到单元格内，不溢出到相邻格
    painter->setClipRect(rect);

    const QFont font = style ? style->font(model, row, column) : QFont();
    if (font != QFont())
        painter->setFont(font);
    const QFontMetrics fontMetrics(painter->font());
    const QVector<QRect> rects = buttonRects(buttons, rect, fontMetrics, context.alignment);

    for (int i = 0; i < buttons.size() && i < rects.size(); ++i) {
        const bool hovered = (row == m_hoverRow && column == m_hoverColumn && i == m_hoverIndex);
        const bool pressed = (row == m_pressRow && column == m_pressColumn && i == m_pressIndex);
        drawButton(painter, buttons.at(i), rects.at(i), fontMetrics, textColor, hovered, pressed);
    }

    painter->restore();
}

bool AuButtonCellRenderer::wantsHoverCursor(int row, int column, const QPoint &) const
{
    // cellMouseMove 先于光标裁决执行，热区索引总是新鲜的
    return row == m_hoverRow && column == m_hoverColumn && m_hoverIndex >= 0;
}

bool AuButtonCellRenderer::wantsMouseMoveUpdates() const
{
    return true;
}

void AuButtonCellRenderer::cellMouseLeave(int row, int column)
{
    if (row == m_hoverRow && column == m_hoverColumn)
        m_hoverIndex = -1;
    if (row == m_pressRow && column == m_pressColumn)
        m_pressIndex = -1;
    QToolTip::hideText();
}

void AuButtonCellRenderer::cellMouseMove(int row, int column, const AuCellInteractionContext &context)
{
    m_hoverRow = row;
    m_hoverColumn = column;

    const QVector<AuCellButton> buttons = buttonsFor(context.value, row, column, context.model);
    const QVector<QRect> rects = hitTestRects(context, buttons);
    const int index = buttonIndexAt(buttons, rects, context.cellPos);
    if (index == m_hoverIndex)
        return;
    m_hoverIndex = index;

    // per-button tooltip：离开热区或无提示时隐藏（move 事件上下文，非 paint）
    if (index >= 0 && !buttons.at(index).tooltip.isEmpty())
        QToolTip::showText(QCursor::pos(), buttons.at(index).tooltip);
    else
        QToolTip::hideText();
}

/// 交互命中用：以单元格本地坐标 (0,0) 为基准排布按钮（cellPos 为相对坐标），
/// 尺寸 / 对齐与 paint 一致，保证热区与绘制位置逐像素对应。
QVector<QRect> AuButtonCellRenderer::hitTestRects(const AuCellInteractionContext &context,
                                                  const QVector<AuCellButton> &buttons) const
{
    return buttonRects(buttons, QRect(QPoint(0, 0), context.cellRect.size()),
                       interactionFontMetrics(context), context.alignment);
}

bool AuButtonCellRenderer::cellMousePress(int row, int column, const AuCellInteractionContext &context)
{
    const QVector<AuCellButton> buttons = buttonsFor(context.value, row, column, context.model);
    const QVector<QRect> rects = hitTestRects(context, buttons);
    const int index = buttonIndexAt(buttons, rects, context.cellPos);
    if (index < 0)
        return false;

    m_hoverRow = row;
    m_hoverColumn = column;
    m_hoverIndex = index;
    m_pressRow = row;
    m_pressColumn = column;
    m_pressIndex = index;
    return true;
}

QVariant AuButtonCellRenderer::cellMouseRelease(int row, int column, const AuCellInteractionContext &context)
{
    // 先复位按下态；命中判定基于按下按钮（跨格释放、按钮被移除等一律不算点击）
    const int pressIndex = m_pressIndex;
    const bool sameCell = (row == m_pressRow && column == m_pressColumn);
    m_pressRow = -1;
    m_pressColumn = -1;
    m_pressIndex = -1;
    if (!sameCell || pressIndex < 0)
        return QVariant();

    const QVector<AuCellButton> buttons = buttonsFor(context.value, row, column, context.model);
    const QVector<QRect> rects = hitTestRects(context, buttons);
    const int index = buttonIndexAt(buttons, rects, context.cellPos);
    if (index != pressIndex) {
        m_hoverIndex = index;
        return QVariant();
    }
    return buttons.at(index).toMap();
}

QVariant AuButtonCellRenderer::copyContent(const QVariant &value,
                                           const AuColumnDef &,
                                           int row,
                                           int column,
                                           const AuGridModel *model) const
{
    QStringList texts;
    const QVector<AuCellButton> buttons = buttonsFor(value, row, column, model);
    for (const AuCellButton &button : buttons)
        texts.append(button.text);
    return texts.join(QStringLiteral(" "));
}
