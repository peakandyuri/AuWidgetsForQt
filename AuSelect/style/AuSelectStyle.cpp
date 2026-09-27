#include "AuSelectStyle.h"

#include "AuSelect.h"

#include <QApplication>
#include <QtGlobal>

AuSelectStyle *AuSelectStyle::s_globalStyle = nullptr;
QVector<AuSelect *> AuSelectStyle::s_observers;

AuSelectStyle::AuSelectStyle()
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
    m_editorFocusBorderColor = QColor(0x16, 0x77, 0xff);
    m_editorFocusBackgroundColor = QColor(0x16, 0x77, 0xff, 26);
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

    // 清除图标
    m_clearIconCircleColor = QColor(0, 0, 0, 96);
    m_clearIconCircleHoverColor = QColor(0, 0, 0, 150);
    m_clearIconCrossColor = QColor(Qt::white);
}

AuSelectStyle::~AuSelectStyle()
{
}

AuSelectStyle *AuSelectStyle::createLightStyle(SizeMode mode)
{
    AuSelectStyle *style = new AuSelectStyle();
    style->setSizeMode(mode);
    return style;
}

AuSelectStyle *AuSelectStyle::createDarkStyle(SizeMode mode)
{
    AuSelectStyle *style = new AuSelectStyle();
    style->setSizeMode(mode);

    // 编辑框
    style->m_editorBackgroundColor = QColor(0x14, 0x14, 0x14);
    style->m_editorForegroundColor = QColor(255, 255, 255, 224);
    style->m_editorPlaceholderColor = QColor(255, 255, 255, 64);
    style->m_editorBorderColor = QColor(0x42, 0x42, 0x42);
    style->m_editorHoverBorderColor = QColor(0x40, 0x96, 0xff);
    style->m_editorActiveBorderColor = QColor(0x16, 0x77, 0xff);
    style->m_editorFocusBorderColor = QColor(0x40, 0x96, 0xff);
    style->m_editorFocusBackgroundColor = QColor(0x40, 0x96, 0xff, 42);
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

    // 清除图标
    style->m_clearIconCircleColor = QColor(255, 255, 255, 96);
    style->m_clearIconCircleHoverColor = QColor(255, 255, 255, 150);
    style->m_clearIconCrossColor = QColor(0x14, 0x14, 0x14);

    return style;
}

AuSelectStyle *AuSelectStyle::globalStyle()
{
    if (!s_globalStyle)
        s_globalStyle = new AuSelectStyle();
    return s_globalStyle;
}

void AuSelectStyle::setGlobalStyle(AuSelectStyle *style)
{
    if (s_globalStyle == style)
        return;
    delete s_globalStyle;
    s_globalStyle = style;
    notifyGlobalStyleChanged();
}

AuSelectStyle::SizeMode AuSelectStyle::sizeMode() const
{
    return m_sizeMode;
}

void AuSelectStyle::setSizeMode(SizeMode mode)
{
    m_sizeMode = mode;
    applySizeModePreset(mode);
}

void AuSelectStyle::applySizeModePreset(SizeMode mode)
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
    m_font.setFamily(QStringLiteral("Microsoft YaHei")); // Au 控件默认字体族
    m_font.setPixelSize(fontPixelSize);

    m_editorBorderWidth = 1;
    m_itemIndentWidth = 16;
    m_panelMaxHeight = 280;
}

QFont AuSelectStyle::font() const
{
    return m_font;
}

void AuSelectStyle::setFont(const QFont &font)
{
    m_font = font;
}

// ---- 编辑框 ----
int AuSelectStyle::editorHeight() const { return m_editorHeight; }
void AuSelectStyle::setEditorHeight(int height) { m_editorHeight = height; }
QColor AuSelectStyle::editorBackgroundColor() const { return m_editorBackgroundColor; }
void AuSelectStyle::setEditorBackgroundColor(const QColor &color) { m_editorBackgroundColor = color; }
QColor AuSelectStyle::editorForegroundColor() const { return m_editorForegroundColor; }
void AuSelectStyle::setEditorForegroundColor(const QColor &color) { m_editorForegroundColor = color; }
QColor AuSelectStyle::editorPlaceholderColor() const { return m_editorPlaceholderColor; }
void AuSelectStyle::setEditorPlaceholderColor(const QColor &color) { m_editorPlaceholderColor = color; }
QColor AuSelectStyle::editorBorderColor() const { return m_editorBorderColor; }
void AuSelectStyle::setEditorBorderColor(const QColor &color) { m_editorBorderColor = color; }
QColor AuSelectStyle::editorHoverBorderColor() const { return m_editorHoverBorderColor; }
void AuSelectStyle::setEditorHoverBorderColor(const QColor &color) { m_editorHoverBorderColor = color; }
QColor AuSelectStyle::editorActiveBorderColor() const { return m_editorActiveBorderColor; }
void AuSelectStyle::setEditorActiveBorderColor(const QColor &color) { m_editorActiveBorderColor = color; }
QColor AuSelectStyle::editorFocusBorderColor() const { return m_editorFocusBorderColor; }
void AuSelectStyle::setEditorFocusBorderColor(const QColor &color) { m_editorFocusBorderColor = color; }
QColor AuSelectStyle::editorFocusBackgroundColor() const { return m_editorFocusBackgroundColor; }
void AuSelectStyle::setEditorFocusBackgroundColor(const QColor &color) { m_editorFocusBackgroundColor = color; }
QColor AuSelectStyle::editorDisabledBackgroundColor() const { return m_editorDisabledBackgroundColor; }
void AuSelectStyle::setEditorDisabledBackgroundColor(const QColor &color) { m_editorDisabledBackgroundColor = color; }
QColor AuSelectStyle::editorDisabledForegroundColor() const { return m_editorDisabledForegroundColor; }
void AuSelectStyle::setEditorDisabledForegroundColor(const QColor &color) { m_editorDisabledForegroundColor = color; }
QColor AuSelectStyle::editorDisabledBorderColor() const { return m_editorDisabledBorderColor; }
void AuSelectStyle::setEditorDisabledBorderColor(const QColor &color) { m_editorDisabledBorderColor = color; }
int AuSelectStyle::editorBorderRadius() const { return m_editorBorderRadius; }
void AuSelectStyle::setEditorBorderRadius(int radius) { m_editorBorderRadius = radius; }
int AuSelectStyle::editorBorderWidth() const { return m_editorBorderWidth; }
void AuSelectStyle::setEditorBorderWidth(int width) { m_editorBorderWidth = qMax(0, width); }
int AuSelectStyle::editorHorizontalPadding() const { return m_editorHorizontalPadding; }
void AuSelectStyle::setEditorHorizontalPadding(int padding) { m_editorHorizontalPadding = qMax(0, padding); }

// ---- chip ----
QColor AuSelectStyle::chipBackgroundColor() const { return m_chipBackgroundColor; }
void AuSelectStyle::setChipBackgroundColor(const QColor &color) { m_chipBackgroundColor = color; }
QColor AuSelectStyle::chipHoverBackgroundColor() const { return m_chipHoverBackgroundColor; }
void AuSelectStyle::setChipHoverBackgroundColor(const QColor &color) { m_chipHoverBackgroundColor = color; }
QColor AuSelectStyle::chipForegroundColor() const { return m_chipForegroundColor; }
void AuSelectStyle::setChipForegroundColor(const QColor &color) { m_chipForegroundColor = color; }
QColor AuSelectStyle::chipBorderColor() const { return m_chipBorderColor; }
void AuSelectStyle::setChipBorderColor(const QColor &color) { m_chipBorderColor = color; }
QColor AuSelectStyle::chipRemoveIconColor() const { return m_chipRemoveIconColor; }
void AuSelectStyle::setChipRemoveIconColor(const QColor &color) { m_chipRemoveIconColor = color; }
QColor AuSelectStyle::chipRemoveIconHoverColor() const { return m_chipRemoveIconHoverColor; }
void AuSelectStyle::setChipRemoveIconHoverColor(const QColor &color) { m_chipRemoveIconHoverColor = color; }
int AuSelectStyle::chipHeight() const { return m_chipHeight; }
void AuSelectStyle::setChipHeight(int height) { m_chipHeight = qMax(12, height); }
int AuSelectStyle::chipSpacing() const { return m_chipSpacing; }
void AuSelectStyle::setChipSpacing(int spacing) { m_chipSpacing = qMax(0, spacing); }

// ---- 面板 ----
QColor AuSelectStyle::panelBackgroundColor() const { return m_panelBackgroundColor; }
void AuSelectStyle::setPanelBackgroundColor(const QColor &color) { m_panelBackgroundColor = color; }
QColor AuSelectStyle::panelBorderColor() const { return m_panelBorderColor; }
void AuSelectStyle::setPanelBorderColor(const QColor &color) { m_panelBorderColor = color; }
int AuSelectStyle::panelBorderRadius() const { return m_panelBorderRadius; }
void AuSelectStyle::setPanelBorderRadius(int radius) { m_panelBorderRadius = qMax(0, radius); }
QColor AuSelectStyle::panelShadowColor() const { return m_panelShadowColor; }
void AuSelectStyle::setPanelShadowColor(const QColor &color) { m_panelShadowColor = color; }
int AuSelectStyle::panelShadowRadius() const { return m_panelShadowRadius; }
void AuSelectStyle::setPanelShadowRadius(int radius) { m_panelShadowRadius = qMax(0, radius); }
int AuSelectStyle::panelMaxHeight() const { return m_panelMaxHeight; }
void AuSelectStyle::setPanelMaxHeight(int height) { m_panelMaxHeight = qMax(40, height); }

// ---- 下拉项 ----
int AuSelectStyle::itemHeight() const { return m_itemHeight; }
void AuSelectStyle::setItemHeight(int height) { m_itemHeight = qMax(16, height); }
int AuSelectStyle::itemSpacing() const { return m_itemSpacing; }
void AuSelectStyle::setItemSpacing(int spacing) { m_itemSpacing = qMax(0, spacing); }
int AuSelectStyle::itemHorizontalPadding() const { return m_itemHorizontalPadding; }
void AuSelectStyle::setItemHorizontalPadding(int padding) { m_itemHorizontalPadding = qMax(0, padding); }
int AuSelectStyle::itemIndentWidth() const { return m_itemIndentWidth; }
void AuSelectStyle::setItemIndentWidth(int width) { m_itemIndentWidth = qMax(0, width); }
int AuSelectStyle::itemBorderRadius() const { return m_itemBorderRadius; }
void AuSelectStyle::setItemBorderRadius(int radius) { m_itemBorderRadius = qMax(0, radius); }
QColor AuSelectStyle::itemForegroundColor() const { return m_itemForegroundColor; }
void AuSelectStyle::setItemForegroundColor(const QColor &color) { m_itemForegroundColor = color; }
QColor AuSelectStyle::itemDisabledForegroundColor() const { return m_itemDisabledForegroundColor; }
void AuSelectStyle::setItemDisabledForegroundColor(const QColor &color) { m_itemDisabledForegroundColor = color; }
QColor AuSelectStyle::itemHoverBackgroundColor() const { return m_itemHoverBackgroundColor; }
void AuSelectStyle::setItemHoverBackgroundColor(const QColor &color) { m_itemHoverBackgroundColor = color; }
QColor AuSelectStyle::itemSelectedBackgroundColor() const { return m_itemSelectedBackgroundColor; }
void AuSelectStyle::setItemSelectedBackgroundColor(const QColor &color) { m_itemSelectedBackgroundColor = color; }
QColor AuSelectStyle::itemSelectedForegroundColor() const { return m_itemSelectedForegroundColor; }
void AuSelectStyle::setItemSelectedForegroundColor(const QColor &color) { m_itemSelectedForegroundColor = color; }
QColor AuSelectStyle::itemHoverSelectedBackgroundColor() const { return m_itemHoverSelectedBackgroundColor; }
void AuSelectStyle::setItemHoverSelectedBackgroundColor(const QColor &color) { m_itemHoverSelectedBackgroundColor = color; }
QColor AuSelectStyle::itemCheckmarkColor() const { return m_itemCheckmarkColor; }
void AuSelectStyle::setItemCheckmarkColor(const QColor &color) { m_itemCheckmarkColor = color; }
QColor AuSelectStyle::matchHighlightForegroundColor() const { return m_matchHighlightForegroundColor; }
void AuSelectStyle::setMatchHighlightForegroundColor(const QColor &color) { m_matchHighlightForegroundColor = color; }

// ---- 组名 ----
int AuSelectStyle::groupHeaderHeight() const { return m_groupHeaderHeight; }
void AuSelectStyle::setGroupHeaderHeight(int height) { m_groupHeaderHeight = qMax(16, height); }
QColor AuSelectStyle::groupHeaderForegroundColor() const { return m_groupHeaderForegroundColor; }
void AuSelectStyle::setGroupHeaderForegroundColor(const QColor &color) { m_groupHeaderForegroundColor = color; }

// ---- 箭头 ----
QColor AuSelectStyle::arrowColor() const { return m_arrowColor; }
void AuSelectStyle::setArrowColor(const QColor &color) { m_arrowColor = color; }
int AuSelectStyle::arrowSlotWidth() const { return m_arrowSlotWidth; }
void AuSelectStyle::setArrowSlotWidth(int width) { m_arrowSlotWidth = qMax(12, width); }

// ---- 清除图标 ----
QColor AuSelectStyle::clearIconCircleColor() const { return m_clearIconCircleColor; }
void AuSelectStyle::setClearIconCircleColor(const QColor &color) { m_clearIconCircleColor = color; }
QColor AuSelectStyle::clearIconCircleHoverColor() const { return m_clearIconCircleHoverColor; }
void AuSelectStyle::setClearIconCircleHoverColor(const QColor &color) { m_clearIconCircleHoverColor = color; }
QColor AuSelectStyle::clearIconCrossColor() const { return m_clearIconCrossColor; }
void AuSelectStyle::setClearIconCrossColor(const QColor &color) { m_clearIconCrossColor = color; }

void AuSelectStyle::registerGlobalObserver(AuSelect *widget)
{
    if (widget && !s_observers.contains(widget))
        s_observers.append(widget);
}

void AuSelectStyle::unregisterGlobalObserver(AuSelect *widget)
{
    s_observers.removeOne(widget);
}

void AuSelectStyle::notifyGlobalStyleChanged()
{
    // 拷贝一份再遍历：观察者可能在回调中销毁或重新注册
    const QVector<AuSelect *> observers = s_observers;
    for (AuSelect *widget : observers) {
        if (widget)
            widget->onGlobalStyleChanged();
    }
}
