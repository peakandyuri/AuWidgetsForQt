#include "AgWindowStyle.h"
#include "AgWindow.h"

#include <QVector>

AgWindowStyle *AgWindowStyle::s_globalStyle = nullptr;
QVector<AgWindow *> AgWindowStyle::s_observers;

AgWindowStyle::AgWindowStyle()
    : m_titleBarHeight(32)
    , m_titleBarLeftPadding(10)
    , m_titleIconSpacing(6)
    , m_titleIconSize(16)
    , m_titleBarButtonWidth(46)
    , m_shadowMargin(24)
    , m_shadowBlurRadius(28)
{
    // 浅色主题默认值
    m_windowBackgroundColor = QColor(0xFF, 0xFF, 0xFF);
    m_windowBorderColor = QColor(0, 0, 0, 30);
    m_titleBarBackgroundColor = QColor(0xF3, 0xF3, 0xF3);
    m_titleBarInactiveBackgroundColor = QColor(0xFA, 0xFA, 0xFA);
    m_titleForegroundColor = QColor(0, 0, 0, 220);
    m_titleInactiveForegroundColor = QColor(0, 0, 0, 130);

    m_titleFont.setFamily(QStringLiteral("微软雅黑"));
    m_titleFont.setPixelSize(12);

    m_buttonGlyphColor = QColor(0, 0, 0, 180);
    m_buttonDisabledGlyphColor = QColor(0, 0, 0, 80);
    m_buttonHoverBackgroundColor = QColor(0, 0, 0, 15);
    m_buttonPressBackgroundColor = QColor(0, 0, 0, 26);
    m_closeButtonHoverBackgroundColor = QColor(0xC4, 0x2B, 0x1C);
    m_closeButtonPressBackgroundColor = QColor(0xB3, 0x2A, 0x21);
    m_closeButtonGlyphColor = QColor(0xFF, 0xFF, 0xFF);

    // 阴影参数按原生窗口装饰阴影标定：贴边约 15% 压暗，sigma≈14px 柔和衰减
    m_shadowColor = QColor(0, 0, 0, 38);
}

AgWindowStyle::~AgWindowStyle()
{
}

AgWindowStyle *AgWindowStyle::createLightStyle()
{
    return new AgWindowStyle();
}

AgWindowStyle *AgWindowStyle::createDarkStyle()
{
    AgWindowStyle *style = new AgWindowStyle();
    style->setWindowBackgroundColor(QColor(0x20, 0x20, 0x20));
    style->setWindowBorderColor(QColor(255, 255, 255, 26));
    style->setTitleBarBackgroundColor(QColor(0x27, 0x27, 0x27));
    style->setTitleBarInactiveBackgroundColor(QColor(0x1F, 0x1F, 0x1F));
    style->setTitleForegroundColor(QColor(255, 255, 255, 225));
    style->setTitleInactiveForegroundColor(QColor(255, 255, 255, 140));
    style->setButtonGlyphColor(QColor(255, 255, 255, 200));
    style->setButtonDisabledGlyphColor(QColor(255, 255, 255, 80));
    style->setButtonHoverBackgroundColor(QColor(255, 255, 255, 26));
    style->setButtonPressBackgroundColor(QColor(255, 255, 255, 51));
    style->setShadowColor(QColor(0, 0, 0, 120));
    return style;
}

AgWindowStyle *AgWindowStyle::globalStyle()
{
    if (!s_globalStyle)
        s_globalStyle = new AgWindowStyle();
    return s_globalStyle;
}

void AgWindowStyle::setGlobalStyle(AgWindowStyle *style)
{
    if (s_globalStyle == style)
        return;
    delete s_globalStyle;
    s_globalStyle = style;
    notifyGlobalStyleChanged();
}

// ---- 窗体 ----

QColor AgWindowStyle::windowBackgroundColor() const { return m_windowBackgroundColor; }
void AgWindowStyle::setWindowBackgroundColor(const QColor &color) { m_windowBackgroundColor = color; }
QColor AgWindowStyle::windowBorderColor() const { return m_windowBorderColor; }
void AgWindowStyle::setWindowBorderColor(const QColor &color) { m_windowBorderColor = color; }

// ---- 标题栏 ----

int AgWindowStyle::titleBarHeight() const { return m_titleBarHeight; }
void AgWindowStyle::setTitleBarHeight(int height) { m_titleBarHeight = qMax(0, height); }
QColor AgWindowStyle::titleBarBackgroundColor() const { return m_titleBarBackgroundColor; }
void AgWindowStyle::setTitleBarBackgroundColor(const QColor &color) { m_titleBarBackgroundColor = color; }
QColor AgWindowStyle::titleBarInactiveBackgroundColor() const { return m_titleBarInactiveBackgroundColor; }
void AgWindowStyle::setTitleBarInactiveBackgroundColor(const QColor &color) { m_titleBarInactiveBackgroundColor = color; }
QColor AgWindowStyle::titleForegroundColor() const { return m_titleForegroundColor; }
void AgWindowStyle::setTitleForegroundColor(const QColor &color) { m_titleForegroundColor = color; }
QColor AgWindowStyle::titleInactiveForegroundColor() const { return m_titleInactiveForegroundColor; }
void AgWindowStyle::setTitleInactiveForegroundColor(const QColor &color) { m_titleInactiveForegroundColor = color; }
QFont AgWindowStyle::titleFont() const { return m_titleFont; }
void AgWindowStyle::setTitleFont(const QFont &font) { m_titleFont = font; }
int AgWindowStyle::titleBarLeftPadding() const { return m_titleBarLeftPadding; }
void AgWindowStyle::setTitleBarLeftPadding(int padding) { m_titleBarLeftPadding = qMax(0, padding); }
int AgWindowStyle::titleIconSpacing() const { return m_titleIconSpacing; }
void AgWindowStyle::setTitleIconSpacing(int spacing) { m_titleIconSpacing = qMax(0, spacing); }
int AgWindowStyle::titleIconSize() const { return m_titleIconSize; }
void AgWindowStyle::setTitleIconSize(int size) { m_titleIconSize = qMax(1, size); }

// ---- 标题栏按钮 ----

int AgWindowStyle::titleBarButtonWidth() const { return m_titleBarButtonWidth; }
void AgWindowStyle::setTitleBarButtonWidth(int width) { m_titleBarButtonWidth = qMax(8, width); }
QColor AgWindowStyle::buttonGlyphColor() const { return m_buttonGlyphColor; }
void AgWindowStyle::setButtonGlyphColor(const QColor &color) { m_buttonGlyphColor = color; }
QColor AgWindowStyle::buttonDisabledGlyphColor() const { return m_buttonDisabledGlyphColor; }
void AgWindowStyle::setButtonDisabledGlyphColor(const QColor &color) { m_buttonDisabledGlyphColor = color; }
QColor AgWindowStyle::buttonHoverBackgroundColor() const { return m_buttonHoverBackgroundColor; }
void AgWindowStyle::setButtonHoverBackgroundColor(const QColor &color) { m_buttonHoverBackgroundColor = color; }
QColor AgWindowStyle::buttonPressBackgroundColor() const { return m_buttonPressBackgroundColor; }
void AgWindowStyle::setButtonPressBackgroundColor(const QColor &color) { m_buttonPressBackgroundColor = color; }
QColor AgWindowStyle::closeButtonHoverBackgroundColor() const { return m_closeButtonHoverBackgroundColor; }
void AgWindowStyle::setCloseButtonHoverBackgroundColor(const QColor &color) { m_closeButtonHoverBackgroundColor = color; }
QColor AgWindowStyle::closeButtonPressBackgroundColor() const { return m_closeButtonPressBackgroundColor; }
void AgWindowStyle::setCloseButtonPressBackgroundColor(const QColor &color) { m_closeButtonPressBackgroundColor = color; }
QColor AgWindowStyle::closeButtonGlyphColor() const { return m_closeButtonGlyphColor; }
void AgWindowStyle::setCloseButtonGlyphColor(const QColor &color) { m_closeButtonGlyphColor = color; }

// ---- 阴影 ----

int AgWindowStyle::shadowMargin() const { return m_shadowMargin; }
void AgWindowStyle::setShadowMargin(int margin) { m_shadowMargin = qMax(0, margin); }
int AgWindowStyle::shadowBlurRadius() const { return m_shadowBlurRadius; }
void AgWindowStyle::setShadowBlurRadius(int radius) { m_shadowBlurRadius = qMax(0, radius); }
QColor AgWindowStyle::shadowColor() const { return m_shadowColor; }
void AgWindowStyle::setShadowColor(const QColor &color) { m_shadowColor = color; }

// ---- 全局样式通知 ----

void AgWindowStyle::registerGlobalObserver(AgWindow *window)
{
    if (window && !s_observers.contains(window))
        s_observers.append(window);
}

void AgWindowStyle::unregisterGlobalObserver(AgWindow *window)
{
    s_observers.removeOne(window);
}

void AgWindowStyle::notifyGlobalStyleChanged()
{
    const QVector<AgWindow *> observers = s_observers;
    for (AgWindow *window : observers) {
        if (s_observers.contains(window))
            window->onGlobalStyleChanged();
    }
}
