#ifndef AUPUSHBUTTON_H
#define AUPUSHBUTTON_H

#include "AuWindowKit_global.h"

#include <QPushButton>

/**
 * @brief 自绘命令按钮：QPainter 绘制背景 / 描边 / 文字，不依赖平台 QStyle，
 *        跨平台观感像素级一致（AuMessageBox 的按钮行即由本控件构成）。
 *
 * 继承自 QPushButton：焦点 / 空格激活 / 助记符等按键语义与文本按钮一致，
 * AuWindow::exec() 的回车触发逻辑（qobject_cast<QPushButton*> + isDefault()）
 * 直接可用。视觉全部由属性驱动，且提供"主按钮"（highlight）形态——强调色
 * 背景 + 反白文字，对应消息框的默认按钮。
 *
 * 属性既可由 AuMessageBoxStyle 批量下发，也可单独设置（单独复用时）。
 */
class AUWINDOWKIT_EXPORT AuPushButton : public QPushButton
{
    Q_OBJECT

public:
    explicit AuPushButton(QWidget *parent = nullptr);
    explicit AuPushButton(const QString &text, QWidget *parent = nullptr);
    ~AuPushButton() override;

    // ---- 主按钮形态 ----
    /// 主按钮：使用 defaultButton 系列颜色（强调背景 + 反白文字）。
    void setHighlight(bool highlight);
    bool isHighlight() const;

    // ---- 视觉属性 ----
    void setBackground(const QColor &color);            ///< 常态背景（无效色 = 透明底）
    void setHoverBackground(const QColor &color);       ///< 悬停背景（无效色 = 沿用常态）
    void setPressBackground(const QColor &color);       ///< 按下背景（无效色 = 沿用常态）
    void setBorderColor(const QColor &color);           ///< 描边（无效色 = 无边框）
    void setTextColor(const QColor &color);
    void setDisabledTextColor(const QColor &color);
    void setRadius(int radius);                         ///< 圆角半径，0 = 直角
    void setContentPadding(int padding);                ///< 文字左右留白（参与 sizeHint）

    QColor background() const { return m_background; }
    QColor hoverBackground() const { return m_hoverBackground; }
    QColor pressBackground() const { return m_pressBackground; }
    QColor borderColor() const { return m_borderColor; }
    QColor textColor() const { return m_textColor; }
    QColor disabledTextColor() const { return m_disabledTextColor; }
    int radius() const { return m_radius; }
    int contentPadding() const { return m_padding; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    QColor effectiveBackground() const;
    QColor focusRingColor() const;

    bool m_highlight;
    int m_radius;
    int m_padding;
    QColor m_background;
    QColor m_hoverBackground;
    QColor m_pressBackground;
    QColor m_borderColor;
    QColor m_textColor;
    QColor m_disabledTextColor;
};

#endif // AUPUSHBUTTON_H
