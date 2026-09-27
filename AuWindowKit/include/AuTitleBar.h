#ifndef AUTITLEBAR_H
#define AUTITLEBAR_H

#include "AuWindowKit_global.h"

#include <QIcon>
#include <QWidget>

class AuWindow;

/**
 * @brief 无边框窗体标题栏：图标 + 标题文字 + 三个操作按钮（最小化 / 最大化 / 关闭）
 *        全部由 QPainter 自绘，支持在三个槽位插入自定义组件。
 *
 * 槽位（addWidget 的 slot 参数）：
 *  - SlotAfterTitle    靠左：紧跟标题文字之后；
 *  - SlotCenter        居中：标题与操作按钮之间空白区域的中点；
 *  - SlotBeforeButtons 靠右：紧贴操作按钮左侧。
 *
 * 插入的自定义组件默认"抢占"鼠标事件（按钮 / 下拉框等交互组件可直接点击），
 * 此时组件区域不响应标题栏拖动；对纯展示组件可传 grabMouse = false
 * （或后续调用 setWidgetGrabMouse(widget, false)），鼠标事件将穿透组件、
 * 该区域仍可拖动窗体。
 *
 * 继承定制：可子类化重写 paintEvent 绘制自定义底色 / 装饰，再通过
 * AuWindow::setTitleBar() 替换默认标题栏；按钮点击、拖动、双击最大化等
 * 交互逻辑由基类提供。
 *
 * 交互行为（与系统原生标题栏一致）：
 *  - 按住空白区域拖动移动窗体（Windows 下由系统非客户区消息驱动，
 *    天然支持拖到屏幕边缘最大化 / 半屏；其它平台延迟 4px 触发系统移动）；
 *  - 双击空白区域切换最大化 / 还原；
 *  - 最小化 / 最大化 / 关闭按钮点击发对应信号，由 AuWindow 执行。
 */
class AUWINDOWKIT_EXPORT AuTitleBar : public QWidget
{
    Q_OBJECT

public:
    /// 操作按钮位标志，可用组合设置可见按钮。
    enum Button {
        ButtonNone      = 0x0,
        ButtonMinimize  = 0x1,
        ButtonMaximize  = 0x2,
        ButtonClose     = 0x4,
        ButtonAll       = ButtonMinimize | ButtonMaximize | ButtonClose
    };
    Q_DECLARE_FLAGS(Buttons, Button)

    /// 自定义组件槽位。
    enum Slot {
        SlotAfterTitle,     ///< 靠左：标题文字之后
        SlotCenter,         ///< 居中：标题与按钮之间空白区域中间
        SlotBeforeButtons   ///< 靠右：操作按钮左侧
    };

    explicit AuTitleBar(AuWindow *window);

    /// 所属窗体。
    AuWindow *hostWindow() const { return m_window; }

    // ---- 图标与标题 ----
    void setIcon(const QIcon &icon);
    QIcon icon() const { return m_icon; }
    void setTitle(const QString &title);
    QString title() const { return m_title; }

    // ---- 操作按钮 ----
    void setButtons(Buttons buttons);
    Buttons buttons() const { return m_buttons; }
    void setButtonVisible(Button button, bool visible);
    bool isButtonVisible(Button button) const;
    /// 窗体是否处于最大化状态（切换最大化 / 还原图标），由 AuWindow 维护。
    void updateMaximizeState(bool maximized);

    // ---- 自定义组件槽位 ----
    /// 插入组件到指定槽位；grabMouse 为 false 时组件不拦截鼠标（可穿透拖动）。
    void addWidget(QWidget *widget, Slot slot, bool grabMouse = true);
    void removeWidget(QWidget *widget);
    /// 动态切换组件是否抢占鼠标事件；false = 穿透（纯展示组件，可拖动）。
    void setWidgetGrabMouse(QWidget *widget, bool grab);
    /// 组件当前是否抢占鼠标事件（未插入的组件返回 false）。
    bool isWidgetGrabMouse(QWidget *widget) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /**
     * @brief 命中测试：标题栏内 pos 处是否为"空白可拖动区"。
     *
     * 供 Windows 平台 WM_NCHITTEST 使用：按钮、交互子组件处返回 false
     * （作为客户区，正常接收 Qt 鼠标事件），空白处返回 true（作为系统标题栏，
     * 由系统接管拖动 / Aero Snap / 双击最大化）。
     */
    bool hitTestCaption(const QPoint &pos) const;

signals:
    void minimizeButtonClicked();
    void maximizeButtonClicked();
    void closeButtonClicked();
    void titleDoubleClicked();      ///< 双击空白区域（切换最大化 / 还原）

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend class AuWindow;

    void relayoutSections();
    /// 清除拖动状态（release 被 WM 吞掉时的卡死恢复用）
    void resetDragState();
    QWidget *sectionWidget(Slot slot) const { return m_sections[slot]; }
    Button buttonAt(const QPoint &pos) const;
    int visibleButtonCount() const;
    int buttonsBlockWidth() const;
    void drawButtons(QPainter &painter);
    void drawButtonGlyph(QPainter &painter, Button button, const QRect &rect, const QColor &color);

    AuWindow *m_window;
    QIcon m_icon;
    QString m_title;
    Buttons m_buttons;
    bool m_maximized;

    QWidget *m_sections[3];        ///< 三槽位容器（自身对鼠标透明，空白可拖动）

    // 拖动状态：非 Windows 平台延迟触发系统移动（移动阈值前先等双击）
    bool m_dragPending;
    bool m_systemDragPending;      ///< 已发系统移动请求、等待确认 WM 是否接管
    qint64 m_systemDragSentMs;     ///< 系统移动请求发出时刻（宽限期判定）
    QPoint m_dragStartPos;
    // 按钮状态
    Button m_hoverButton;
    Button m_pressedButton;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AuTitleBar::Buttons)

#endif // AUTITLEBAR_H
