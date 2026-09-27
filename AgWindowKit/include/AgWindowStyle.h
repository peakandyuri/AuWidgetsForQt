#ifndef AGWINDOWSTYLE_H
#define AGWINDOWSTYLE_H

#include "AgWindowKit_global.h"

#include <QColor>
#include <QFont>
#include <QVector>

class AgWindow;

/**
 * @brief AgWindow / AgTitleBar 样式：窗体背景、标题栏、操作按钮、阴影的全部视觉属性。
 *
 * 三种使用方式（与 AgSelectStyle 等控件保持一致）：
 *  1. 全局样式：AgWindowStyle::setGlobalStyle() 设置后，所有未显式 setStyle() 的
 *     AgWindow 自动使用，且全局样式变化时已注册窗体自动刷新；
 *  2. 单窗体独立样式：AgWindow::setStyle(new AgWindowStyle())（窗体接管所有权），
 *     随后可对实例逐项微调，例如 window->style()->setTitleBarHeight(40);
 *  3. 内置主题工厂：createLightStyle() / createDarkStyle()。
 *
 * 说明：
 *  - Windows 平台窗体阴影由系统 DWM 提供，shadow 系列字段在该平台被忽略；
 *    其余平台（X11 等）没有系统级无标题栏窗体阴影，按这些字段自绘 CSD 风格阴影。
 *  - 颜色字段为无效 QColor 时表示不绘制对应元素（如关闭窗体描边）。
 */
class AGWINDOWKIT_EXPORT AgWindowStyle
{
public:
    AgWindowStyle();
    virtual ~AgWindowStyle();

    // 内置主题工厂（所有权归调用者）
    static AgWindowStyle *createLightStyle();
    static AgWindowStyle *createDarkStyle();

    // 全局样式：未显式设置样式的 AgWindow 自动使用
    static AgWindowStyle *globalStyle();
    static void setGlobalStyle(AgWindowStyle *style);   // 所有权归调用者

    // ---- 窗体 ----
    QColor windowBackgroundColor() const;               ///< 内容区背景
    void setWindowBackgroundColor(const QColor &color);
    QColor windowBorderColor() const;                   ///< 窗体 1px 描边（无效色 = 不绘制）
    void setWindowBorderColor(const QColor &color);

    // ---- 标题栏 ----
    int titleBarHeight() const;                         ///< 标题栏高度，默认 32
    void setTitleBarHeight(int height);
    QColor titleBarBackgroundColor() const;             ///< 激活态标题栏背景
    void setTitleBarBackgroundColor(const QColor &color);
    QColor titleBarInactiveBackgroundColor() const;     ///< 非激活态标题栏背景
    void setTitleBarInactiveBackgroundColor(const QColor &color);
    QColor titleForegroundColor() const;                ///< 激活态标题文字
    void setTitleForegroundColor(const QColor &color);
    QColor titleInactiveForegroundColor() const;        ///< 非激活态标题文字
    void setTitleInactiveForegroundColor(const QColor &color);
    QFont titleFont() const;                            ///< 标题字体（默认微软雅黑 12px）
    void setTitleFont(const QFont &font);
    int titleBarLeftPadding() const;                    ///< 标题栏左侧留白（图标前）
    void setTitleBarLeftPadding(int padding);
    int titleIconSpacing() const;                       ///< 图标与标题文字间距
    void setTitleIconSpacing(int spacing);
    int titleIconSize() const;                          ///< 标题图标边长
    void setTitleIconSize(int size);

    // ---- 标题栏操作按钮（最小化 / 最大化 / 关闭） ----
    int titleBarButtonWidth() const;                    ///< 单个按钮宽度，默认 46
    void setTitleBarButtonWidth(int width);
    QColor buttonGlyphColor() const;                    ///< 按钮图标线条色
    void setButtonGlyphColor(const QColor &color);
    QColor buttonDisabledGlyphColor() const;            ///< 按钮不可用（或窗体禁用）时图标色
    void setButtonDisabledGlyphColor(const QColor &color);
    QColor buttonHoverBackgroundColor() const;          ///< 悬停背景（无效色 = 无悬停效果）
    void setButtonHoverBackgroundColor(const QColor &color);
    QColor buttonPressBackgroundColor() const;          ///< 按下背景
    void setButtonPressBackgroundColor(const QColor &color);
    QColor closeButtonHoverBackgroundColor() const;     ///< 关闭按钮悬停背景（默认红色）
    void setCloseButtonHoverBackgroundColor(const QColor &color);
    QColor closeButtonPressBackgroundColor() const;     ///< 关闭按钮按下背景
    void setCloseButtonPressBackgroundColor(const QColor &color);
    QColor closeButtonGlyphColor() const;               ///< 关闭按钮悬停 / 按下时图标色（默认白）
    void setCloseButtonGlyphColor(const QColor &color);

    // ---- 阴影（非 Windows 平台自绘时生效） ----
    int shadowMargin() const;                           ///< 阴影外扩留白，默认 12；<=0 关闭阴影
    void setShadowMargin(int margin);
    int shadowBlurRadius() const;                       ///< 阴影模糊半径
    void setShadowBlurRadius(int radius);
    QColor shadowColor() const;                         ///< 阴影颜色（可带透明度）
    void setShadowColor(const QColor &color);

private:
    friend class AgWindow;
    static void registerGlobalObserver(AgWindow *window);
    static void unregisterGlobalObserver(AgWindow *window);
    static void notifyGlobalStyleChanged();

    QColor m_windowBackgroundColor;
    QColor m_windowBorderColor;

    int m_titleBarHeight;
    QColor m_titleBarBackgroundColor;
    QColor m_titleBarInactiveBackgroundColor;
    QColor m_titleForegroundColor;
    QColor m_titleInactiveForegroundColor;
    QFont m_titleFont;
    int m_titleBarLeftPadding;
    int m_titleIconSpacing;
    int m_titleIconSize;

    int m_titleBarButtonWidth;
    QColor m_buttonGlyphColor;
    QColor m_buttonDisabledGlyphColor;
    QColor m_buttonHoverBackgroundColor;
    QColor m_buttonPressBackgroundColor;
    QColor m_closeButtonHoverBackgroundColor;
    QColor m_closeButtonPressBackgroundColor;
    QColor m_closeButtonGlyphColor;

    int m_shadowMargin;
    int m_shadowBlurRadius;
    QColor m_shadowColor;

    static AgWindowStyle *s_globalStyle;
    static QVector<AgWindow *> s_observers;
};

#endif // AGWINDOWSTYLE_H
