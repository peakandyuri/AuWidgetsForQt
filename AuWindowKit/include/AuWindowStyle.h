#ifndef AUWINDOWSTYLE_H
#define AUWINDOWSTYLE_H

#include "AuWindowKit_global.h"

#include <QColor>
#include <QFont>
#include <QVector>

class AuWindow;

/**
 * @brief AuWindow / AuTitleBar 样式：窗体背景、标题栏、操作按钮、阴影的全部视觉属性。
 *
 * 三种使用方式（与 AuSelectStyle 等控件保持一致）：
 *  1. 全局样式：AuWindowStyle::setGlobalStyle() 设置后，所有未显式 setStyle() 的
 *     AuWindow 自动使用，且全局样式变化时已注册窗体自动刷新；
 *  2. 单窗体独立样式：AuWindow::setStyle(new AuWindowStyle())（窗体接管所有权），
 *     随后可对实例逐项微调，例如 window->style()->setTitleBarHeight(40);
 *  3. 内置主题工厂：createLightStyle() / createDarkStyle()。
 *
 * 说明：
 *  - 窗体阴影：Windows 由系统 DWM 原生提供（无需也无法经此样式定制）；
 *    X11 对无边框窗口没有窗口管理器侧的阴影通道（_KDE_NET_WM_SHADOW
 *    需客户端供图，非原生主题阴影），按需求本模块不做客户端阴影，
 *    shadow 系列字段当前在所有平台均无视觉效果（保留字段，暂勿依赖）。
 *  - 颜色字段为无效 QColor 时表示不绘制对应元素（如关闭窗体描边）。
 */
class AUWINDOWKIT_EXPORT AuWindowStyle
{
public:
    AuWindowStyle();
    virtual ~AuWindowStyle();

    // 内置主题工厂（所有权归调用者）
    static AuWindowStyle *createLightStyle();
    static AuWindowStyle *createDarkStyle();

    // 全局样式：未显式设置样式的 AuWindow 自动使用
    static AuWindowStyle *globalStyle();
    static void setGlobalStyle(AuWindowStyle *style);   // 所有权归调用者

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

    // ---- 阴影（当前无视觉效果：Windows 由 DWM 原生提供，其余平台不做客户端阴影） ----
    int shadowMargin() const;                           ///< 阴影外扩留白（保留字段）
    void setShadowMargin(int margin);
    int shadowBlurRadius() const;                       ///< 阴影模糊半径（保留字段）
    void setShadowBlurRadius(int radius);
    QColor shadowColor() const;                         ///< 阴影颜色（保留字段）
    void setShadowColor(const QColor &color);

private:
    friend class AuWindow;
    static void registerGlobalObserver(AuWindow *window);
    static void unregisterGlobalObserver(AuWindow *window);
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

    static AuWindowStyle *s_globalStyle;
    static QVector<AuWindow *> s_observers;
};

#endif // AUWINDOWSTYLE_H
