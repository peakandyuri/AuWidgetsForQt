#include "AuDatePickerStyle.h"

#include "AuDatePicker.h"
#include "AuDatePickerPanel.h"
#include "AuDateRangePanel.h"
#include "AuTimePanel.h"

#include <QApplication>
#include <QtGlobal>

AuDatePickerStyle *AuDatePickerStyle::s_globalStyle = nullptr;
QVector<AuDatePicker *> AuDatePickerStyle::s_observers;
QVector<QPointer<QWidget> > AuDatePickerStyle::s_panelObservers;

AuDatePickerStyle::AuDatePickerStyle()
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

    // 时间面板（尺寸类字段在 applySizeModePreset 中设置）

    // 确定按钮
    m_confirmBackgroundColor = QColor(0x16, 0x77, 0xff);
    m_confirmHoverBackgroundColor = QColor(0x40, 0x96, 0xff);
    m_confirmForegroundColor = QColor(Qt::white);
    m_confirmDisabledBackgroundColor = QColor(0, 0, 0, 25);
    m_confirmDisabledForegroundColor = QColor(0, 0, 0, 102);

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

AuDatePickerStyle::~AuDatePickerStyle()
{
}

AuDatePickerStyle *AuDatePickerStyle::createLightStyle(SizeMode mode)
{
    AuDatePickerStyle *style = new AuDatePickerStyle();
    style->setSizeMode(mode);
    return style;
}

AuDatePickerStyle *AuDatePickerStyle::createDarkStyle(SizeMode mode)
{
    AuDatePickerStyle *style = new AuDatePickerStyle();
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

    // 确定按钮
    style->m_confirmBackgroundColor = QColor(0x16, 0x77, 0xff);
    style->m_confirmHoverBackgroundColor = QColor(0x40, 0x96, 0xff);
    style->m_confirmForegroundColor = QColor(Qt::white);
    style->m_confirmDisabledBackgroundColor = QColor(255, 255, 255, 25);
    style->m_confirmDisabledForegroundColor = QColor(255, 255, 255, 102);

    return style;
}

AuDatePickerStyle *AuDatePickerStyle::globalStyle()
{
    if (!s_globalStyle)
        s_globalStyle = new AuDatePickerStyle();
    return s_globalStyle;
}

void AuDatePickerStyle::setGlobalStyle(AuDatePickerStyle *style)
{
    if (s_globalStyle == style)
        return;
    AuDatePickerStyle *oldStyle = s_globalStyle;
    delete s_globalStyle;
    s_globalStyle = style;
    notifyGlobalStyleChanged(oldStyle);
}

AuDatePickerStyle::SizeMode AuDatePickerStyle::sizeMode() const
{
    return m_sizeMode;
}

void AuDatePickerStyle::setSizeMode(SizeMode mode)
{
    m_sizeMode = mode;
    applySizeModePreset(mode);
}

void AuDatePickerStyle::applySizeModePreset(SizeMode mode)
{
    // 字体：小 12px / 中 14px / 大 16px，默认字体族微软雅黑（Au 控件统一约定）
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
        m_timeColumnWidth = 36;
        m_timeItemHeight = 20;
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
        m_timeColumnWidth = 44;
        m_timeItemHeight = 28;
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
        m_timeColumnWidth = 40;
        m_timeItemHeight = 24;
        break;
    }

    m_font = QApplication::font();
    m_font.setFamily(QStringLiteral("Microsoft YaHei"));
    m_font.setPixelSize(fontPixelSize);

    m_editorBorderWidth = 1;
    m_panelShadowRadius = 10;
}

QFont AuDatePickerStyle::font() const
{
    return m_font;
}

void AuDatePickerStyle::setFont(const QFont &font)
{
    m_font = font;
}

// ---- 编辑框 ----
int AuDatePickerStyle::editorHeight() const { return m_editorHeight; }
void AuDatePickerStyle::setEditorHeight(int height) { m_editorHeight = height; }
QColor AuDatePickerStyle::editorBackgroundColor() const { return m_editorBackgroundColor; }
void AuDatePickerStyle::setEditorBackgroundColor(const QColor &color) { m_editorBackgroundColor = color; }
QColor AuDatePickerStyle::editorForegroundColor() const { return m_editorForegroundColor; }
void AuDatePickerStyle::setEditorForegroundColor(const QColor &color) { m_editorForegroundColor = color; }
QColor AuDatePickerStyle::editorPlaceholderColor() const { return m_editorPlaceholderColor; }
void AuDatePickerStyle::setEditorPlaceholderColor(const QColor &color) { m_editorPlaceholderColor = color; }
QColor AuDatePickerStyle::editorBorderColor() const { return m_editorBorderColor; }
void AuDatePickerStyle::setEditorBorderColor(const QColor &color) { m_editorBorderColor = color; }
QColor AuDatePickerStyle::editorHoverBorderColor() const { return m_editorHoverBorderColor; }
void AuDatePickerStyle::setEditorHoverBorderColor(const QColor &color) { m_editorHoverBorderColor = color; }
QColor AuDatePickerStyle::editorActiveBorderColor() const { return m_editorActiveBorderColor; }
void AuDatePickerStyle::setActiveBorderColor(const QColor &color) { m_editorActiveBorderColor = color; }
QColor AuDatePickerStyle::editorFocusBorderColor() const { return m_editorFocusBorderColor; }
void AuDatePickerStyle::setEditorFocusBorderColor(const QColor &color) { m_editorFocusBorderColor = color; }
QColor AuDatePickerStyle::editorFocusBackgroundColor() const { return m_editorFocusBackgroundColor; }
void AuDatePickerStyle::setEditorFocusBackgroundColor(const QColor &color) { m_editorFocusBackgroundColor = color; }
QColor AuDatePickerStyle::editorDisabledBackgroundColor() const { return m_editorDisabledBackgroundColor; }
void AuDatePickerStyle::setEditorDisabledBackgroundColor(const QColor &color) { m_editorDisabledBackgroundColor = color; }
QColor AuDatePickerStyle::editorDisabledForegroundColor() const { return m_editorDisabledForegroundColor; }
void AuDatePickerStyle::setEditorDisabledForegroundColor(const QColor &color) { m_editorDisabledForegroundColor = color; }
QColor AuDatePickerStyle::editorDisabledBorderColor() const { return m_editorDisabledBorderColor; }
void AuDatePickerStyle::setEditorDisabledBorderColor(const QColor &color) { m_editorDisabledBorderColor = color; }
int AuDatePickerStyle::editorBorderRadius() const { return m_editorBorderRadius; }
void AuDatePickerStyle::setEditorBorderRadius(int radius) { m_editorBorderRadius = qMax(0, radius); }
int AuDatePickerStyle::editorBorderWidth() const { return m_editorBorderWidth; }
void AuDatePickerStyle::setEditorBorderWidth(int width) { m_editorBorderWidth = qMax(0, width); }
int AuDatePickerStyle::editorHorizontalPadding() const { return m_editorHorizontalPadding; }
void AuDatePickerStyle::setEditorHorizontalPadding(int padding) { m_editorHorizontalPadding = qMax(0, padding); }
int AuDatePickerStyle::iconSlotWidth() const { return m_iconSlotWidth; }
void AuDatePickerStyle::setIconSlotWidth(int width) { m_iconSlotWidth = qMax(12, width); }

// ---- 日历 / 清除图标 ----
QColor AuDatePickerStyle::calendarIconColor() const { return m_calendarIconColor; }
void AuDatePickerStyle::setCalendarIconColor(const QColor &color) { m_calendarIconColor = color; }
QColor AuDatePickerStyle::clearIconCircleColor() const { return m_clearIconCircleColor; }
void AuDatePickerStyle::setClearIconCircleColor(const QColor &color) { m_clearIconCircleColor = color; }
QColor AuDatePickerStyle::clearIconCircleHoverColor() const { return m_clearIconCircleHoverColor; }
void AuDatePickerStyle::setClearIconCircleHoverColor(const QColor &color) { m_clearIconCircleHoverColor = color; }
QColor AuDatePickerStyle::clearIconCrossColor() const { return m_clearIconCrossColor; }
void AuDatePickerStyle::setClearIconCrossColor(const QColor &color) { m_clearIconCrossColor = color; }

// ---- 面板 ----
QColor AuDatePickerStyle::panelBackgroundColor() const { return m_panelBackgroundColor; }
void AuDatePickerStyle::setPanelBackgroundColor(const QColor &color) { m_panelBackgroundColor = color; }
QColor AuDatePickerStyle::panelBorderColor() const { return m_panelBorderColor; }
void AuDatePickerStyle::setPanelBorderColor(const QColor &color) { m_panelBorderColor = color; }
int AuDatePickerStyle::panelBorderRadius() const { return m_panelBorderRadius; }
void AuDatePickerStyle::setPanelBorderRadius(int radius) { m_panelBorderRadius = qMax(0, radius); }
QColor AuDatePickerStyle::panelShadowColor() const { return m_panelShadowColor; }
void AuDatePickerStyle::setPanelShadowColor(const QColor &color) { m_panelShadowColor = color; }
int AuDatePickerStyle::panelShadowRadius() const { return m_panelShadowRadius; }
void AuDatePickerStyle::setPanelShadowRadius(int radius) { m_panelShadowRadius = qMax(0, radius); }
int AuDatePickerStyle::panelHorizontalPadding() const { return m_panelHorizontalPadding; }
void AuDatePickerStyle::setPanelHorizontalPadding(int padding) { m_panelHorizontalPadding = qMax(0, padding); }

// ---- 时间面板 ----
int AuDatePickerStyle::timeColumnWidth() const { return m_timeColumnWidth; }
void AuDatePickerStyle::setTimeColumnWidth(int width) { m_timeColumnWidth = qMax(24, width); }
int AuDatePickerStyle::timeItemHeight() const { return m_timeItemHeight; }
void AuDatePickerStyle::setTimeItemHeight(int height) { m_timeItemHeight = qMax(14, height); }

// ---- 确定按钮 ----
QColor AuDatePickerStyle::confirmBackgroundColor() const { return m_confirmBackgroundColor; }
void AuDatePickerStyle::setConfirmBackgroundColor(const QColor &color) { m_confirmBackgroundColor = color; }
QColor AuDatePickerStyle::confirmHoverBackgroundColor() const { return m_confirmHoverBackgroundColor; }
void AuDatePickerStyle::setConfirmHoverBackgroundColor(const QColor &color) { m_confirmHoverBackgroundColor = color; }
QColor AuDatePickerStyle::confirmForegroundColor() const { return m_confirmForegroundColor; }
void AuDatePickerStyle::setConfirmForegroundColor(const QColor &color) { m_confirmForegroundColor = color; }
QColor AuDatePickerStyle::confirmDisabledBackgroundColor() const { return m_confirmDisabledBackgroundColor; }
void AuDatePickerStyle::setConfirmDisabledBackgroundColor(const QColor &color) { m_confirmDisabledBackgroundColor = color; }
QColor AuDatePickerStyle::confirmDisabledForegroundColor() const { return m_confirmDisabledForegroundColor; }
void AuDatePickerStyle::setConfirmDisabledForegroundColor(const QColor &color) { m_confirmDisabledForegroundColor = color; }

// ---- 面板头部 ----
int AuDatePickerStyle::headHeight() const { return m_headHeight; }
void AuDatePickerStyle::setHeadHeight(int height) { m_headHeight = qMax(16, height); }
int AuDatePickerStyle::navButtonSize() const { return m_navButtonSize; }
void AuDatePickerStyle::setNavButtonSize(int size) { m_navButtonSize = qMax(12, size); }
QColor AuDatePickerStyle::navIconColor() const { return m_navIconColor; }
void AuDatePickerStyle::setNavIconColor(const QColor &color) { m_navIconColor = color; }
QColor AuDatePickerStyle::navIconDisabledColor() const { return m_navIconDisabledColor; }
void AuDatePickerStyle::setNavIconDisabledColor(const QColor &color) { m_navIconDisabledColor = color; }
QColor AuDatePickerStyle::navHoverBackgroundColor() const { return m_navHoverBackgroundColor; }
void AuDatePickerStyle::setNavHoverBackgroundColor(const QColor &color) { m_navHoverBackgroundColor = color; }
QColor AuDatePickerStyle::headTitleColor() const { return m_headTitleColor; }
void AuDatePickerStyle::setHeadTitleColor(const QColor &color) { m_headTitleColor = color; }
QColor AuDatePickerStyle::headTitleHoverColor() const { return m_headTitleHoverColor; }
void AuDatePickerStyle::setHeadTitleHoverColor(const QColor &color) { m_headTitleHoverColor = color; }
QColor AuDatePickerStyle::headLabelColor() const { return m_headLabelColor; }
void AuDatePickerStyle::setHeadLabelColor(const QColor &color) { m_headLabelColor = color; }

// ---- 星期行 ----
int AuDatePickerStyle::weekdayRowHeight() const { return m_weekdayRowHeight; }
void AuDatePickerStyle::setWeekdayRowHeight(int height) { m_weekdayRowHeight = qMax(12, height); }
QColor AuDatePickerStyle::weekdayForegroundColor() const { return m_weekdayForegroundColor; }
void AuDatePickerStyle::setWeekdayForegroundColor(const QColor &color) { m_weekdayForegroundColor = color; }

// ---- 格子 ----
int AuDatePickerStyle::dayCellWidth() const { return m_dayCellWidth; }
void AuDatePickerStyle::setDayCellWidth(int width) { m_dayCellWidth = qMax(16, width); }
int AuDatePickerStyle::dayCellHeight() const { return m_dayCellHeight; }
void AuDatePickerStyle::setDayCellHeight(int height) { m_dayCellHeight = qMax(14, height); }
int AuDatePickerStyle::cellSpacing() const { return m_cellSpacing; }
void AuDatePickerStyle::setCellSpacing(int spacing) { m_cellSpacing = qMax(0, spacing); }
int AuDatePickerStyle::cellRadius() const { return m_cellRadius; }
void AuDatePickerStyle::setCellRadius(int radius) { m_cellRadius = qMax(0, radius); }
QColor AuDatePickerStyle::cellForegroundColor() const { return m_cellForegroundColor; }
void AuDatePickerStyle::setCellForegroundColor(const QColor &color) { m_cellForegroundColor = color; }
QColor AuDatePickerStyle::cellOtherMonthForegroundColor() const { return m_cellOtherMonthForegroundColor; }
void AuDatePickerStyle::setCellOtherMonthForegroundColor(const QColor &color) { m_cellOtherMonthForegroundColor = color; }
QColor AuDatePickerStyle::cellDisabledForegroundColor() const { return m_cellDisabledForegroundColor; }
void AuDatePickerStyle::setCellDisabledForegroundColor(const QColor &color) { m_cellDisabledForegroundColor = color; }
QColor AuDatePickerStyle::cellHoverBackgroundColor() const { return m_cellHoverBackgroundColor; }
void AuDatePickerStyle::setCellHoverBackgroundColor(const QColor &color) { m_cellHoverBackgroundColor = color; }
QColor AuDatePickerStyle::cellSelectedBackgroundColor() const { return m_cellSelectedBackgroundColor; }
void AuDatePickerStyle::setCellSelectedBackgroundColor(const QColor &color) { m_cellSelectedBackgroundColor = color; }
QColor AuDatePickerStyle::cellSelectedForegroundColor() const { return m_cellSelectedForegroundColor; }
void AuDatePickerStyle::setCellSelectedForegroundColor(const QColor &color) { m_cellSelectedForegroundColor = color; }
QColor AuDatePickerStyle::todayForegroundColor() const { return m_todayForegroundColor; }
void AuDatePickerStyle::setTodayForegroundColor(const QColor &color) { m_todayForegroundColor = color; }
QColor AuDatePickerStyle::cellInRangeBackgroundColor() const { return m_cellInRangeBackgroundColor; }
void AuDatePickerStyle::setCellInRangeBackgroundColor(const QColor &color) { m_cellInRangeBackgroundColor = color; }

// ---- footer ----
int AuDatePickerStyle::footerHeight() const { return m_footerHeight; }
void AuDatePickerStyle::setFooterHeight(int height) { m_footerHeight = qMax(16, height); }
QColor AuDatePickerStyle::footerTopLineColor() const { return m_footerTopLineColor; }
void AuDatePickerStyle::setFooterTopLineColor(const QColor &color) { m_footerTopLineColor = color; }
QColor AuDatePickerStyle::footerLinkColor() const { return m_footerLinkColor; }
void AuDatePickerStyle::setFooterLinkColor(const QColor &color) { m_footerLinkColor = color; }
QColor AuDatePickerStyle::footerLinkHoverColor() const { return m_footerLinkHoverColor; }
void AuDatePickerStyle::setFooterLinkHoverColor(const QColor &color) { m_footerLinkHoverColor = color; }
QColor AuDatePickerStyle::footerLinkDisabledColor() const { return m_footerLinkDisabledColor; }
void AuDatePickerStyle::setFooterLinkDisabledColor(const QColor &color) { m_footerLinkDisabledColor = color; }

void AuDatePickerStyle::registerGlobalObserver(AuDatePicker *widget)
{
    if (widget && !s_observers.contains(widget))
        s_observers.append(widget);
}

void AuDatePickerStyle::unregisterGlobalObserver(AuDatePicker *widget)
{
    s_observers.removeOne(widget);
}

void AuDatePickerStyle::registerPanelObserver(QWidget *panel)
{
    if (panel && !s_panelObservers.contains(panel))
        s_panelObservers.append(panel);
}

void AuDatePickerStyle::unregisterPanelObserver(QWidget *panel)
{
    s_panelObservers.removeOne(panel);
}

void AuDatePickerStyle::notifyGlobalStyleChanged(AuDatePickerStyle *oldStyle)
{
    // 拷贝一份再遍历：观察者可能在回调中销毁或重新注册
    const QVector<AuDatePicker *> observers = s_observers;
    for (AuDatePicker *widget : observers) {
        if (widget)
            widget->onGlobalStyleChanged();
    }
    // 独立内嵌面板：仍指向旧全局样式的（即跟随全局的）刷新为新样式；
    // 显式 setStyle 过的面板保持自己的样式不动
    const QVector<QPointer<QWidget> > panels = s_panelObservers;
    for (const QPointer<QWidget> &panel : panels) {
        if (!panel)
            continue;
        if (auto *p = qobject_cast<AuDatePickerPanel *>(panel)) {
            if (p->style() == oldStyle)
                p->setStyle(s_globalStyle);
        } else if (auto *r = qobject_cast<AuDateRangePanel *>(panel)) {
            if (r->style() == oldStyle)
                r->setStyle(s_globalStyle);
        } else if (auto *t = qobject_cast<AuTimePanel *>(panel)) {
            if (t->style() == oldStyle)
                t->setStyle(s_globalStyle);
        }
    }
}
