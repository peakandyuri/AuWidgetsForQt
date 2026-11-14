#ifndef AUMESSAGEBOXSTYLE_H
#define AUMESSAGEBOXSTYLE_H

#include "AuWindowKit_global.h"

#include <QColor>
#include <QFont>
#include <QVector>

class AuMessageBox;

/**
 * @brief AuMessageBox 消息框本体样式：正文 / 补充文本、图标、按钮行的全部视觉属性。
 *
 * 窗体骨架（标题栏、窗体背景、边框、阴影）不属于本类——消息框作为 AuWindow
 * 子类，窗体层视觉始终跟随 AuWindowStyle（全局样式或 AuWindow::setStyle）。
 * 两套全局样式需分别设置：
 * \code
 * AuWindowStyle::setGlobalStyle(AuWindowStyle::createDarkStyle());        // 窗体骨架
 * AuMessageBoxStyle::setGlobalStyle(AuMessageBoxStyle::createDarkStyle()); // 消息框本体
 * \endcode
 *
 * 三种使用方式（与 AuWindowStyle / AuSelectStyle 保持一致）：
 *  1. 全局样式：setGlobalStyle() 设置后，所有未显式 setStyle() 的 AuMessageBox
 *     自动使用，且全局样式变化时已存在的消息框自动刷新；
 *  2. 单实例独立样式：box.setStyle(new AuMessageBoxStyle())（消息框接管所有权），
 *     随后可对实例逐项微调，例如 box.style()->setButtonRadius(2);
 *  3. 内置主题工厂：createLightStyle() / createDarkStyle()。
 *
 * 说明：
 *  - 按钮颜色字段为无效 QColor 时的语义：buttonBackground 无效 = 按钮透明底
 *    （仅描边）；hover / press 无效 = 沿用常态背景；buttonBorderColor 无效 =
 *    无边框（扁平按钮）。
 *  - 图标颜色只作用于内置矢量图标（Information / Question / Warning /
 *    Critical 四色徽章）；setIconPixmap() 的自定义贴图不受本类影响。
 */
class AUWINDOWKIT_EXPORT AuMessageBoxStyle
{
public:
    AuMessageBoxStyle();
    virtual ~AuMessageBoxStyle();

    // 内置主题工厂（所有权归调用者）
    static AuMessageBoxStyle *createLightStyle();
    static AuMessageBoxStyle *createDarkStyle();

    // 全局样式：未显式设置样式的 AuMessageBox 自动使用
    static AuMessageBoxStyle *globalStyle();
    static void setGlobalStyle(AuMessageBoxStyle *style);   // 所有权归调用者

    // ---- 正文 / 补充文本 ----
    QFont messageFont() const;                              ///< 主文本字体（默认微软雅黑 14px）
    void setMessageFont(const QFont &font);
    QColor messageColor() const;                            ///< 主文本颜色
    void setMessageColor(const QColor &color);
    QFont informativeFont() const;                          ///< 补充文本字体（默认微软雅黑 12px）
    void setInformativeFont(const QFont &font);
    QColor informativeColor() const;                        ///< 补充文本颜色
    void setInformativeColor(const QColor &color);

    // ---- 图标 ----
    int iconSize() const;                                   ///< 图标边长，默认 40
    void setIconSize(int size);
    int iconSpacing() const;                                ///< 图标与文本间距，默认 14
    void setIconSpacing(int spacing);
    QColor iconInformationColor() const;                    ///< 内置信息图标徽章色
    void setIconInformationColor(const QColor &color);
    QColor iconQuestionColor() const;                       ///< 内置询问图标徽章色
    void setIconQuestionColor(const QColor &color);
    QColor iconWarningColor() const;                        ///< 内置警告图标徽章色
    void setIconWarningColor(const QColor &color);
    QColor iconCriticalColor() const;                       ///< 内置错误图标徽章色
    void setIconCriticalColor(const QColor &color);

    // ---- 内容区布局 ----
    int contentMargin() const;                              ///< 内容四周留白，默认 24
    void setContentMargin(int margin);
    int contentSpacing() const;                             ///< 文本块 / 自定义内容 / 按钮行之间的行距，默认 16
    void setContentSpacing(int spacing);
    int minContentWidth() const;                            ///< 内容区最小宽度（窗体过窄时的下限），默认 320
    void setMinContentWidth(int width);
    int maxContentWidth() const;                            ///< 文本自动换行宽度上限，默认 520
    void setMaxContentWidth(int width);

    // ---- 按钮（标准按钮与 addButton 自定义按钮统一取值） ----
    QFont buttonFont() const;                               ///< 按钮字体（默认微软雅黑 12px）
    void setButtonFont(const QFont &font);
    int buttonMinWidth() const;                             ///< 单个按钮最小宽度，默认 72
    void setButtonMinWidth(int width);
    int buttonHeight() const;                               ///< 按钮高度，默认 30
    void setButtonHeight(int height);
    int buttonSpacing() const;                              ///< 按钮间距，默认 8
    void setButtonSpacing(int spacing);
    int buttonRadius() const;                               ///< 按钮圆角半径，默认 4
    void setButtonRadius(int radius);
    int buttonPadding() const;                              ///< 按钮文字左右留白（参与宽度计算），默认 18
    void setButtonPadding(int padding);
    QColor buttonBackground() const;                        ///< 常态背景（无效色 = 透明底）
    void setButtonBackground(const QColor &color);
    QColor buttonHoverBackground() const;                   ///< 悬停背景（无效色 = 沿用常态背景）
    void setButtonHoverBackground(const QColor &color);
    QColor buttonPressBackground() const;                   ///< 按下背景（无效色 = 沿用常态背景）
    void setButtonPressBackground(const QColor &color);
    QColor buttonBorderColor() const;                       ///< 按钮描边（无效色 = 无边框）
    void setButtonBorderColor(const QColor &color);
    QColor buttonTextColor() const;                         ///< 按钮文字颜色
    void setButtonTextColor(const QColor &color);
    QColor buttonDisabledTextColor() const;                 ///< 按钮禁用文字颜色
    void setButtonDisabledTextColor(const QColor &color);

    // ---- 默认按钮（主按钮，Windows 习惯的高亮形态） ----
    QColor defaultButtonBackground() const;                 ///< 主按钮常态背景
    void setDefaultButtonBackground(const QColor &color);
    QColor defaultButtonHoverBackground() const;            ///< 主按钮悬停背景
    void setDefaultButtonHoverBackground(const QColor &color);
    QColor defaultButtonPressBackground() const;            ///< 主按钮按下背景
    void setDefaultButtonPressBackground(const QColor &color);
    QColor defaultButtonTextColor() const;                  ///< 主按钮文字颜色
    void setDefaultButtonTextColor(const QColor &color);

private:
    friend class AuMessageBox;
    static void registerGlobalObserver(AuMessageBox *box);
    static void unregisterGlobalObserver(AuMessageBox *box);
    static void notifyGlobalStyleChanged();

    QFont m_messageFont;
    QColor m_messageColor;
    QFont m_informativeFont;
    QColor m_informativeColor;

    int m_iconSize;
    int m_iconSpacing;
    QColor m_iconInformationColor;
    QColor m_iconQuestionColor;
    QColor m_iconWarningColor;
    QColor m_iconCriticalColor;

    int m_contentMargin;
    int m_contentSpacing;
    int m_minContentWidth;
    int m_maxContentWidth;

    QFont m_buttonFont;
    int m_buttonMinWidth;
    int m_buttonHeight;
    int m_buttonSpacing;
    int m_buttonRadius;
    int m_buttonPadding;
    QColor m_buttonBackground;
    QColor m_buttonHoverBackground;
    QColor m_buttonPressBackground;
    QColor m_buttonBorderColor;
    QColor m_buttonTextColor;
    QColor m_buttonDisabledTextColor;

    QColor m_defaultButtonBackground;
    QColor m_defaultButtonHoverBackground;
    QColor m_defaultButtonPressBackground;
    QColor m_defaultButtonTextColor;

    static AuMessageBoxStyle *s_globalStyle;
    static QVector<AuMessageBox *> s_observers;
};

#endif // AUMESSAGEBOXSTYLE_H
