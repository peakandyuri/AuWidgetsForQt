#include "AgSelectStyle.h"

#include "AgSelect.h"

#include <QApplication>
#include <QtGlobal>

AgSelectStyle *AgSelectStyle::s_globalStyle = nullptr;
QVector<AgSelect *> AgSelectStyle::s_observers;

AgSelectStyle::AgSelectStyle()
{
    // 默认：亮色主题、Medium 尺寸
    applySizeModePreset(SizeMode::Medium);

    // 编辑框
    m_editorBackgroundColor = QColor(Qt::white);
    m_editorForegroundColor = QColor(0, 0, 0, 224);
    m_editorPlaceholderColor = QColor(0, 0, 0, 64);
    m_editorBorderColor = QColor(0xd9, 0xd9, 0xd9);
    m_editorHoverBorderColor = QColor(0x40, 0x96, 0xff);
    m_editorActiveBorderColor = QColor(0x16, 0x77, 0xff);
    m_editorDisabledBackgroundColor = QColor(0, 0, 0, 10);
    m_editorDisabledForegroundColor = QColor(0, 0, 0, 64);
    m_editorDisabledBorderColor = QColor(0xd9, 0xd9, 0xd9);

    // chip
    m_chipBackgroundColor = QColor(0, 0, 0, 10);
    m_chipHoverBackgroundColor = QColor(0, 0, 0, 20);
    m_chipForegroundColor = QColor(0, 0, 0, 224);
    m_chipBorderColor = QColor();                     // 默认不绘制边框
    m_chipRemoveIconColor = QColor(0, 0, 0, 115);
    m_chipRemoveIconHoverColor = QColor(0, 0, 0, 224);

    // 面板
    m_panelBackgroundColor = QColor(Qt::white);
    m_panelBorderColor = QColor(5, 5, 5, 15);
    m_panelShadowColor = QColor(0, 0, 0, 45);
    m_panelShadowRadius = 10;

    // 下拉项
    m_itemForegroundColor = QColor(0, 0, 0, 224);
    m_itemDisabledForegroundColor = QColor(0, 0, 0, 64);
    m_itemHoverBackgroundColor = QColor(0xf5, 0xf5, 0xf5);
    m_itemSelectedBackgroundColor = QColor(0xe6, 0xf4, 0xff);   // 淡蓝
    m_itemSelectedForegroundColor = QColor(0x16, 0x77, 0xff);
    m_itemHoverSelectedBackgroundColor = QColor(0xba, 0xe0, 0xff); // 天蓝
    m_itemCheckmarkColor = QColor(0x16, 0x77, 0xff);
    m_matchHighlightForegroundColor = QColor(0x16, 0x77, 0xff);

    // 组名 / 箭头
    m_groupHeaderForegroundColor = QColor(0, 0, 0, 115);
    m_arrowColor = QColor(0, 0, 0, 115);
}

AgSelectStyle::~AgSelectStyle()
{
}

AgSelectStyle *AgSelectStyle::createLightStyle(SizeMode mode)
{
    AgSelectStyle *style = new AgSelectStyle();
    style->setSizeMode(mode);
    return style;
}

AgSelectStyle *AgSelectStyle::createDarkStyle(SizeMode mode)
{
    AgSelectStyle *style = new AgSelectStyle();
    style->setSizeMode(mode);

    // 编辑框
    style->m_editorBackgroundColor = QColor(0x14, 0x14, 0x14);
    style->m_editorForegroundColor = QColor(255, 255, 255, 224);
    style->m_editorPlaceholderColor = QColor(255, 255, 255, 64);
    style->m_editorBorderColor = QColor(0x42, 0x42, 0x42);
    style->m_editorHoverBorderColor = QColor(0x40, 0x96, 0xff);
    style->m_editorActiveBorderColor = QColor(0x16, 0x77, 0xff);
    style->m_editorDisabledBackgroundColor = QColor(255, 255, 255, 20);
    style->m_editorDisabledForegroundColor = QColor(255, 255, 255, 64);
    style->m_editorDisabledBorderColor = QColor(0x42, 0x42, 0x42);

    // chip
    style->m_chipBackgroundColor = QColor(255, 255, 255, 31);
    style->m_chipHoverBackgroundColor = QColor(255, 255, 255, 51);
    style->m_chipForegroundColor = QColor(255, 255, 255, 224);
    style->m_chipBorderColor = QColor();
    style->m_chipRemoveIconColor = QColor(255, 255, 255, 115);
    style->m_chipRemoveIconHoverColor = QColor(255, 255, 255, 224);

    // 面板
    style->m_panelBackgroundColor = QColor(0x1f, 0x1f, 0x1f);
    style->m_panelBorderColor = QColor(255, 255, 255, 31);
    style->m_panelShadowColor = QColor(0, 0, 0, 90);

    // 下拉项
    style->m_itemForegroundColor = QColor(255, 255, 255, 217);
    style->m_itemDisabledForegroundColor = QColor(255, 255, 255, 64);
    style->m_itemHoverBackgroundColor = QColor(255, 255, 255, 20);
    style->m_itemSelectedBackgroundColor = QColor(0x16, 0x3a, 0x5f);
    style->m_itemSelectedForegroundColor = QColor(0x40, 0x96, 0xff);
    style->m_itemHoverSelectedBackgroundColor = QColor(0x1d, 0x4a, 0x78);
    style->m_itemCheckmarkColor = QColor(0x40, 0x96, 0xff);
    style->m_matchHighlightForegroundColor = QColor(0x40, 0x96, 0xff);

    // 组名 / 箭头
    style->m_groupHeaderForegroundColor = QColor(255, 255, 255, 115);
    style->m_arrowColor = QColor(255, 255, 255, 115);

    return style;
}

AgSelectStyle *AgSelectStyle::globalStyle()
{
    if (!s_globalStyle)
        s_globalStyle = new AgSelectStyle();
    return s_globalStyle;
}

void AgSelectStyle::setGlobalStyle(AgSelectStyle *style)
{
    if (s_globalStyle == style)
        return;
    delete s_globalStyle;
    s_globalStyle = style;
    notifyGlobalStyleChanged();
}

AgSelectStyle::SizeMode AgSelectStyle::sizeMode() const
{
    return m_sizeMode;
}

void AgSelectStyle::setSizeMode(SizeMode mode)
{
    m_sizeMode = mode;
    applySizeModePreset(mode);
}

void AgSelectStyle::applySizeModePreset(SizeMode mode)
{
    // 字体：小 12px / 中 14px / 大 16px（保留应用默认字体族）
    int fontPixelSize = 14;
    switch (mode) {
    case SizeMode::Small:
        fontPixelSize = 12;
        m_editorHeight = 24;
        m_editorBorderRadius = 4;
        m_editorHorizontalPadding = 8;
        m_itemHeight = 22;      // 下拉项略小于编辑框，排布更紧凑
        m_itemSpacing = 2;
        m_itemHorizontalPadding = 8;
        m_itemBorderRadius = 3;
        m_groupHeaderHeight = 20;
        m_chipHeight = 18;
        m_chipSpacing = 4;
        m_panelBorderRadius = 4;
        m_arrowSlotWidth = 20;
        break;
    case SizeMode::Large:
        fontPixelSize = 16;
        m_editorHeight = 36;
        m_editorBorderRadius = 8;
        m_editorHorizontalPadding = 12;
        m_itemHeight = 32;
        m_itemSpacing = 2;
        m_itemHorizontalPadding = 12;
        m_itemBorderRadius = 6;
        m_groupHeaderHeight = 28;
        m_chipHeight = 26;
        m_chipSpacing = 6;
        m_panelBorderRadius = 8;
        m_arrowSlotWidth = 28;
        break;
    case SizeMode::Medium:
    default:
        fontPixelSize = 14;
        m_editorHeight = 30;
        m_editorBorderRadius = 6;
        m_editorHorizontalPadding = 10;
        m_itemHeight = 26;
        m_itemSpacing = 2;
        m_itemHorizontalPadding = 10;
        m_itemBorderRadius = 4;
        m_groupHeaderHeight = 24;
        m_chipHeight = 22;
        m_chipSpacing = 4;
        m_panelBorderRadius = 6;
        m_arrowSlotWidth = 24;
        break;
    }

    m_font = QApplication::font();
    m_font.setPixelSize(fontPixelSize);

    m_editorBorderWidth = 1;
    m_itemIndentWidth = 16;
    m_panelMaxHeight = 280;
}

QFont AgSelectStyle::font() const
{
    return m_font;
}

void AgSelectStyle::setFont(const QFont &font)
{
    m_font = font;
}

// ---- 编辑框 ----
int AgSelectStyle::editorHeight() const { return m_editorHeight; }
void AgSelectStyle::setEditorHeight(int height) { m_editorHeight = height; }
QColor AgSelectStyle::editorBackgroundColor() const { return m_editorBackgroundColor; }
void AgSelectStyle::setEditorBackgroundColor(const QColor &color) { m_editorBackgroundColor = color; }
QColor AgSelectStyle::editorForegroundColor() const { return m_editorForegroundColor; }
void AgSelectStyle::setEditorForegroundColor(const QColor &color) { m_editorForegroundColor = color; }
QColor AgSelectStyle::editorPlaceholderColor() const { return m_editorPlaceholderColor; }
void AgSelectStyle::setEditorPlaceholderColor(const QColor &color) { m_editorPlaceholderColor = color; }
QColor AgSelectStyle::editorBorderColor() const { return m_editorBorderColor; }
void AgSelectStyle::setEditorBorderColor(const QColor &color) { m_editorBorderColor = color; }
QColor AgSelectStyle::editorHoverBorderColor() const { return m_editorHoverBorderColor; }
void AgSelectStyle::setEditorHoverBorderColor(const QColor &color) { m_editorHoverBorderColor = color; }
QColor AgSelectStyle::editorActiveBorderColor() const { return m_editorActiveBorderColor; }
void AgSelectStyle::setEditorActiveBorderColor(const QColor &color) { m_editorActiveBorderColor = color; }
QColor AgSelectStyle::editorDisabledBackgroundColor() const { return m_editorDisabledBackgroundColor; }
void AgSelectStyle::setEditorDisabledBackgroundColor(const QColor &color) { m_editorDisabledBackgroundColor = color; }
QColor AgSelectStyle::editorDisabledForegroundColor() const { return m_editorDisabledForegroundColor; }
void AgSelectStyle::setEditorDisabledForegroundColor(const QColor &color) { m_editorDisabledForegroundColor = color; }
QColor AgSelectStyle::editorDisabledBorderColor() const { return m_editorDisabledBorderColor; }
void AgSelectStyle::setEditorDisabledBorderColor(const QColor &color) { m_editorDisabledBorderColor = color; }
int AgSelectStyle::editorBorderRadius() const { return m_editorBorderRadius; }
void AgSelectStyle::setEditorBorderRadius(int radius) { m_editorBorderRadius = radius; }
int AgSelectStyle::editorBorderWidth() const { return m_editorBorderWidth; }
void AgSelectStyle::setEditorBorderWidth(int width) { m_editorBorderWidth = qMax(0, width); }
int AgSelectStyle::editorHorizontalPadding() const { return m_editorHorizontalPadding; }
void AgSelectStyle::setEditorHorizontalPadding(int padding) { m_editorHorizontalPadding = qMax(0, padding); }

// ---- chip ----
QColor AgSelectStyle::chipBackgroundColor() const { return m_chipBackgroundColor; }
void AgSelectStyle::setChipBackgroundColor(const QColor &color) { m_chipBackgroundColor = color; }
QColor AgSelectStyle::chipHoverBackgroundColor() const { return m_chipHoverBackgroundColor; }
void AgSelectStyle::setChipHoverBackgroundColor(const QColor &color) { m_chipHoverBackgroundColor = color; }
QColor AgSelectStyle::chipForegroundColor() const { return m_chipForegroundColor; }
void AgSelectStyle::setChipForegroundColor(const QColor &color) { m_chipForegroundColor = color; }
QColor AgSelectStyle::chipBorderColor() const { return m_chipBorderColor; }
void AgSelectStyle::setChipBorderColor(const QColor &color) { m_chipBorderColor = color; }
QColor AgSelectStyle::chipRemoveIconColor() const { return m_chipRemoveIconColor; }
void AgSelectStyle::setChipRemoveIconColor(const QColor &color) { m_chipRemoveIconColor = color; }
QColor AgSelectStyle::chipRemoveIconHoverColor() const { return m_chipRemoveIconHoverColor; }
void AgSelectStyle::setChipRemoveIconHoverColor(const QColor &color) { m_chipRemoveIconHoverColor = color; }
int AgSelectStyle::chipHeight() const { return m_chipHeight; }
void AgSelectStyle::setChipHeight(int height) { m_chipHeight = qMax(12, height); }
int AgSelectStyle::chipSpacing() const { return m_chipSpacing; }
void AgSelectStyle::setChipSpacing(int spacing) { m_chipSpacing = qMax(0, spacing); }

// ---- 面板 ----
QColor AgSelectStyle::panelBackgroundColor() const { return m_panelBackgroundColor; }
void AgSelectStyle::setPanelBackgroundColor(const QColor &color) { m_panelBackgroundColor = color; }
QColor AgSelectStyle::panelBorderColor() const { return m_panelBorderColor; }
void AgSelectStyle::setPanelBorderColor(const QColor &color) { m_panelBorderColor = color; }
int AgSelectStyle::panelBorderRadius() const { return m_panelBorderRadius; }
void AgSelectStyle::setPanelBorderRadius(int radius) { m_panelBorderRadius = qMax(0, radius); }
QColor AgSelectStyle::panelShadowColor() const { return m_panelShadowColor; }
void AgSelectStyle::setPanelShadowColor(const QColor &color) { m_panelShadowColor = color; }
int AgSelectStyle::panelShadowRadius() const { return m_panelShadowRadius; }
void AgSelectStyle::setPanelShadowRadius(int radius) { m_panelShadowRadius = qMax(0, radius); }
int AgSelectStyle::panelMaxHeight() const { return m_panelMaxHeight; }
void AgSelectStyle::setPanelMaxHeight(int height) { m_panelMaxHeight = qMax(40, height); }

// ---- 下拉项 ----
int AgSelectStyle::itemHeight() const { return m_itemHeight; }
void AgSelectStyle::setItemHeight(int height) { m_itemHeight = qMax(16, height); }
int AgSelectStyle::itemSpacing() const { return m_itemSpacing; }
void AgSelectStyle::setItemSpacing(int spacing) { m_itemSpacing = qMax(0, spacing); }
int AgSelectStyle::itemHorizontalPadding() const { return m_itemHorizontalPadding; }
void AgSelectStyle::setItemHorizontalPadding(int padding) { m_itemHorizontalPadding = qMax(0, padding); }
int AgSelectStyle::itemIndentWidth() const { return m_itemIndentWidth; }
void AgSelectStyle::setItemIndentWidth(int width) { m_itemIndentWidth = qMax(0, width); }
int AgSelectStyle::itemBorderRadius() const { return m_itemBorderRadius; }
void AgSelectStyle::setItemBorderRadius(int radius) { m_itemBorderRadius = qMax(0, radius); }
QColor AgSelectStyle::itemForegroundColor() const { return m_itemForegroundColor; }
void AgSelectStyle::setItemForegroundColor(const QColor &color) { m_itemForegroundColor = color; }
QColor AgSelectStyle::itemDisabledForegroundColor() const { return m_itemDisabledForegroundColor; }
void AgSelectStyle::setItemDisabledForegroundColor(const QColor &color) { m_itemDisabledForegroundColor = color; }
QColor AgSelectStyle::itemHoverBackgroundColor() const { return m_itemHoverBackgroundColor; }
void AgSelectStyle::setItemHoverBackgroundColor(const QColor &color) { m_itemHoverBackgroundColor = color; }
QColor AgSelectStyle::itemSelectedBackgroundColor() const { return m_itemSelectedBackgroundColor; }
void AgSelectStyle::setItemSelectedBackgroundColor(const QColor &color) { m_itemSelectedBackgroundColor = color; }
QColor AgSelectStyle::itemSelectedForegroundColor() const { return m_itemSelectedForegroundColor; }
void AgSelectStyle::setItemSelectedForegroundColor(const QColor &color) { m_itemSelectedForegroundColor = color; }
QColor AgSelectStyle::itemHoverSelectedBackgroundColor() const { return m_itemHoverSelectedBackgroundColor; }
void AgSelectStyle::setItemHoverSelectedBackgroundColor(const QColor &color) { m_itemHoverSelectedBackgroundColor = color; }
QColor AgSelectStyle::itemCheckmarkColor() const { return m_itemCheckmarkColor; }
void AgSelectStyle::setItemCheckmarkColor(const QColor &color) { m_itemCheckmarkColor = color; }
QColor AgSelectStyle::matchHighlightForegroundColor() const { return m_matchHighlightForegroundColor; }
void AgSelectStyle::setMatchHighlightForegroundColor(const QColor &color) { m_matchHighlightForegroundColor = color; }

// ---- 组名 ----
int AgSelectStyle::groupHeaderHeight() const { return m_groupHeaderHeight; }
void AgSelectStyle::setGroupHeaderHeight(int height) { m_groupHeaderHeight = qMax(16, height); }
QColor AgSelectStyle::groupHeaderForegroundColor() const { return m_groupHeaderForegroundColor; }
void AgSelectStyle::setGroupHeaderForegroundColor(const QColor &color) { m_groupHeaderForegroundColor = color; }

// ---- 箭头 ----
QColor AgSelectStyle::arrowColor() const { return m_arrowColor; }
void AgSelectStyle::setArrowColor(const QColor &color) { m_arrowColor = color; }
int AgSelectStyle::arrowSlotWidth() const { return m_arrowSlotWidth; }
void AgSelectStyle::setArrowSlotWidth(int width) { m_arrowSlotWidth = qMax(12, width); }

void AgSelectStyle::registerGlobalObserver(AgSelect *widget)
{
    if (widget && !s_observers.contains(widget))
        s_observers.append(widget);
}

void AgSelectStyle::unregisterGlobalObserver(AgSelect *widget)
{
    s_observers.removeOne(widget);
}

void AgSelectStyle::notifyGlobalStyleChanged()
{
    // 拷贝一份再遍历：观察者可能在回调中销毁或重新注册
    const QVector<AgSelect *> observers = s_observers;
    for (AgSelect *widget : observers) {
        if (widget)
            widget->onGlobalStyleChanged();
    }
}
