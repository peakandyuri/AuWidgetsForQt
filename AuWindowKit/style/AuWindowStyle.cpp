#include "AuWindowStyle.h"
#include "AuWindow.h"

#include <QVector>

AuWindowStyle *AuWindowStyle::s_globalStyle = nullptr;
QVector<AuWindow *> AuWindowStyle::s_observers;

AuWindowStyle::AuWindowStyle()
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

    // 阴影参数：Windows DWM 通道使用（shadowColor 取 RGB，alpha 不参与）；
    // X11 上阴影由 WM 主题经 border-only 装饰绘制（见 AuWindowX11
    // platformInit），本组参数在 Linux 不生效（外观由主题决定）
    m_shadowColor = QColor(0, 0, 0, 255);
}

AuWindowStyle::~AuWindowStyle()
{
}

AuWindowStyle *AuWindowStyle::createLightStyle()
{
    return new AuWindowStyle();
}

AuWindowStyle *AuWindowStyle::createDarkStyle()
{
    AuWindowStyle *style = new AuWindowStyle();
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

AuWindowStyle *AuWindowStyle::globalStyle()
{
    if (!s_globalStyle)
        s_globalStyle = new AuWindowStyle();
    return s_globalStyle;
}

void AuWindowStyle::setGlobalStyle(AuWindowStyle *style)
{
    if (s_globalStyle == style)
        return;
    delete s_globalStyle;
    s_globalStyle = style;
    notifyGlobalStyleChanged();
}

// ---- 窗体 ----

QColor AuWindowStyle::windowBackgroundColor() const { return m_windowBackgroundColor; }
void AuWindowStyle::setWindowBackgroundColor(const QColor &color) { m_windowBackgroundColor = color; }
QColor AuWindowStyle::windowBorderColor() const { return m_windowBorderColor; }
void AuWindowStyle::setWindowBorderColor(const QColor &color) { m_windowBorderColor = color; }

// ---- 标题栏 ----

int AuWindowStyle::titleBarHeight() const { return m_titleBarHeight; }
void AuWindowStyle::setTitleBarHeight(int height) { m_titleBarHeight = qMax(0, height); }
QColor AuWindowStyle::titleBarBackgroundColor() const { return m_titleBarBackgroundColor; }
void AuWindowStyle::setTitleBarBackgroundColor(const QColor &color) { m_titleBarBackgroundColor = color; }
QColor AuWindowStyle::titleBarInactiveBackgroundColor() const { return m_titleBarInactiveBackgroundColor; }
void AuWindowStyle::setTitleBarInactiveBackgroundColor(const QColor &color) { m_titleBarInactiveBackgroundColor = color; }
QColor AuWindowStyle::titleForegroundColor() const { return m_titleForegroundColor; }
void AuWindowStyle::setTitleForegroundColor(const QColor &color) { m_titleForegroundColor = color; }
QColor AuWindowStyle::titleInactiveForegroundColor() const { return m_titleInactiveForegroundColor; }
void AuWindowStyle::setTitleInactiveForegroundColor(const QColor &color) { m_titleInactiveForegroundColor = color; }
QFont AuWindowStyle::titleFont() const { return m_titleFont; }
void AuWindowStyle::setTitleFont(const QFont &font) { m_titleFont = font; }
int AuWindowStyle::titleBarLeftPadding() const { return m_titleBarLeftPadding; }
void AuWindowStyle::setTitleBarLeftPadding(int padding) { m_titleBarLeftPadding = qMax(0, padding); }
int AuWindowStyle::titleIconSpacing() const { return m_titleIconSpacing; }
void AuWindowStyle::setTitleIconSpacing(int spacing) { m_titleIconSpacing = qMax(0, spacing); }
int AuWindowStyle::titleIconSize() const { return m_titleIconSize; }
void AuWindowStyle::setTitleIconSize(int size) { m_titleIconSize = qMax(1, size); }

// ---- 标题栏按钮 ----

int AuWindowStyle::titleBarButtonWidth() const { return m_titleBarButtonWidth; }
void AuWindowStyle::setTitleBarButtonWidth(int width) { m_titleBarButtonWidth = qMax(8, width); }
QColor AuWindowStyle::buttonGlyphColor() const { return m_buttonGlyphColor; }
void AuWindowStyle::setButtonGlyphColor(const QColor &color) { m_buttonGlyphColor = color; }
QColor AuWindowStyle::buttonDisabledGlyphColor() const { return m_buttonDisabledGlyphColor; }
void AuWindowStyle::setButtonDisabledGlyphColor(const QColor &color) { m_buttonDisabledGlyphColor = color; }
QColor AuWindowStyle::buttonHoverBackgroundColor() const { return m_buttonHoverBackgroundColor; }
void AuWindowStyle::setButtonHoverBackgroundColor(const QColor &color) { m_buttonHoverBackgroundColor = color; }
QColor AuWindowStyle::buttonPressBackgroundColor() const { return m_buttonPressBackgroundColor; }
void AuWindowStyle::setButtonPressBackgroundColor(const QColor &color) { m_buttonPressBackgroundColor = color; }
QColor AuWindowStyle::closeButtonHoverBackgroundColor() const { return m_closeButtonHoverBackgroundColor; }
void AuWindowStyle::setCloseButtonHoverBackgroundColor(const QColor &color) { m_closeButtonHoverBackgroundColor = color; }
QColor AuWindowStyle::closeButtonPressBackgroundColor() const { return m_closeButtonPressBackgroundColor; }
void AuWindowStyle::setCloseButtonPressBackgroundColor(const QColor &color) { m_closeButtonPressBackgroundColor = color; }
QColor AuWindowStyle::closeButtonGlyphColor() const { return m_closeButtonGlyphColor; }
void AuWindowStyle::setCloseButtonGlyphColor(const QColor &color) { m_closeButtonGlyphColor = color; }

// ---- 阴影 ----

int AuWindowStyle::shadowMargin() const { return m_shadowMargin; }
void AuWindowStyle::setShadowMargin(int margin) { m_shadowMargin = qMax(0, margin); }
int AuWindowStyle::shadowBlurRadius() const { return m_shadowBlurRadius; }
void AuWindowStyle::setShadowBlurRadius(int radius) { m_shadowBlurRadius = qMax(0, radius); }
QColor AuWindowStyle::shadowColor() const { return m_shadowColor; }
void AuWindowStyle::setShadowColor(const QColor &color) { m_shadowColor = color; }

// ---- 全局样式通知 ----

void AuWindowStyle::registerGlobalObserver(AuWindow *window)
{
    if (window && !s_observers.contains(window))
        s_observers.append(window);
}

void AuWindowStyle::unregisterGlobalObserver(AuWindow *window)
{
    s_observers.removeOne(window);
}

void AuWindowStyle::notifyGlobalStyleChanged()
{
    const QVector<AuWindow *> observers = s_observers;
    for (AuWindow *window : observers) {
        if (s_observers.contains(window))
            window->onGlobalStyleChanged();
    }
}
