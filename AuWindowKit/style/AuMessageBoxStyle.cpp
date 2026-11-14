#include "AuMessageBoxStyle.h"
#include "AuMessageBox.h"

#include <QVector>

AuMessageBoxStyle *AuMessageBoxStyle::s_globalStyle = nullptr;
QVector<AuMessageBox *> AuMessageBoxStyle::s_observers;

AuMessageBoxStyle::AuMessageBoxStyle()
    : m_iconSize(40)
    , m_iconSpacing(14)
    , m_contentMargin(24)
    , m_contentSpacing(16)
    , m_minContentWidth(320)
    , m_maxContentWidth(520)
    , m_buttonMinWidth(72)
    , m_buttonHeight(30)
    , m_buttonSpacing(8)
    , m_buttonRadius(4)
    , m_buttonPadding(18)
{
    // 浅色主题默认值（与 AuWindowStyle 浅色窗体底色搭配）
    m_messageFont.setFamily(QStringLiteral("微软雅黑"));
    m_messageFont.setPixelSize(14);
    m_messageColor = QColor(0, 0, 0, 220);

    m_informativeFont.setFamily(QStringLiteral("微软雅黑"));
    m_informativeFont.setPixelSize(12);
    m_informativeColor = QColor(0, 0, 0, 150);

    // 内置图标徽章语义色（信息 / 询问同色系，警告琥珀，错误红）
    m_iconInformationColor = QColor(45, 140, 240);
    m_iconQuestionColor = QColor(45, 140, 240);
    m_iconWarningColor = QColor(240, 163, 45);
    m_iconCriticalColor = QColor(229, 72, 77);

    m_buttonFont.setFamily(QStringLiteral("微软雅黑"));
    m_buttonFont.setPixelSize(12);
    m_buttonBackground = QColor(0, 0, 0, 10);
    m_buttonHoverBackground = QColor(0, 0, 0, 22);
    m_buttonPressBackground = QColor(0, 0, 0, 38);
    m_buttonBorderColor = QColor(0, 0, 0, 40);
    m_buttonTextColor = QColor(0, 0, 0, 220);
    m_buttonDisabledTextColor = QColor(0, 0, 0, 80);

    m_defaultButtonBackground = QColor(45, 140, 240);
    m_defaultButtonHoverBackground = QColor(70, 158, 245);
    m_defaultButtonPressBackground = QColor(33, 116, 205);
    m_defaultButtonTextColor = QColor(255, 255, 255);
}

AuMessageBoxStyle::~AuMessageBoxStyle()
{
}

AuMessageBoxStyle *AuMessageBoxStyle::createLightStyle()
{
    return new AuMessageBoxStyle();
}

AuMessageBoxStyle *AuMessageBoxStyle::createDarkStyle()
{
    AuMessageBoxStyle *style = new AuMessageBoxStyle();
    style->setMessageColor(QColor(255, 255, 255, 230));
    style->setInformativeColor(QColor(255, 255, 255, 160));
    style->setIconInformationColor(QColor(75, 163, 245));
    style->setIconQuestionColor(QColor(75, 163, 245));
    style->setIconWarningColor(QColor(245, 178, 66));
    style->setIconCriticalColor(QColor(240, 90, 90));
    style->setButtonBackground(QColor(255, 255, 255, 16));
    style->setButtonHoverBackground(QColor(255, 255, 255, 30));
    style->setButtonPressBackground(QColor(255, 255, 255, 48));
    style->setButtonBorderColor(QColor(255, 255, 255, 50));
    style->setButtonTextColor(QColor(255, 255, 255, 225));
    style->setButtonDisabledTextColor(QColor(255, 255, 255, 90));
    // 主按钮蓝色在深浅两套主题下均成立，仅文字色不同（浅色主题白字同理成立）
    return style;
}

AuMessageBoxStyle *AuMessageBoxStyle::globalStyle()
{
    if (!s_globalStyle)
        s_globalStyle = new AuMessageBoxStyle();
    return s_globalStyle;
}

void AuMessageBoxStyle::setGlobalStyle(AuMessageBoxStyle *style)
{
    if (s_globalStyle == style)
        return;
    delete s_globalStyle;
    s_globalStyle = style;
    notifyGlobalStyleChanged();
}

// ---- 正文 / 补充文本 ----

QFont AuMessageBoxStyle::messageFont() const { return m_messageFont; }
void AuMessageBoxStyle::setMessageFont(const QFont &font) { m_messageFont = font; }
QColor AuMessageBoxStyle::messageColor() const { return m_messageColor; }
void AuMessageBoxStyle::setMessageColor(const QColor &color) { m_messageColor = color; }
QFont AuMessageBoxStyle::informativeFont() const { return m_informativeFont; }
void AuMessageBoxStyle::setInformativeFont(const QFont &font) { m_informativeFont = font; }
QColor AuMessageBoxStyle::informativeColor() const { return m_informativeColor; }
void AuMessageBoxStyle::setInformativeColor(const QColor &color) { m_informativeColor = color; }

// ---- 图标 ----

int AuMessageBoxStyle::iconSize() const { return m_iconSize; }
void AuMessageBoxStyle::setIconSize(int size) { m_iconSize = qMax(16, size); }
int AuMessageBoxStyle::iconSpacing() const { return m_iconSpacing; }
void AuMessageBoxStyle::setIconSpacing(int spacing) { m_iconSpacing = qMax(0, spacing); }
QColor AuMessageBoxStyle::iconInformationColor() const { return m_iconInformationColor; }
void AuMessageBoxStyle::setIconInformationColor(const QColor &color) { m_iconInformationColor = color; }
QColor AuMessageBoxStyle::iconQuestionColor() const { return m_iconQuestionColor; }
void AuMessageBoxStyle::setIconQuestionColor(const QColor &color) { m_iconQuestionColor = color; }
QColor AuMessageBoxStyle::iconWarningColor() const { return m_iconWarningColor; }
void AuMessageBoxStyle::setIconWarningColor(const QColor &color) { m_iconWarningColor = color; }
QColor AuMessageBoxStyle::iconCriticalColor() const { return m_iconCriticalColor; }
void AuMessageBoxStyle::setIconCriticalColor(const QColor &color) { m_iconCriticalColor = color; }

// ---- 内容区布局 ----

int AuMessageBoxStyle::contentMargin() const { return m_contentMargin; }
void AuMessageBoxStyle::setContentMargin(int margin) { m_contentMargin = qMax(0, margin); }
int AuMessageBoxStyle::contentSpacing() const { return m_contentSpacing; }
void AuMessageBoxStyle::setContentSpacing(int spacing) { m_contentSpacing = qMax(0, spacing); }
int AuMessageBoxStyle::minContentWidth() const { return m_minContentWidth; }
void AuMessageBoxStyle::setMinContentWidth(int width) { m_minContentWidth = qMax(160, width); }
int AuMessageBoxStyle::maxContentWidth() const { return m_maxContentWidth; }
void AuMessageBoxStyle::setMaxContentWidth(int width) { m_maxContentWidth = qMax(240, width); }

// ---- 按钮 ----

QFont AuMessageBoxStyle::buttonFont() const { return m_buttonFont; }
void AuMessageBoxStyle::setButtonFont(const QFont &font) { m_buttonFont = font; }
int AuMessageBoxStyle::buttonMinWidth() const { return m_buttonMinWidth; }
void AuMessageBoxStyle::setButtonMinWidth(int width) { m_buttonMinWidth = qMax(40, width); }
int AuMessageBoxStyle::buttonHeight() const { return m_buttonHeight; }
void AuMessageBoxStyle::setButtonHeight(int height) { m_buttonHeight = qMax(20, height); }
int AuMessageBoxStyle::buttonSpacing() const { return m_buttonSpacing; }
void AuMessageBoxStyle::setButtonSpacing(int spacing) { m_buttonSpacing = qMax(0, spacing); }
int AuMessageBoxStyle::buttonRadius() const { return m_buttonRadius; }
void AuMessageBoxStyle::setButtonRadius(int radius) { m_buttonRadius = qMax(0, radius); }
int AuMessageBoxStyle::buttonPadding() const { return m_buttonPadding; }
void AuMessageBoxStyle::setButtonPadding(int padding) { m_buttonPadding = qMax(4, padding); }
QColor AuMessageBoxStyle::buttonBackground() const { return m_buttonBackground; }
void AuMessageBoxStyle::setButtonBackground(const QColor &color) { m_buttonBackground = color; }
QColor AuMessageBoxStyle::buttonHoverBackground() const { return m_buttonHoverBackground; }
void AuMessageBoxStyle::setButtonHoverBackground(const QColor &color) { m_buttonHoverBackground = color; }
QColor AuMessageBoxStyle::buttonPressBackground() const { return m_buttonPressBackground; }
void AuMessageBoxStyle::setButtonPressBackground(const QColor &color) { m_buttonPressBackground = color; }
QColor AuMessageBoxStyle::buttonBorderColor() const { return m_buttonBorderColor; }
void AuMessageBoxStyle::setButtonBorderColor(const QColor &color) { m_buttonBorderColor = color; }
QColor AuMessageBoxStyle::buttonTextColor() const { return m_buttonTextColor; }
void AuMessageBoxStyle::setButtonTextColor(const QColor &color) { m_buttonTextColor = color; }
QColor AuMessageBoxStyle::buttonDisabledTextColor() const { return m_buttonDisabledTextColor; }
void AuMessageBoxStyle::setButtonDisabledTextColor(const QColor &color) { m_buttonDisabledTextColor = color; }

// ---- 默认按钮 ----

QColor AuMessageBoxStyle::defaultButtonBackground() const { return m_defaultButtonBackground; }
void AuMessageBoxStyle::setDefaultButtonBackground(const QColor &color) { m_defaultButtonBackground = color; }
QColor AuMessageBoxStyle::defaultButtonHoverBackground() const { return m_defaultButtonHoverBackground; }
void AuMessageBoxStyle::setDefaultButtonHoverBackground(const QColor &color) { m_defaultButtonHoverBackground = color; }
QColor AuMessageBoxStyle::defaultButtonPressBackground() const { return m_defaultButtonPressBackground; }
void AuMessageBoxStyle::setDefaultButtonPressBackground(const QColor &color) { m_defaultButtonPressBackground = color; }
QColor AuMessageBoxStyle::defaultButtonTextColor() const { return m_defaultButtonTextColor; }
void AuMessageBoxStyle::setDefaultButtonTextColor(const QColor &color) { m_defaultButtonTextColor = color; }

// ---- 全局样式通知 ----

void AuMessageBoxStyle::registerGlobalObserver(AuMessageBox *box)
{
    if (box && !s_observers.contains(box))
        s_observers.append(box);
}

void AuMessageBoxStyle::unregisterGlobalObserver(AuMessageBox *box)
{
    s_observers.removeOne(box);
}

void AuMessageBoxStyle::notifyGlobalStyleChanged()
{
    const QVector<AuMessageBox *> observers = s_observers;
    for (AuMessageBox *box : observers) {
        if (s_observers.contains(box))
            box->onGlobalMessageStyleChanged();
    }
}
