#ifndef AUWINDOW_H
#define AUWINDOW_H

#include "AuWindowKit_global.h"
#include "AuTitleBar.h"

#include <QColor>
#include <QImage>
#include <QPointer>
#include <QWidget>

class QLayout;
class QEventLoop;
class AuWindowStyle;
class AuSnapPreview;
class QVBoxLayout;

// nativeEvent 的 result 参数类型在 Qt6 由 long 改为 qintptr，
// 各平台实现统一用本别名声明，保证 Qt5 / Qt6 双版本编译
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using AuNativeEventResult = qintptr;
#else
using AuNativeEventResult = long;
#endif

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
 *    交给窗口管理器（在支持的 WM 上同样具有边缘分屏）。阴影：KWin 系
 *    窗口管理器（ukui-kwin / KWin）经 _KDE_NET_WM_SHADOW 协议由客户端
 *    提供阴影贴图、WM 在窗口外合成（ukui 生态同款），窗体几何不变、
 *    缩放零重绘。WM 不支持该协议 / 无混成时无阴影（不做客户端自绘
 *    兜底）。贴图参数见 AuWindowStyle 的 shadow 系列字段；X11 默认
 *    开启。圆角在混成下由 ARGB alpha 平滑呈现，无混成时区域裁剪；
 *    缩放尊重 WM_NORMAL_HINTS 中的最小尺寸。
 *  - 其它平台：退化为手动拖动 / 缩放（同样钳制最小宽高），无阴影
 *    （Windows 由 DWM 提供原生阴影）。
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

    // ---- 窗口间吸附（手动拖动路径） ----
    /// 拖动窗口靠近其它顶层窗口（间距 ≤10px）时自动磁吸贴合边缘，同时
    /// 对齐相邻边（无预览浮层，与麒麟原生行为一致）。吸附目标：X11 为
    /// 全系统所有顶层窗口（_NET_CLIENT_LIST 枚举，过滤最小化与面板 /
    /// 桌面等特殊类型）；其它平台为本进程的 AuWindow。仅手动拖动通道
    /// 生效——启用后标题栏拖动改走手动通道（Qt 5.15+ 的 WM Quick Tiling /
    /// Windows Aero Snap 在拖动时让位于本功能，关闭即恢复原生通道）。
    /// 默认跟随手动通道的默认状态：Qt 5.12 X11 默认开启，Qt 5.15+ X11
    /// 与 Windows 默认关闭。
    void setWindowSnapEnabled(bool enable);
    bool isWindowSnapEnabled() const;

    // ---- 任务栏 ----
    /// 是否在任务栏显示图标（显示时 Windows 自动带实时缩略图预览）。
    /// 隐藏后同时不出现在 Alt+Tab。需在 show() 前设置，运行中切换立即生效。
    void setTaskbarVisible(bool visible);
    bool isTaskbarVisible() const;

    // ---- 边框阴影 ----
    /// 是否启用边框阴影（参数取自样式：AuWindowStyle::shadowColor /
    /// shadowBlurRadius / shadowMargin）。阴影一律经系统 API 在窗体矩形
    /// 之外合成，窗体几何不受影响：
    ///  - Linux/X11（ukui-kwin / KWin，WM 支持 _KDE_NET_WM_SHADOW 且混成）：
    ///    阴影贴图交窗口管理器在窗口外合成（ukui 生态同款），窗体几何
    ///    不变、缩放零重绘开销；
    ///  - 其它平台 / 无混成 / WM 不支持该协议：无阴影（不做客户端自绘
    ///    兜底——自绘的阴影占窗体几何，体验差）。Windows 保留 DWM 原生
    ///    阴影，与本开关无关。
    /// 默认值：Linux 开启，其余平台关闭。
    void setBorderShadowEnabled(bool enabled);
    /// 当前是否启用边框阴影。注意无混成 / WM 不支持时阴影不会呈现
    /// （见上），此处返回的是开关本身。
    bool isBorderShadowEnabled() const;

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
    /// 传入 AuWindowStyle::globalStyle() 返回的全局指针时直接跳过不处理：
    /// 全局对象归 setGlobalStyle 管理（仅设置新全局样式时释放旧的），
    /// 不可被窗体接管；恢复跟随全局请显式传 nullptr。
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
    void setVisible(bool visible) override;
    void paintEvent(QPaintEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void childEvent(QChildEvent *event) override;
    bool event(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, AuNativeEventResult *result) override;

private:
    friend class AuTitleBar;
    friend class AuWindowStyle;
    friend class AuEdgeGrip;

    void onGlobalStyleChanged();
    void updateMask();
    int effectiveMargin() const;
    void ensurePolishedFrame();
    void connectTitleBar();

    /// 首次显示前的默认摆放：用户未显式定位（未调用过 move()/setGeometry()，
    /// 同 QDialog 的判定）时居中——有可见父窗口居中于父窗口，否则居中于
    /// 光标所在屏幕的工作区。
    void adjustDefaultPosition();
    /// 四角圆角裁剪浮层：按内容区几何摆放，清除不透明子控件盖住的圆角外像素
    void updateCornerClips(const QRect &content);
    /// X11：系统移动/缩放被 WM 接管会吞掉 release 事件（Qt 内部隐式抓取停在
    /// 按下控件上，hover/光标失效，直至下一次点击）。开始轮询物理按键，检测到
    /// 松开且 release 未到达时，直接向 Qt 事件系统补发一次鼠标 release
    /// （不经 X 输入管线，不影响 KWin 的 snap-assist 等交互辅助层）。
    void startGrabRecoveryPoll();
    void pollGrabRecovery();
    /// 原生拖动收场（每拍 250ms、几何稳定或达到 retries 上限后定基准）：
    /// 最大化 / 全屏收场保留拖动起点为浮动基准，常规收场（含原生贴靠）
    /// 取终点；期间又开新拖动作废
    void scheduleNativeDragSettle(int retries = 4);
    /// 布防还原纠正断言保护（500ms 窗，moveEvent 重断言，按下失效）
    void armNormalRestoreAssert(const QRect &target);
    /// 任意真实 MouseButtonRelease 到达时回调（清恢复标志）
    void noteMouseRelease();

    // 布局劫持：把子类 setupUi(this) 安装到窗体上的布局转移到内容区
    void adoptLayout(QLayout *layout);

    // 标题栏拖动 / 边框缩放：系统原生接管优先（平台钩子），失败回退手动
    void performTitleBarDrag(const QPoint &pressGlobal);
    /// pressGlobal：按下时刻的全局光标位置，用于兜底拖动时贴合整体位移
    void startManualMove(const QPoint &pressGlobal = QCursor::pos());
    void startManualResize(Qt::Edges edges, const QPoint &pressGlobal = QCursor::pos());
    void manualResizeStep(const QPoint &globalPos);
    /// 最大化拖拽还原（手动通道）的落位收养：WM 取消最大化落定后（首个
    /// Move / 首次拖拽运动），以其落点为 1:1 跟随基准并纠正过期记忆尺寸。
    /// 返回是否已落地（未落地时调用方必须继续抑制几何输出）
    bool adoptManualRestorePosition();
    /// 最大化态拖标题栏还原的目标矩形：最大化前浮动几何 + 抓取点比例锚定；
    /// 无已知几何时无效（收养 WM 落位兜底）
    QRect maximizeRestoreTarget(const QPoint &pressGlobal) const;
    /// 发起边缘缩放：优先系统原生缩放，未接管时回退手动
    void performBorderResize(Qt::Edges edges, const QPoint &pressGlobal);
    bool m_systemResizePending;    ///< X11：已发系统缩放请求、等待确认 WM 是否接管
    qint64 m_systemResizeSentMs;   ///< 系统缩放请求发出时刻（宽限期判定）
    Qt::Edges m_pendingEdges;
    QPoint m_pressGlobal;          ///< 左键按下时刻的全局光标位置
    QSize m_normalSizeHint;        ///< 非最大化浮动尺寸，最大化还原后纠正 WM 过期记忆几何
    QRect m_normalGeometry;        ///< 非最大化浮动几何（含位置）：最大化还原落位由 WM
                                    ///< 记忆决定，实测 KWin 会把按钮还原位置丢到 (0,0)，
                                    ///< changeEvent 的 max→normal 分支据此纠正几何；
                                    ///< 亦作拖拽还原的光标锚定基准（startManualMove）
    /// 拖动跟随定位：X11 拖出屏幕左缘时经 _NET_MOVERESIZE_WINDOW（仅 x 字段）
    /// 绕过 WM 的边界钳制（可拖出屏幕左右，与原生一致）；其余平台为 move()。
    void applyPositionViaWM(const QPoint &pos);
    /// 创建边缘缩放手柄（透明子控件，自带缩放光标）；Windows 不创建
    void setupEdgeGrips();
    void relayoutEdgeGrips();
    /// 手动拖动路径的边缘吸附：目标判定 / 预览浮层 / 松手应用
    enum class SnapEdge { None, Top, Left, Right };
    SnapEdge edgeSnapTarget(const QPoint &globalPos) const;
    QRect edgeSnapRect(SnapEdge edge) const;
    void showSnapPreview(SnapEdge edge);
    void hideSnapPreview();
    void applyEdgeSnap(SnapEdge edge);
    /// 手动拖动路径的窗口间磁吸：返回向最近候选窗口边缘修正后的位置
    /// （X/Y 两轴独立判定，|偏移| ≤ 吸附带才触发）
    QPoint applyWindowSnap(const QPoint &proposedPos) const;
    /// 吸附目标矩形汇集：X11 全系统顶层窗口（platformWindowSnapCandidates，
    /// 视觉外沿矩形）；为空回退本进程 AuWindow 注册表
    QVector<QRect> windowSnapCandidateRects() const;

    // ---- 平台实现 ----
    // Win32/DWM 实现位于 AuWindowWin.cpp，Xlib/EWMH 实现位于 AuWindowX11.cpp，
    // 其它平台见 AuWindowFallback.cpp；本头文件保持平台无关（无 Q_OS_* 条件）。
    void platformInit();                    ///< 构造：窗口标志/属性、内容区容器、缩放手柄、系统移动通道
    void platformWinIdChanged();            ///< 原生窗口句柄建立后（Windows：应用原生样式/圆角/任务栏）
    void platformFirstShow();               ///< 首次显示（X11：混成检测 + 窗口类型覆盖）
    /// X11：模态无父窗对话框的 WM_TRANSIENT_FOR 规范化（指向打开模态时
    /// 本应用的活动窗口；默认的 clientLeader 会让 ukui-kwin 模态拦截失效，
    /// 点击被阻塞窗口时焦点弹跳闪烁）
    void normalizeModalTransient();
    void platformResized();                 ///< 尺寸变化（Windows：DWM 圆角跟随）
    void platformStateChanged();            ///< 最大化状态切换后（Windows：清除残影的重绘）
    void platformRadiusChanged();           ///< 圆角设置变化（Windows：DWM 偏好；其余：mask 裁剪）
    /// 生效圆角半径（Windows：Win10 直角降级 / Win11 系统圆角；其余平台原值）
    int platformEffectiveRadius(int radius) const;
    void platformButtonVisibilityChanged(); ///< 最大化按钮显隐（Windows：WS_MAXIMIZEBOX 跟随）
    void platformStyleChanged();            ///< 样式切换（Windows：DWM 边框明暗跟随）
    /// 应用任务栏可见性；返回 false 表示平台不支持运行中切换（调用方提示）
    bool platformSetTaskbarVisible(bool visible);
    /// 标题栏拖动交给系统 / 窗口管理器原生完成；返回 false 回退手动拖动
    bool platformStartSystemMove();
    /// 边框缩放交给系统 / 窗口管理器原生完成；返回 false 回退手动缩放
    bool platformStartSystemResize(Qt::Edges edges);
    /// 最大化窗体拖标题栏还原：交 WM 原生交互移动（取消最大化 + 锚定光标
    /// 一次到位）；返回 false 回退手动还原
    bool platformStartMaximizeRestoreDrag();
    /// 系统缩放请求未被 WM 接管的宽限期回退判定（X11 专属，其余平台空实现）
    void platformMouseMoveEvent(QMouseEvent *event);
    /// 窗口是否运行在 alpha 合成环境（决定圆角浮层 / 吸附预览浮层可用性）
    bool platformIsComposited() const;
    /// 窗体形状是否由系统裁剪（Windows：DWM/窗口区域，客户端圆角浮层无用）
    bool platformClipsWindowShape() const;
    /// 阴影开关 / 样式 / 圆角变化后刷新平台阴影通道（X11：上传或清除
    /// _KDE_NET_WM_SHADOW 阴影贴图属性；其余平台无操作）
    void platformShadowUpdate();
    /// 窗口间吸附的全系统候选窗口矩形（Qt 逻辑全局坐标，视觉外沿）：
    /// X11 经 _NET_CLIENT_LIST 枚举（过滤自身/最小化/面板等）；其它平台
    /// 返回空，调用方回退本进程 AuWindow 注册表
    QVector<QRect> platformWindowSnapCandidates() const;

    // Windows 原生辅助（仅在 AuWindowWin.cpp 中定义与调用）
    void applyWindowStyles();
    void applyWindowRoundedCorners();
    void applyTaskbarVisibility();
    QRect m_creationGeometry;           ///< 显示前捕获的期望几何（Windows 首显
                                        ///  校正用：抵消 Qt 创建期放大的原生矩形）
    bool handleNcCalcSize(void *message, AuNativeEventResult *result);
    bool handleNcHitTest(void *message, AuNativeEventResult *result);
    bool handleMinMaxInfo(void *message, AuNativeEventResult *result);

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

    // 边框阴影开关：Windows 上 DWM 阴影随开关切换（platformShadowUpdate）；
    // X11 上阴影由 WM 的 border-only 装饰绘制（见 platformInit），开关仅留存语义
    bool m_borderShadowEnabled;         ///< 阴影开关（platformInit 按平台设默认）

    // 手动拖动 / 缩放状态（无系统 move-resize 能力时的兜底路径）
    bool m_manualResizing;
    bool m_manualMoving;
    bool m_manualRestorePending;   ///< 最大化拖拽还原（手动通道）：已 showNormal、
                                    ///< 待 WM 落位后收养（changeEvent/状态翻转期
                                    ///< 间不得插入任何几何请求——会闪帧；须先于
                                    ///< showNormal 置位，changeEvent 同步触发）
    QRect m_manualRestoreTargetRect;   ///< 吸附态拖拽还原的挂起目标矩形（吸附前浮动
                                        ///< 几何按抓取点比例锚定）；收养时应用。
                                        ///< 无效 = 非吸附还原，收养 KWin 实际落位
    QPoint m_manualStartGlobal;
    QPoint m_manualStartPos;
    QSize m_manualStartSize;
    Qt::Edges m_manualEdges;
    bool m_systemMoveEnabled;      ///< X11：是否使用 _NET_WM_MOVERESIZE 系统移动
    bool m_edgeSnapEnabled;        ///< 手动拖动路径：边缘吸附（拖顶最大化 / 左右半屏）
    bool m_windowSnapEnabled;      ///< 手动拖动路径：窗口间磁吸（X11 全系统窗口）
    AuSnapPreview *m_snapPreview;  ///< 手动拖动的吸附预览浮层（懒创建）
    bool m_snapped;                ///< 当前处于边缘吸附状态（拖动标题栏时还原）
    QRect m_preSnapGeometry;       ///< 吸附前的浮动几何（还原跟随基准）
    bool m_nativeDragActive;       ///< 系统原生拖动进行中：期间冻结 m_normalGeometry
                                    ///< 记录（拖动途中位置是过渡态），收场时按终态
                                    ///< 定基准——最大化收场保留拖动起点，常规收场
                                    ///< （含原生贴靠）取终点
    bool m_inNativeMoveLoop;       ///< Windows：系统原生移动循环（WM_ENTERSIZEMOVE
                                    ///< 到 WM_EXITSIZEMOVE 之间）进行中：期间
                                    ///< changeEvent 的还原几何纠正不得输出
                                    ///< setGeometry——与移动循环逐帧互搏会把窗口
                                    ///< 拽离光标、并在松手最大化瞬间把窗口拖回
                                    ///< 最大化前位置（实测表现为拖拽还原后不松手
                                    ///< 再拖顶无法最大化）
    int m_nativeDragGen;           ///< 原生拖动代次：延迟收场定时器的归属判定
                                    ///< （收场前用户又开始新拖动时旧定时器作废）
    QRect m_normalRestoreAssert;   ///< 最大化还原纠正的断言保护：KWin 取消最大化
                                    ///< 的两步落位（步骤b：首个 motion 移到其记忆
                                    ///< 位置）会在我们纠正后几十毫秒到达并拽回过期
                                    ///< 记忆位置——保护窗内 moveEvent 重新断言；
                                    ///< 用户按下（新手势）立即失效
    QRect m_snapRestoreTarget;     ///< 拖动还原吸附态时的锚定落位目标：WM 异步
                                    ///< 还原的过期几何后到并覆盖时，由
                                    ///< changeEvent 据此纠正回本目标
    bool m_x11BorderDecorActive;   ///< X11：当前原生窗口是否已按 border-only
                                    ///< 写入装饰属性（跟踪运行时开关/圆角切换，
                                    ///< 窗口重建时强制重写）
    bool m_wmGeometryBroken;       ///< X11：_NET_MOVERESIZE_WINDOW 不可用（已回退）
    bool m_wmGeometryPending;      ///< X11：正在等待一次 WM 位置校验
    QVector<QWidget *> m_edgeGrips; ///< 边缘缩放手柄（透明子控件）
    QWidget *m_cornerClips[4];     ///< 四角圆角裁剪浮层（懒创建，仅混成下使用）
    QTimer *m_grabRecoveryTimer;   ///< X11：系统接管后轮询物理按键状态（补发 release）
    bool m_awaitingRelease;        ///< X11：系统通道的 release 被 WM 吞掉，等待补发
};

#endif // AUWINDOW_H
