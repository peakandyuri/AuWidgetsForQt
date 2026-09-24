#include "AgDatePickerStyle.h"

#include "AgDatePicker.h"

#include <QApplication>
#include <QtGlobal>

AgDatePickerStyle *AgDatePickerStyle::s_globalStyle = nullptr;
QVector<AgDatePicker *> AgDatePickerStyle::s_observers;

AgDatePickerStyle::AgDatePickerStyle()
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

    // 日历 / 清除图标
    m_calendarIconColor = QColor(0, 0, 0, 115);
    m_clearIconCircleColor = QColor(0, 0, 0, 96);
    m_clearIconCircleHoverColor = QColor(0, 0, 0, 150);
    m_clearIconCrossColor = QColor(Qt::white);

    // 面板
    m_panelBackgroundColor = QColor(Qt::white);
    m_panelBorderColor = QColor(5, 5, 5, 15);
    m_panelShadowColor = QColor(0, 0, 0, 45);
    m_panelShadowRadius = 10;

    // 面板头部
    m_navIconColor = QColor(0, 0, 0, 165);
    m_navIconDisabledColor = QColor(0, 0, 0, 45);
    m_navHoverBackgroundColor = QColor(0, 0, 0, 16);
    m_headTitleColor = QColor(0, 0, 0, 224);
    m_headTitleHoverColor = QColor(0x16, 0x77, 0xff);
    m_headLabelColor = QColor(0, 0, 0, 165);

    // 星期行
    m_weekdayForegroundColor = QColor(0, 0, 0, 115);

    // 格子
    m_cellForegroundColor = QColor(0, 0, 0, 224);
    m_cellOtherMonthForegroundColor = QColor(0, 0, 0, 88);
    m_cellDisabledForegroundColor = QColor(0, 0, 0, 40);
    m_cellHoverBackgroundColor = QColor(0, 0, 0, 16);
    m_cellSelectedBackgroundColor = QColor(0x16, 0x77, 0xff);
    m_cellSelectedForegroundColor = QColor(Qt::white);
    m_todayForegroundColor = QColor(0x16, 0x77, 0xff);
    m_cellInRangeBackgroundColor = QColor(0x16, 0x77, 0xff, 33);

    // footer
    m_footerTopLineColor = QColor(0, 0, 0, 20);
    m_footerLinkColor = QColor(0x16, 0x77, 0xff);
    m_footerLinkHoverColor = QColor(0x40, 0x96, 0xff);
    m_footerLinkDisabledColor = QColor(0, 0, 0, 40);
}

AgDatePickerStyle::~AgDatePickerStyle()
{
}

AgDatePickerStyle *AgDatePickerStyle::createLightStyle(SizeMode mode)
{
    AgDatePickerStyle *style = new AgDatePickerStyle();
    style->setSizeMode(mode);
    return style;
}

AgDatePickerStyle *AgDatePickerStyle::createDarkStyle(SizeMode mode)
{
    AgDatePickerStyle *style = new AgDatePickerStyle();
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

    // 日历 / 清除图标
    style->m_calendarIconColor = QColor(255, 255, 255, 115);
    style->m_clearIconCircleColor = QColor(255, 255, 255, 96);
    style->m_clearIconCircleHoverColor = QColor(255, 255, 255, 150);
    style->m_clearIconCrossColor = QColor(0x14, 0x14, 0x14);

    // 面板
    style->m_panelBackgroundColor = QColor(0x1f, 0x1f, 0x1f);
    style->m_panelBorderColor = QColor(255, 255, 255, 31);
    style->m_panelShadowColor = QColor(0, 0, 0, 90);

    // 面板头部
    style->m_navIconColor = QColor(255, 255, 255, 165);
    style->m_navIconDisabledColor = QColor(255, 255, 255, 45);
    style->m_navHoverBackgroundColor = QColor(255, 255, 255, 26);
    style->m_headTitleColor = QColor(255, 255, 255, 224);
    style->m_headTitleHoverColor = QColor(0x40, 0x96, 0xff);
    style->m_headLabelColor = QColor(255, 255, 255, 165);

    // 星期行
    style->m_weekdayForegroundColor = QColor(255, 255, 255, 115);

    // 格子
    style->m_cellForegroundColor = QColor(255, 255, 255, 217);
    style->m_cellOtherMonthForegroundColor = QColor(255, 255, 255, 88);
    style->m_cellDisabledForegroundColor = QColor(255, 255, 255, 40);
    style->m_cellHoverBackgroundColor = QColor(255, 255, 255, 26);
    style->m_cellSelectedBackgroundColor = QColor(0x16, 0x77, 0xff);
    style->m_cellSelectedForegroundColor = QColor(Qt::white);
    style->m_todayForegroundColor = QColor(0x40, 0x96, 0xff);
    style->m_cellInRangeBackgroundColor = QColor(0x40, 0x96, 0xff, 46);

    // footer
    style->m_footerTopLineColor = QColor(255, 255, 255, 26);
    style->m_footerLinkColor = QColor(0x40, 0x96, 0xff);
    style->m_footerLinkHoverColor = QColor(0x70, 0xb8, 0xff);
    style->m_footerLinkDisabledColor = QColor(255, 255, 255, 40);

    return style;
}

AgDatePickerStyle *AgDatePickerStyle::globalStyle()
{
    if (!s_globalStyle)
        s_globalStyle = new AgDatePickerStyle();
    return s_globalStyle;
}

void AgDatePickerStyle::setGlobalStyle(AgDatePickerStyle *style)
{
    if (s_globalStyle == style)
        return;
    delete s_globalStyle;
    s_globalStyle = style;
    notifyGlobalStyleChanged();
}

AgDatePickerStyle::SizeMode AgDatePickerStyle::sizeMode() const
{
    return m_sizeMode;
}

void AgDatePickerStyle::setSizeMode(SizeMode mode)
{
    m_sizeMode = mode;
    applySizeModePreset(mode);
}

void AgDatePickerStyle::applySizeModePreset(SizeMode mode)
{
    // 字体：小 12px / 中 14px / 大 16px，默认字体族微软雅黑（Ag 控件统一约定）
    int fontPixelSize = 14;
    switch (mode) {
    case SizeMode::Small:
        fontPixelSize = 12;
        m_editorHeight = 24;
        m_editorBorderRadius = 4;
        m_editorHorizontalPadding = 8;
        m_iconSlotWidth = 20;
        m_panelBorderRadius = 4;
        m_panelHorizontalPadding = 8;
        m_headHeight = 32;
        m_navButtonSize = 20;
        m_weekdayRowHeight = 22;
        m_dayCellWidth = 24;
        m_dayCellHeight = 22;
        m_cellSpacing = 2;
        m_cellRadius = 3;
        m_footerHeight = 30;
        break;
    case SizeMode::Large:
        fontPixelSize = 16;
        m_editorHeight = 36;
        m_editorBorderRadius = 8;
        m_editorHorizontalPadding = 12;
        m_iconSlotWidth = 28;
        m_panelBorderRadius = 8;
        m_panelHorizontalPadding = 12;
        m_headHeight = 44;
        m_navButtonSize = 28;
        m_weekdayRowHeight = 30;
        m_dayCellWidth = 36;
        m_dayCellHeight = 32;
        m_cellSpacing = 2;
        m_cellRadius = 6;
        m_footerHeight = 40;
        break;
    case SizeMode::Medium:
    default:
        fontPixelSize = 14;
        m_editorHeight = 30;
        m_editorBorderRadius = 6;
        m_editorHorizontalPadding = 10;
        m_iconSlotWidth = 24;
        m_panelBorderRadius = 6;
        m_panelHorizontalPadding = 10;
        m_headHeight = 38;
        m_navButtonSize = 24;
        m_weekdayRowHeight = 26;
        m_dayCellWidth = 30;
        m_dayCellHeight = 28;
        m_cellSpacing = 2;
        m_cellRadius = 4;
        m_footerHeight = 36;
        break;
    }

    m_font = QApplication::font();
    m_font.setFamily(QStringLiteral("Microsoft YaHei"));
    m_font.setPixelSize(fontPixelSize);

    m_editorBorderWidth = 1;
    m_panelShadowRadius = 10;
}

QFont AgDatePickerStyle::font() const
{
    return m_font;
}

void AgDatePickerStyle::setFont(const QFont &font)
{
    m_font = font;
}

// ---- 编辑框 ----
int AgDatePickerStyle::editorHeight() const { return m_editorHeight; }
void AgDatePickerStyle::setEditorHeight(int height) { m_editorHeight = height; }
QColor AgDatePickerStyle::editorBackgroundColor() const { return m_editorBackgroundColor; }
void AgDatePickerStyle::setEditorBackgroundColor(const QColor &color) { m_editorBackgroundColor = color; }
QColor AgDatePickerStyle::editorForegroundColor() const { return m_editorForegroundColor; }
void AgDatePickerStyle::setEditorForegroundColor(const QColor &color) { m_editorForegroundColor = color; }
QColor AgDatePickerStyle::editorPlaceholderColor() const { return m_editorPlaceholderColor; }
void AgDatePickerStyle::setEditorPlaceholderColor(const QColor &color) { m_editorPlaceholderColor = color; }
QColor AgDatePickerStyle::editorBorderColor() const { return m_editorBorderColor; }
void AgDatePickerStyle::setEditorBorderColor(const QColor &color) { m_editorBorderColor = color; }
QColor AgDatePickerStyle::editorHoverBorderColor() const { return m_editorHoverBorderColor; }
void AgDatePickerStyle::setEditorHoverBorderColor(const QColor &color) { m_editorHoverBorderColor = color; }
QColor AgDatePickerStyle::editorActiveBorderColor() const { return m_editorActiveBorderColor; }
void AgDatePickerStyle::setActiveBorderColor(const QColor &color) { m_editorActiveBorderColor = color; }
QColor AgDatePickerStyle::editorFocusBorderColor() const { return m_editorFocusBorderColor; }
void AgDatePickerStyle::setEditorFocusBorderColor(const QColor &color) { m_editorFocusBorderColor = color; }
QColor AgDatePickerStyle::editorFocusBackgroundColor() const { return m_editorFocusBackgroundColor; }
void AgDatePickerStyle::setEditorFocusBackgroundColor(const QColor &color) { m_editorFocusBackgroundColor = color; }
QColor AgDatePickerStyle::editorDisabledBackgroundColor() const { return m_editorDisabledBackgroundColor; }
void AgDatePickerStyle::setEditorDisabledBackgroundColor(const QColor &color) { m_editorDisabledBackgroundColor = color; }
QColor AgDatePickerStyle::editorDisabledForegroundColor() const { return m_editorDisabledForegroundColor; }
void AgDatePickerStyle::setEditorDisabledForegroundColor(const QColor &color) { m_editorDisabledForegroundColor = color; }
QColor AgDatePickerStyle::editorDisabledBorderColor() const { return m_editorDisabledBorderColor; }
void AgDatePickerStyle::setEditorDisabledBorderColor(const QColor &color) { m_editorDisabledBorderColor = color; }
int AgDatePickerStyle::editorBorderRadius() const { return m_editorBorderRadius; }
void AgDatePickerStyle::setEditorBorderRadius(int radius) { m_editorBorderRadius = qMax(0, radius); }
int AgDatePickerStyle::editorBorderWidth() const { return m_editorBorderWidth; }
void AgDatePickerStyle::setEditorBorderWidth(int width) { m_editorBorderWidth = qMax(0, width); }
int AgDatePickerStyle::editorHorizontalPadding() const { return m_editorHorizontalPadding; }
void AgDatePickerStyle::setEditorHorizontalPadding(int padding) { m_editorHorizontalPadding = qMax(0, padding); }
int AgDatePickerStyle::iconSlotWidth() const { return m_iconSlotWidth; }
void AgDatePickerStyle::setIconSlotWidth(int width) { m_iconSlotWidth = qMax(12, width); }

// ---- 日历 / 清除图标 ----
QColor AgDatePickerStyle::calendarIconColor() const { return m_calendarIconColor; }
void AgDatePickerStyle::setCalendarIconColor(const QColor &color) { m_calendarIconColor = color; }
QColor AgDatePickerStyle::clearIconCircleColor() const { return m_clearIconCircleColor; }
void AgDatePickerStyle::setClearIconCircleColor(const QColor &color) { m_clearIconCircleColor = color; }
QColor AgDatePickerStyle::clearIconCircleHoverColor() const { return m_clearIconCircleHoverColor; }
void AgDatePickerStyle::setClearIconCircleHoverColor(const QColor &color) { m_clearIconCircleHoverColor = color; }
QColor AgDatePickerStyle::clearIconCrossColor() const { return m_clearIconCrossColor; }
void AgDatePickerStyle::setClearIconCrossColor(const QColor &color) { m_clearIconCrossColor = color; }

// ---- 面板 ----
QColor AgDatePickerStyle::panelBackgroundColor() const { return m_panelBackgroundColor; }
void AgDatePickerStyle::setPanelBackgroundColor(const QColor &color) { m_panelBackgroundColor = color; }
QColor AgDatePickerStyle::panelBorderColor() const { return m_panelBorderColor; }
void AgDatePickerStyle::setPanelBorderColor(const QColor &color) { m_panelBorderColor = color; }
int AgDatePickerStyle::panelBorderRadius() const { return m_panelBorderRadius; }
void AgDatePickerStyle::setPanelBorderRadius(int radius) { m_panelBorderRadius = qMax(0, radius); }
QColor AgDatePickerStyle::panelShadowColor() const { return m_panelShadowColor; }
void AgDatePickerStyle::setPanelShadowColor(const QColor &color) { m_panelShadowColor = color; }
int AgDatePickerStyle::panelShadowRadius() const { return m_panelShadowRadius; }
void AgDatePickerStyle::setPanelShadowRadius(int radius) { m_panelShadowRadius = qMax(0, radius); }
int AgDatePickerStyle::panelHorizontalPadding() const { return m_panelHorizontalPadding; }
void AgDatePickerStyle::setPanelHorizontalPadding(int padding) { m_panelHorizontalPadding = qMax(0, padding); }

// ---- 面板头部 ----
int AgDatePickerStyle::headHeight() const { return m_headHeight; }
void AgDatePickerStyle::setHeadHeight(int height) { m_headHeight = qMax(16, height); }
int AgDatePickerStyle::navButtonSize() const { return m_navButtonSize; }
void AgDatePickerStyle::setNavButtonSize(int size) { m_navButtonSize = qMax(12, size); }
QColor AgDatePickerStyle::navIconColor() const { return m_navIconColor; }
void AgDatePickerStyle::setNavIconColor(const QColor &color) { m_navIconColor = color; }
QColor AgDatePickerStyle::navIconDisabledColor() const { return m_navIconDisabledColor; }
void AgDatePickerStyle::setNavIconDisabledColor(const QColor &color) { m_navIconDisabledColor = color; }
QColor AgDatePickerStyle::navHoverBackgroundColor() const { return m_navHoverBackgroundColor; }
void AgDatePickerStyle::setNavHoverBackgroundColor(const QColor &color) { m_navHoverBackgroundColor = color; }
QColor AgDatePickerStyle::headTitleColor() const { return m_headTitleColor; }
void AgDatePickerStyle::setHeadTitleColor(const QColor &color) { m_headTitleColor = color; }
QColor AgDatePickerStyle::headTitleHoverColor() const { return m_headTitleHoverColor; }
void AgDatePickerStyle::setHeadTitleHoverColor(const QColor &color) { m_headTitleHoverColor = color; }
QColor AgDatePickerStyle::headLabelColor() const { return m_headLabelColor; }
void AgDatePickerStyle::setHeadLabelColor(const QColor &color) { m_headLabelColor = color; }

// ---- 星期行 ----
int AgDatePickerStyle::weekdayRowHeight() const { return m_weekdayRowHeight; }
void AgDatePickerStyle::setWeekdayRowHeight(int height) { m_weekdayRowHeight = qMax(12, height); }
QColor AgDatePickerStyle::weekdayForegroundColor() const { return m_weekdayForegroundColor; }
void AgDatePickerStyle::setWeekdayForegroundColor(const QColor &color) { m_weekdayForegroundColor = color; }

// ---- 格子 ----
int AgDatePickerStyle::dayCellWidth() const { return m_dayCellWidth; }
void AgDatePickerStyle::setDayCellWidth(int width) { m_dayCellWidth = qMax(16, width); }
int AgDatePickerStyle::dayCellHeight() const { return m_dayCellHeight; }
void AgDatePickerStyle::setDayCellHeight(int height) { m_dayCellHeight = qMax(14, height); }
int AgDatePickerStyle::cellSpacing() const { return m_cellSpacing; }
void AgDatePickerStyle::setCellSpacing(int spacing) { m_cellSpacing = qMax(0, spacing); }
int AgDatePickerStyle::cellRadius() const { return m_cellRadius; }
void AgDatePickerStyle::setCellRadius(int radius) { m_cellRadius = qMax(0, radius); }
QColor AgDatePickerStyle::cellForegroundColor() const { return m_cellForegroundColor; }
void AgDatePickerStyle::setCellForegroundColor(const QColor &color) { m_cellForegroundColor = color; }
QColor AgDatePickerStyle::cellOtherMonthForegroundColor() const { return m_cellOtherMonthForegroundColor; }
void AgDatePickerStyle::setCellOtherMonthForegroundColor(const QColor &color) { m_cellOtherMonthForegroundColor = color; }
QColor AgDatePickerStyle::cellDisabledForegroundColor() const { return m_cellDisabledForegroundColor; }
void AgDatePickerStyle::setCellDisabledForegroundColor(const QColor &color) { m_cellDisabledForegroundColor = color; }
QColor AgDatePickerStyle::cellHoverBackgroundColor() const { return m_cellHoverBackgroundColor; }
void AgDatePickerStyle::setCellHoverBackgroundColor(const QColor &color) { m_cellHoverBackgroundColor = color; }
QColor AgDatePickerStyle::cellSelectedBackgroundColor() const { return m_cellSelectedBackgroundColor; }
void AgDatePickerStyle::setCellSelectedBackgroundColor(const QColor &color) { m_cellSelectedBackgroundColor = color; }
QColor AgDatePickerStyle::cellSelectedForegroundColor() const { return m_cellSelectedForegroundColor; }
void AgDatePickerStyle::setCellSelectedForegroundColor(const QColor &color) { m_cellSelectedForegroundColor = color; }
QColor AgDatePickerStyle::todayForegroundColor() const { return m_todayForegroundColor; }
void AgDatePickerStyle::setTodayForegroundColor(const QColor &color) { m_todayForegroundColor = color; }
QColor AgDatePickerStyle::cellInRangeBackgroundColor() const { return m_cellInRangeBackgroundColor; }
void AgDatePickerStyle::setCellInRangeBackgroundColor(const QColor &color) { m_cellInRangeBackgroundColor = color; }

// ---- footer ----
int AgDatePickerStyle::footerHeight() const { return m_footerHeight; }
void AgDatePickerStyle::setFooterHeight(int height) { m_footerHeight = qMax(16, height); }
QColor AgDatePickerStyle::footerTopLineColor() const { return m_footerTopLineColor; }
void AgDatePickerStyle::setFooterTopLineColor(const QColor &color) { m_footerTopLineColor = color; }
QColor AgDatePickerStyle::footerLinkColor() const { return m_footerLinkColor; }
void AgDatePickerStyle::setFooterLinkColor(const QColor &color) { m_footerLinkColor = color; }
QColor AgDatePickerStyle::footerLinkHoverColor() const { return m_footerLinkHoverColor; }
void AgDatePickerStyle::setFooterLinkHoverColor(const QColor &color) { m_footerLinkHoverColor = color; }
QColor AgDatePickerStyle::footerLinkDisabledColor() const { return m_footerLinkDisabledColor; }
void AgDatePickerStyle::setFooterLinkDisabledColor(const QColor &color) { m_footerLinkDisabledColor = color; }

void AgDatePickerStyle::registerGlobalObserver(AgDatePicker *widget)
{
    if (widget && !s_observers.contains(widget))
        s_observers.append(widget);
}

void AgDatePickerStyle::unregisterGlobalObserver(AgDatePicker *widget)
{
    s_observers.removeOne(widget);
}

void AgDatePickerStyle::notifyGlobalStyleChanged()
{
    // 拷贝一份再遍历：观察者可能在回调中销毁或重新注册
    const QVector<AgDatePicker *> observers = s_observers;
    for (AgDatePicker *widget : observers) {
        if (widget)
            widget->onGlobalStyleChanged();
    }
}
