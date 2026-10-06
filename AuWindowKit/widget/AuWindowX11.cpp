#include "AuWindow.h"
#include "AuWindow_p.h"

#include <QCursor>
#include <QDateTime>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QTimer>
#include <qpa/qwindowsysteminterface_p.h>

// ============================================================================
// AuWindow 的 Linux/X11 平台实现。全部 Xlib / EWMH 原生代码集中在本文件，
// AuWindow.cpp 只保留跨平台逻辑（经 platformXxx() 钩子进入本文件）。
// 其它平台编译本文件为空翻译单元。
// ============================================================================

#ifdef Q_OS_LINUX

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>   // XImage / XPutImage（原生阴影贴图上传）
#include <cstring>

// X11 头文件中的宏与 Qt / 标准类型冲突，按 Qt 源码惯例取消定义
#undef Bool
#undef CursorShape
#undef Expose
#undef KeyPress
#undef KeyRelease
#undef None
#undef Status
#undef Unsorted

// 极简 Xlib 访问层（不依赖 QtX11Extras）：复用 Qt 使用的 DISPLAY，
// 进程内缓存 Display 连接，仅供 GUI 线程调用。
namespace AuX11 {

inline bool isPlatformX11()
{
    return QGuiApplication::platformName() == QLatin1String("xcb");
}

inline Display *display()
{
    static Display *s_display = XOpenDisplay(nullptr);
    return s_display;
}

/// 透明是否实际生效：优先查 EWMH 混成选择；ukui-kwin 等定制混成器
/// 不注册 _NET_WM_CM_S 选择，故再以窗口 ARGB visual（depth 32）兜底判断。
inline bool translucencyAvailable(WId windowId)
{
    Display *d = display();
    if (!d)
        return false;
    const QByteArray atom = "_NET_WM_CM_S" + QByteArray::number(XDefaultScreen(d));
    if (XGetSelectionOwner(d, XInternAtom(d, atom.constData(), False)) != 0)
        return true;
    XWindowAttributes attrs;
    if (XGetWindowAttributes(d, static_cast<Window>(windowId), &attrs))
        return attrs.depth == 32;
    return false;
}

} // namespace AuX11

namespace {

// X11：请求窗口管理器启动原生移动 / 缩放（_NET_WM_MOVERESIZE）。
// direction：8 = 移动，0..7 = 左上/上/右上/右/右下/下/左下/左 缩放。
// 发送要点（缺一即被 WM 静默降级，实测 ukui-kwin / KWin 系）：
// 1. button 字段固定 0——非 0 值触发 WM 的按键状态校验，被 CSD 隐式
//    grab 挡住而忽略（Chromium 同款做法）；
// 2. 按下产生的服务器隐式指针抓取归属 Qt 的显示连接，必须在 Qt 连接上
//    解除（setMouseGrabEnabled(false)）——本函数第二连接上的
//    XUngrabPointer 对其无效；不解除则 WM 的 xcb_grab_pointer 返回
//    AlreadyGrabbed，退化为不跟随指针的空转移动循环（窗口冻结）。
bool auSendX11MoveResize(WId windowId, const QPoint &globalPos, int direction)
{
    if (!AuX11::isPlatformX11())
        return false;
    Display *display = AuX11::display();
    if (!display)
        return false;
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = static_cast<Window>(windowId);
    ev.xclient.message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = globalPos.x();
    ev.xclient.data.l[1] = globalPos.y();
    ev.xclient.data.l[2] = direction;
    ev.xclient.data.l[3] = 0;
    ev.xclient.data.l[4] = 0;
    // Qt（XInput2）在按下时会建立 XI2 设备抓取，核心协议的 XUngrabPointer
    // 解除不了它；不释放则 WM 的 xcb_grab_pointer 返回 AlreadyGrabbed，
    // KWin 系 WM 会退化为仅键盘抓取的移动循环——不跟随指针，且丢弃后续
    // 客户端移动请求（实测）。必须先经 Qt 通道解除隐式抓取，再请求接管。
    if (QWidget *w = QWidget::find(windowId)) {
        if (QWidget *grabber = QWidget::mouseGrabber())
            grabber->releaseMouse();
        if (QWindow *wh = w->windowHandle())
            wh->setMouseGrabEnabled(false);
    }
    XUngrabPointer(display, CurrentTime);
    XSendEvent(display, XDefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(display);
    return true;
}

// Qt::Edges → _NET_WM_MOVERESIZE 方向编号
int auEdgesToMoveResizeDirection(Qt::Edges edges)
{
    if (edges.testFlag(Qt::LeftEdge) && edges.testFlag(Qt::TopEdge))
        return 0;
    if (edges.testFlag(Qt::TopEdge) && edges.testFlag(Qt::RightEdge))
        return 2;
    if (edges.testFlag(Qt::RightEdge) && edges.testFlag(Qt::BottomEdge))
        return 4;
    if (edges.testFlag(Qt::BottomEdge) && edges.testFlag(Qt::LeftEdge))
        return 6;
    if (edges.testFlag(Qt::TopEdge))
        return 1;
    if (edges.testFlag(Qt::RightEdge))
        return 3;
    if (edges.testFlag(Qt::BottomEdge))
        return 5;
    if (edges.testFlag(Qt::LeftEdge))
        return 7;
    return 8;
}

// X11：发送 _NET_MOVERESIZE_WINDOW（EWMH 分页器消息）仅携带 x 字段，
// 由 WM 直接应用窗口 x 位置——不经过普通移动请求（ConfigureRequest）的
// 边界钳制，窗口可拖出屏幕左右，与原生窗口拖动行为一致。
// 注：实测 ukui-kwin 对同时携带 y/w/h 的请求处理异常（被忽略），
// 故只发 x；y 与缩放走普通通道。
bool auSendNetMoveResizeWindow(WId windowId, const QPoint &pos)
{
    if (!AuX11::isPlatformX11())
        return false;
    Display *display = AuX11::display();
    if (!display)
        return false;
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = static_cast<Window>(windowId);
    ev.xclient.message_type = XInternAtom(display, "_NET_MOVERESIZE_WINDOW", False);
    ev.xclient.format = 32;
    // flags: bit0=x 为绝对值；bit8-10 = gravity（NorthWest=1）
    ev.xclient.data.l[0] = 0x1 | (1 << 8);
    ev.xclient.data.l[1] = pos.x();
    XSendEvent(display, XDefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(display);
    return true;
}

} // namespace

// ---- 平台钩子 ----

void AuWindow::platformInit()
{
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    m_client = new QWidget(this);
    setupEdgeGrips();
    // 系统通道（Qt 5.15+ startSystemMove/Resize）：拖动/缩放/贴靠全部由 WM
    // 原生完成，默认开启。实测接管后本进程仅 motion/enter 事件不再投递
    // （QTBUG-69716 族，表现为 hover 失效），ConfigureNotify 正常到达，
    // 几何与标题栏跟随不受影响。Qt 5.12 无该 API 且接管后事件冻结更严重
    // （连 ConfigureNotify 也丢失），默认走手动路径 + 模块内置吸附。
    // 两种通道均可经 setSystemWindowMoveEnabled 运行时切换。
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    m_systemMoveEnabled = true;
#endif
}

void AuWindow::platformWinIdChanged() {}

void AuWindow::platformFirstShow()
{
    if (!AuX11::isPlatformX11())
        return;
    // 此时原生窗口已建立，透明是否生效以实际 visual 为准
    m_x11Compositing = AuX11::translucencyAvailable(winId());
    m_x11CheckedTranslucency = true;
    // Qt 的 FramelessWindowHint 会附加 _KDE_NET_WM_WINDOW_TYPE_OVERRIDE，
    // KWin 对该类型禁用拖动预览 / 边缘吸附；覆盖为普通 NORMAL 类型
    Display *display = AuX11::display();
    if (display) {
        Atom normal = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
        XChangeProperty(display, static_cast<Window>(winId()),
                        XInternAtom(display, "_NET_WM_WINDOW_TYPE", False),
                        XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char *>(&normal), 1);
        XFlush(display);
    }
    relayoutEdgeGrips();
}

void AuWindow::platformResized() {}

void AuWindow::platformStateChanged() {}

void AuWindow::platformRadiusChanged()
{
    updateMask();
}

int AuWindow::platformEffectiveRadius(int radius) const
{
    return radius;
}

void AuWindow::platformButtonVisibilityChanged() {}

void AuWindow::platformStyleChanged() {}

bool AuWindow::platformSetTaskbarVisible(bool visible)
{
    if (!AuX11::isPlatformX11())
        return false;
    Q_UNUSED(visible)
    // X11：Tool 窗口即跳过任务栏（_NET_WM_STATE_SKIP_TASKBAR 由 Qt 设置）
    setWindowFlag(Qt::Tool, !visible);
    return true;
}

bool AuWindow::platformStartSystemMove()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    // Qt 5.15+：官方 startSystemMove（内部即 _NET_WM_MOVERESIZE，且已修复
    // 接管后事件管线冻结的 QTBUG-69716），跨 X11/Wayland。
    if (m_systemMoveEnabled && windowHandle() && windowHandle()->startSystemMove()) {
        startGrabRecoveryPoll();
        return true;
    }
#else
    // Qt 5.12：手写 EWMH 消息（button=0 + 解除 Qt 连接隐式抓取），已知
    // 接管后 motion/enter 永久丢失（QTBUG-69716），仅显式开启时使用。
    if (m_systemMoveEnabled && auSendX11MoveResize(winId(), QCursor::pos(), 8)) {
        startGrabRecoveryPoll();
        return true;
    }
#endif
    return false;
}

bool AuWindow::platformStartSystemResize(Qt::Edges edges)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return m_systemMoveEnabled && windowHandle()
           && windowHandle()->startSystemResize(edges);
#else
    return m_systemMoveEnabled
           && auSendX11MoveResize(winId(), QCursor::pos(),
                                  auEdgesToMoveResizeDirection(edges));
#endif
}

// 最大化窗体的拖动还原走 WM 原生交互移动：KWin 在最大化窗口上的交互移动
// = 取消最大化 + 锚定光标一次到位，无手动路径的中间帧闪动与锚定脱离。
// 手动通道默认关闭的 5.12 也走此捷径——接管后的事件冻结（motion/enter
// 失效）由 grab-recovery 轮询兜底；还原尺寸偏差由 changeEvent 的
// max→normal 分支按 m_normalSizeHint 纠正。
bool AuWindow::platformStartMaximizeRestoreDrag()
{
    if (!AuX11::isPlatformX11())
        return false;
    if (!auSendX11MoveResize(winId(), QCursor::pos(), 8))
        return false;
    startGrabRecoveryPoll();
    return true;
}

// 已发系统缩放请求却持续收到本进程事件（超出接管宽限期）：WM 未接管，
// 回退手动缩放。宽限期内忽略在途事件，避免与 WM 的指针抓取相撞
// （同 AuTitleBar 拖动回退）。
void AuWindow::platformMouseMoveEvent(QMouseEvent *event)
{
    if (!m_systemResizePending)
        return;
    if (QDateTime::currentMSecsSinceEpoch() - m_systemResizeSentMs > 150) {
        m_systemResizePending = false;
        if (event->buttons() & Qt::LeftButton)
            startManualResize(m_pendingEdges, m_pressGlobal);
    }
}

// 混成运行时窗口为 ARGB 透明背景（圆角 / 预览浮层可用）；Wayland 等非
// X11 平台按混成处理，无 X11 侧的透明能力检测。
bool AuWindow::platformIsComposited() const
{
    return !AuX11::isPlatformX11() || m_x11Compositing;
}

bool AuWindow::platformClipsWindowShape() const
{
    return false;
}

// ---- Windows 专属接口的空实现（X11 无对应操作） ----

void AuWindow::applyWindowStyles() {}
void AuWindow::applyWindowRoundedCorners() {}
void AuWindow::applyTaskbarVisibility() {}

// ---- mask / WM 几何 ----

void AuWindow::updateMask()
{
    const bool noMaskNeeded = windowRadius() <= 0 || isMaximized() || isFullScreen();
    // 混成运行时窗口为 ARGB 透明背景：圆角与阴影由 alpha 通道平滑呈现，
    // 区域裁剪（1bit 无抗锯齿）反而产生锯齿并破坏阴影，必须跳过。
    // 仅无混成（透明不可用）时才用 mask 裁出圆角。
    // 注意：首显之前（m_x11Compositing 尚未检测）setWindowRadius 可能已
    // 按当时尺寸设置过 mask；检测到混成后必须清掉这份过期 mask——否则
    // 窗体的显示与输入区域被永久钳制在旧尺寸，缩放后新增区域不可见、
    // 不可点（关闭按钮"漂出"窗体即此原因），且 mask 不随 resize 更新。
    if (!noMaskNeeded && AuX11::isPlatformX11() && m_x11Compositing) {
        if (!mask().isNull()) {
            clearMask();
            update(); // 清理过期 mask 后补一次重绘，确保此前被裁剪的区域可见
        }
        return;
    }
    if (noMaskNeeded) {
        if (!mask().isNull())
            clearMask();
        return;
    }
    const int margin = effectiveMargin();
    const QRect content = rect().adjusted(margin, margin, -margin, -margin);
    const QPainterPath path = AuWindowDetail::auRoundedRectPath(
        QRectF(content), m_cornerRadii[0], m_cornerRadii[1],
        m_cornerRadii[2], m_cornerRadii[3]);
    const QRegion rounded(path.toFillPolygon().toPolygon());
    const QRegion outer(rect());
    // 内容区圆角裁剪 + 阴影环保留（阴影自身带 alpha 渐隐）
    setMask(rounded | (outer - QRegion(content)));
}

void AuWindow::applyPositionViaWM(const QPoint &pos)
{
    // 屏内目标：普通移动，x/y 都可靠。
    // 目标 x < 0（用户向左拖出屏幕）：普通移动请求会被 KWin 钳制（x>=0），
    // 改发 _NET_MOVERESIZE_WINDOW（EWMH 分页器消息，仅 x 字段 + NorthWest
    // gravity）——该消息不受钳制，实测可稳定拖出屏幕左缘；此通道无法携带
    // y（ukui-kwin 对 y/w/h 字段存在错位 bug），故出屏期间 y 冻结在上次
    // 屏内位置，拖回屏内后恢复正常。右侧 / 底部本就不受钳制。
    if (m_wmGeometryBroken || pos.x() >= 0) {
        move(pos);
        return;
    }
    if (!auSendNetMoveResizeWindow(winId(), pos))
        m_wmGeometryBroken = true;   // 消息发送失败，回退普通移动
}

// ---- X11：系统通道接管后的抓取恢复 ----

// WM 接管交互移动/缩放期间，松开按键产生的 release 事件被 WM 的指针抓取
// 吞掉，Qt 侧收不到：内部隐式抓取（qt_button_down）停留在按下时的控件上，
// 后续 motion 全部路由给该控件，enter/leave 不再合成——表现为 hover 失效、
// 光标停留在最后一次调整的形状，直到用户再点一下才复位。
// 检测到物理松开且 Qt 未收到 release 时，直接向 Qt 事件系统补发一次鼠标
// release（QWindowSystemInterface，不经 X 输入管线）。不得用 XTEST 注入
// 输入事件：KWin 在贴靠后会弹出 snap-assist 等交互辅助层，XTEST 点击会把
// 它们误关掉，还可能命中边缘手柄引发误缩放；直接补事件对 WM 完全透明。
void AuWindow::startGrabRecoveryPoll()
{
    if (!AuX11::isPlatformX11())
        return;
    m_awaitingRelease = true;
    if (!m_grabRecoveryTimer) {
        m_grabRecoveryTimer = new QTimer(this);
        m_grabRecoveryTimer->setInterval(25);
        connect(m_grabRecoveryTimer, &QTimer::timeout, this, &AuWindow::pollGrabRecovery);
    }
    m_grabRecoveryTimer->start();
}

void AuWindow::pollGrabRecovery()
{
    Display *display = AuX11::isPlatformX11() ? AuX11::display() : nullptr;
    if (!display) {
        m_grabRecoveryTimer->stop();
        m_awaitingRelease = false;
        return;
    }
    ::Window root = XDefaultRootWindow(display);
    ::Window rootRet = 0, childRet = 0;
    int rootX = 0, rootY = 0, winX = 0, winY = 0;
    unsigned int mask = 0;
    XQueryPointer(display, root, &rootRet, &childRet, &rootX, &rootY,
                  &winX, &winY, &mask);
    if (mask & Button1Mask)
        return; // 用户正按着按键（可能已开始新操作），不打扰

    m_grabRecoveryTimer->stop();
    const bool awaiting = m_awaitingRelease;
    m_awaitingRelease = false;
    if (!awaiting)
        return; // release 已正常到达，无需处理

    // 复位被吞 release 冻结的滞留状态，防止后续事件被卡死的拖动 / 缩放
    // 状态放大成新的系统动作
    m_systemResizePending = false;
    m_manualResizing = false;
    m_manualMoving = false;
    m_titleBar->resetDragState();
    // 直接向 Qt 补发 release：清除内部隐式抓取，触发 enter/leave 重算，
    // hover 与光标立即恢复。KWin 的 snap-assist 等浮层不受任何影响。
    if (QWindow *wh = windowHandle()) {
        const QPointF global(QCursor::pos());
        QWindowSystemInterface::handleMouseEvent(wh, wh->mapFromGlobal(global.toPoint()),
                                                 global, Qt::NoButton, Qt::NoModifier);
        QWindowSystemInterface::flushWindowSystemEvents();
    }
}

bool AuWindow::nativeEvent(const QByteArray &eventType, void *message, AuNativeEventResult *result)
{
    return QWidget::nativeEvent(eventType, message, result);
}

#endif // Q_OS_LINUX
