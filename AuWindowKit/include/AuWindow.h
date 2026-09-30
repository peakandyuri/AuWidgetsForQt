#ifndef AUWINDOW_H
#define AUWINDOW_H

#include "AuWindowKit_global.h"
#include "AuTitleBar.h"

#include <QPointer>
#include <QWidget>

class QLayout;
class QEventLoop;
class AuWindowStyle;
class AuSnapPreview;
class QVBoxLayout;

/**
 * @brief 无边框窗体：以系统 API 为主、Qt 渲染为辅，替代 QMainWindow / QDialog
 *        的统一风格窗体容器（继承自 QWidget）。
 *
 * 平台实现策略：
 *  - Windows：保留 WS_THICKFRAME / WS_SYSMENU 等原生样式并剥离 WS_CAPTION，
 *    经 DwmExtendFrameIntoClientArea 保住 DWM 框架阴影，通过 WM_NCCALCSIZE
 *    去掉可见边框、WM_NCHITTEST 把标题栏空白区标记为 HTCAPTION。由此免费
 *    获得系统级特性：DWM 阴影、原生最小化 / 最大化动画、拖动到屏幕上 /
 *    左 / 右边缘的系统分屏与最大化（Aero Snap）、双击标题栏最大化、
 *    Alt+Space 系统菜单、任务栏实时缩略图；缩放由系统边框热区完成，
 *    最小宽高经 WM_GETMINMAXINFO 生效，最大化目标为工作区（不遮挡任务栏）。
 *    圆角仅 Win11 生效（DWM 圆角偏好 + 阴影并存）；Win10 上为保留系统
 *    阴影降级为直角（SetWindowRgn 区域圆角会关掉 DWM 框架渲染，弃用）。
 *  - Linux/X11：Qt 无边框 + 透明背景，拖动与缩放通过 _NET_WM_MOVERESIZE
 *    交给窗口管理器（在支持的 WM 上同样具有边缘分屏）。阴影说明：X11 对
 *    无边框窗口没有窗口管理器侧的阴影通道（KWin 系的 _KDE_NET_WM_SHADOW
 *    协议要求客户端自制贴图供图，非 WM 主题原生阴影），按需求本模块不做
 *    任何客户端阴影，Linux 下窗体为纯内容矩形无阴影（Windows 的 DWM
 *    原生阴影不受影响）。圆角在混成下由 ARGB alpha 平滑呈现，无混成时
 *    区域裁剪；缩放尊重 WM_NORMAL_HINTS 中的最小尺寸。
 *  - 其它平台：退化为手动拖动 / 缩放（同样钳制最小宽高），无阴影。
 *
 * 使用方式与 QMainWindow 贴合：
 *  - setWindowTitle() / setWindowIcon() 直接同步到自绘标题栏；
 *  - setCentralWidget() 设置内容控件；也可以在子类中直接 ui->setupUi(this)，
 *    窗体会自动把根布局转移到标题栏下方的内容区（见下方"继承定制"）；
 *  - show() / showMaximized() / showMinimized() / close() 等 QWidget 行为不变。
 *
 * 继承定制（推荐工程做法）：
 * \code
 * // MyWin.h —— 业务窗体基类
 * class MyWin : public AuWindow {
 *     Q_OBJECT
 * public:
 *     explicit MyWin(QWidget *parent = nullptr) : AuWindow(parent) {
 *         setStyle(AuWindowStyle::createDarkStyle());     // 统一主题
 *         setWindowRadius(8);                              // 统一圆角
 *         setButtonVisible(AuTitleBar::ButtonMaximize, false);
 *     }
 * };
 *
 * // 业务窗口：直接用 Qt Designer 设计，setupUi(this) 即可自动带标题栏
 * class SettingsPage : public MyWin {
 *     Q_OBJECT
 * public:
 *     explicit SettingsPage(QWidget *parent = nullptr) : MyWin(parent) {
 *         ui->setupUi(this);      // 布局被自动转移到标题栏下方内容区
 *     }
 * private:
 *     Ui::SettingsPage *ui = new Ui::SettingsPage;
 * };
 * \endcode
 */
class AUWINDOWKIT_EXPORT AuWindow : public QWidget
{
    Q_OBJECT

public:
    /// 模态范围：父窗口模态（阻塞父窗口链）或全局模态（阻塞整个应用）。
    enum class Modality {
        WindowModal,     ///< 父窗口模态
        ApplicationModal ///< 全局模态
    };

    /// done()/exec() 返回码，数值与 QDialog 一致。
    enum DialogCode {
        Rejected = 0,
        Accepted = 1
    };

    explicit AuWindow(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::Window);
    ~AuWindow() override;

    // ---- 内容区（QMainWindow 风格） ----
    /// 内容区容器：位于标题栏下方，可向其中添加布局或控件。
    QWidget *clientWidget() const;
    void setCentralWidget(QWidget *widget);
    QWidget *centralWidget() const;

    // ---- 标题栏 ----
    AuTitleBar *titleBar() const;
    /// 替换为自定义标题栏（通常是 AuTitleBar 子类）；窗体接管所有权。
    /// 传入 nullptr 无效。
    void setTitleBar(AuTitleBar *bar);

    /// 标题栏自定义组件：插入到指定槽位（靠左 / 居中 / 靠右）。
    /// grabMouse = false 时组件不拦截鼠标，区域仍可拖动窗体（纯展示组件）。
    void addTitleBarWidget(QWidget *widget, AuTitleBar::Slot slot, bool grabMouse = true);
    void removeTitleBarWidget(QWidget *widget);
    /// 切换已插入组件是否抢占鼠标事件。
    void setTitleBarWidgetGrabMouse(QWidget *widget, bool grab);

    // ---- 操作按钮 ----
    void setButtonVisible(AuTitleBar::Button button, bool visible);
    bool isButtonVisible(AuTitleBar::Button button) const;

    // ---- 圆角 ----
    /// 窗体圆角半径（四角统一），默认 0（直角）。Win11 上由系统提供圆角与
    /// 配套阴影：1~5 映射为系统小圆角，>5 映射为系统标准圆角；Win10 上为
    /// 保留 DWM 原生阴影降级为直角（无视觉效果）；其它平台按像素裁剪。
    /// 四角独立控制见 setCornerRadius()。
    void setWindowRadius(int radius);
    /// 四角中的最大圆角半径（独立设置各角后返回最大者；Windows 系统圆角
    /// 映射以此为基准）。
    int windowRadius() const;
    /// 设置单个角的圆角半径（Qt::Corner 指定角），其余角不变。
    /// 可实现"仅顶部圆角""顶部大圆角 + 底部小圆角"等形态。
    /// Windows 上系统圆角仅支持统一值：Win11 按最大角映射 DWM 偏好，
    /// Win10 直角（见 setWindowRadius 说明）。
    void setCornerRadius(Qt::Corner corner, int radius);
    /// 单角圆角半径。
    int cornerRadius(Qt::Corner corner) const;

    // ---- 关闭拦截 ----
    /// 开启后 close() / 关闭按钮 / 系统关闭消息均被拦截（QCloseEvent 被忽略），
    /// 同时发出 closeIntercepted() 信号；由业务决定何时 forceClose() 真正关闭。
    void setCloseInterceptionEnabled(bool enable);
    bool isCloseInterceptionEnabled() const;
    /// 无视拦截直接关闭（exec 模式下同时退出模态循环）。
    void forceClose();

    // ---- X11 系统移动（可选） ----
    /// 是否把标题栏拖动 / 边框缩放交给窗口管理器（X11 经 startSystemMove/
    /// startSystemResize，Windows 不受影响）。
    /// Qt 5.15+：默认开启——拖动 / 缩放 / 贴靠（Quick Tiling）全部由 WM
    /// 原生完成，模块内置吸附预览不再参与。已知特性：WM 接管后本进程
    /// 仅 motion/enter 事件不再投递（QTBUG-69716 族，hover 失效），
    /// 几何同步不受影响。
    /// Qt 5.12：默认关闭——该版本无 startSystemMove 且接管后 ConfigureNotify
    /// 一并丢失（几何脱节），使用 Qt 手动拖动 / 缩放 + 模块内置边缘吸附
    /// （拖顶最大化 / 左右半屏 / 铺屏预览 / 贴靠后拖出自动还原）。
    /// 两种通道均可运行时切换。
    void setSystemWindowMoveEnabled(bool enable);
    bool isSystemWindowMoveEnabled() const;

    // ---- 边缘吸附（手动拖动路径） ----
    /// 手动拖动松开时按光标位置贴靠：拖到屏幕顶部 → 最大化（仅当标题栏
    /// 显示最大化按钮），左 / 右边缘 → 贴边半屏；拖动过程中光标进入吸附带
    /// 时显示半透明铺屏预览。默认开启。
    void setEdgeSnapEnabled(bool enable);
    bool isEdgeSnapEnabled() const;

    // ---- 任务栏 ----
    /// 是否在任务栏显示图标（显示时 Windows 自动带实时缩略图预览）。
    /// 隐藏后同时不出现在 Alt+Tab。需在 show() 前设置，运行中切换立即生效。
    void setTaskbarVisible(bool visible);
    bool isTaskbarVisible() const;

    // ---- Dialog 方式展示 ----
    /// 切换 Dialog 形态（Qt::Dialog 标志：带父窗口时作为对话框、居中于父窗口）。
    /// 需在 show() 之前调用（切换会重建原生窗口）。
    void setDialogMode(bool dialog);
    bool isDialogMode() const;
    /// 模态范围，配合 exec() / 模态 show() 使用。
    void setModality(Modality modality);
    Modality modality() const;
    /// 模态阻塞执行（QDialog::exec 风格）。未显式设置模态范围时，
    /// 有父窗口默认 WindowModal，无父窗口默认 ApplicationModal。
    /// 返回值由 setResult()/accept()/reject()/done() 决定。
    /// exec() 期间键盘交互与 QDialog 一致：Esc 触发 reject()，
    /// Enter/Return 触发当前焦点的 QPushButton，焦点不在按钮上时触发
    /// setDefault(true) 的默认按钮。
    int exec();
    void done(int result);
    void accept();
    void reject();
    int result() const;
    void setResult(int result);

    // ---- 样式 ----
    /// 设置独立样式（窗体接管所有权）；nullptr 表示恢复跟随全局样式。
    void setStyle(AuWindowStyle *style);
    AuWindowStyle *style() const;

signals:
    /// 关闭被拦截（事件已忽略），业务处理完（如二次确认）后调用 forceClose()。
    void closeIntercepted();
    /// exec() 结束（done/accept/reject/close 都会触发）。
    void finished(int result);
    void accepted();
    void rejected();
    /// 最大化 / 还原状态变化。
    void maximizeStateChanged(bool maximized);

protected:
    /// 手动触发整体重排（标题栏显隐等场景）。一般无需调用。
    void relayout();
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void childEvent(QChildEvent *event) override;
    bool event(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif

private:
    friend class AuTitleBar;
    friend class AuWindowStyle;
    friend class AuEdgeGrip;

    void onGlobalStyleChanged();
    void updateMask();
    int effectiveMargin() const;
    void ensurePolishedFrame();
    void connectTitleBar();
    /// 四角圆角裁剪浮层：按内容区几何摆放，清除不透明子控件盖住的圆角外像素
    void updateCornerClips(const QRect &content);
    /// X11：系统移动/缩放被 WM 接管会吞掉 release 事件（Qt 内部隐式抓取停在
    /// 按下控件上，hover/光标失效，直至下一次点击）。开始轮询物理按键，检测到
    /// 松开且 release 未到达时，直接向 Qt 事件系统补发一次鼠标 release
    /// （不经 X 输入管线，不影响 KWin 的 snap-assist 等交互辅助层）。
    void startGrabRecoveryPoll();
    void pollGrabRecovery();
    /// 任意真实 MouseButtonRelease 到达时回调（清恢复标志）
    void noteMouseRelease();

    // 布局劫持：把子类 setupUi(this) 安装到窗体上的布局转移到内容区
    void adoptLayout(QLayout *layout);

    // 平台相关：标题栏拖动 / 边框缩放（非 Windows 路径）
    void performTitleBarDrag(const QPoint &pressGlobal);
    /// pressGlobal：按下时刻的全局光标位置，用于兜底拖动时贴合整体位移
    void startManualMove(const QPoint &pressGlobal = QCursor::pos());
    void startManualResize(Qt::Edges edges, const QPoint &pressGlobal = QCursor::pos());
    void manualResizeStep(const QPoint &globalPos);
    /// 发起边缘缩放：优先 _NET_WM_MOVERESIZE（KWin 原生路径），WM 忽略时回退手动
    void performBorderResize(Qt::Edges edges, const QPoint &pressGlobal);
    bool m_systemResizePending;    ///< X11：已发系统缩放请求、等待确认 WM 是否接管
    qint64 m_systemResizeSentMs;   ///< 系统缩放请求发出时刻（宽限期判定）
    Qt::Edges m_pendingEdges;
    QPoint m_pressGlobal;          ///< 左键按下时刻的全局光标位置
    /// X11：经 _NET_MOVERESIZE_WINDOW（仅 x 字段）由 WM 应用窗口 x 位置
    /// （可拖出屏幕左右，与原生一致）；y 经只带 CWY 的 XConfigureWindow
    /// 屏内跟随。WM 忽略时首次校验失败自动回退 QWidget::move()。
    void applyPositionViaWM(const QPoint &pos);
    bool sendNetMoveResizeWindow(const QPoint &pos);
    /// X11：创建边缘缩放手柄（透明子控件，自带缩放光标）
    void setupEdgeGrips();
    void relayoutEdgeGrips();
    /// 手动拖动路径的边缘吸附：目标判定 / 预览浮层 / 松手应用
    enum class SnapEdge { None, Top, Left, Right };
    SnapEdge edgeSnapTarget(const QPoint &globalPos) const;
    QRect edgeSnapRect(SnapEdge edge) const;
    void showSnapPreview(SnapEdge edge);
    void hideSnapPreview();
    void applyEdgeSnap(SnapEdge edge);

#ifdef Q_OS_WIN
    // Windows 原生辅助
    void applyWindowStyles();
    void applyWindowRoundedCorners();
    void applyTaskbarVisibility();
    bool handleNcCalcSize(void *message, long *result);
    bool handleNcHitTest(void *message, long *result);
    bool handleMinMaxInfo(void *message, long *result);
#endif

    AuWindowStyle *m_style;             // nullptr = 跟随全局样式
    AuTitleBar *m_titleBar;
    QWidget *m_client;
    QVBoxLayout *m_clientLayout;        // 内容区占位布局（被用户布局替换后置空）
    QLayout *m_adoptedLayout;           // 已劫持的用户布局
    QWidget *m_centralWidget;

    int m_cornerRadii[4];               // 四角圆角半径（Qt::Corner 顺序），全部相同即统一圆角
    bool m_closeInterception;
    bool m_forceClosing;
    bool m_taskbarVisible;
    bool m_dialogMode;
    int m_result;
    QEventLoop *m_eventLoop;

    bool m_shownOnce;
    bool m_dwmCornerOk;                 // Windows：DWM 圆角偏好是否可用（Win11+）
    bool m_x11Compositing;              // X11：透明是否实际生效（决定平滑圆角 / mask）
    bool m_x11CheckedTranslucency;      // X11：是否已在首显时检测过透明能力

    // 手动拖动 / 缩放状态（无系统 move-resize 能力时的兜底路径）
    bool m_manualResizing;
    bool m_manualMoving;
    QPoint m_manualStartGlobal;
    QPoint m_manualStartPos;
    QSize m_manualStartSize;
    Qt::Edges m_manualEdges;
    bool m_systemMoveEnabled;      ///< X11：是否使用 _NET_WM_MOVERESIZE 系统移动
    bool m_edgeSnapEnabled;        ///< 手动拖动路径：边缘吸附（拖顶最大化 / 左右半屏）
    AuSnapPreview *m_snapPreview;  ///< 手动拖动的吸附预览浮层（懒创建）
    bool m_snapped;                ///< 当前处于边缘吸附状态（拖动标题栏时还原）
    QRect m_preSnapGeometry;       ///< 吸附前的浮动几何（还原跟随基准）
    bool m_wmGeometryBroken;       ///< X11：_NET_MOVERESIZE_WINDOW 不可用（已回退）
    bool m_wmGeometryPending;      ///< X11：正在等待一次 WM 位置校验
    QVector<QWidget *> m_edgeGrips; ///< 边缘缩放手柄（透明子控件）
    QWidget *m_cornerClips[4];     ///< 四角圆角裁剪浮层（懒创建，仅混成下使用）
    QTimer *m_grabRecoveryTimer;   ///< X11：系统接管后轮询物理按键状态（补发 release）
    bool m_awaitingRelease;        ///< X11：系统通道的 release 被 WM 吞掉，等待补发
};

#endif // AUWINDOW_H
