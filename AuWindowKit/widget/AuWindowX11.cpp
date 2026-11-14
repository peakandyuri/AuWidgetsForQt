#include "AuWindow.h"
#include "AuWindow_p.h"

#include <QApplication>
#include <QCursor>
#include <QDateTime>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QObject>
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
#include <X11/Xmd.h>     // CARD32 等：Kylin 的 X.h 未按惯例包含 Xmd.h，
                         // 显式包含在标准 X11 上幂等
#include <cstdlib>
#include <cstring>

// X11 头文件中的宏与 Qt / 标准类型冲突，按 Qt 源码惯例取消定义。
// None 保留：KDE 阴影协议代码按 Xlib 惯例使用（None == 0L）。
#undef Bool
#undef CursorShape
#undef Expose
#undef KeyPress
#undef KeyRelease
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

// ---- 窗口间吸附：全系统候选窗口枚举辅助 ----

// 忽略枚举期间的 BadWindow：目标窗口可能在 _NET_CLIENT_LIST 读取与逐个
// 查询之间被销毁。X 协议错误异步送达，守卫析构时先 XSync 把挂起错误
// 冲刷到本守卫的空处理器，再还原先前处理器（Qt 的）。
struct AuX11ErrorGuard
{
    explicit AuX11ErrorGuard(Display *d)
        : display(d), previous(XSetErrorHandler(&AuX11ErrorGuard::ignore))
    {
    }
    ~AuX11ErrorGuard()
    {
        if (display)
            XSync(display, False);
        XSetErrorHandler(previous);
    }
    AuX11ErrorGuard(const AuX11ErrorGuard &) = delete;
    AuX11ErrorGuard &operator=(const AuX11ErrorGuard &) = delete;
    static int ignore(Display *, XErrorEvent *) { return 0; }

private:
    Display *display;
    XErrorHandler previous;
};

// 读取窗口 32 位属性（XA_ATOM / XA_CARDINAL / XA_WINDOW 通用）。Xlib 对
// format=32 的返回数据按 long（LP64 下 8 字节）对齐存放。
QVector<long> auReadLongArray(Display *display, Window window, Atom property,
                              Atom expectedType)
{
    QVector<long> values;
    if (property == None || expectedType == None)
        return values;
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long nitems = 0, bytesAfter = 0;
    unsigned char *data = nullptr;
    if (XGetWindowProperty(display, window, property, 0, 65536, False, expectedType,
                           &actualType, &actualFormat, &nitems, &bytesAfter,
                           &data) != Success
        || actualType != expectedType || actualFormat != 32 || !data) {
        if (data)
            XFree(data);
        return values;
    }
    const long *items = reinterpret_cast<const long *>(data);
    values.reserve(int(nitems));
    for (unsigned long i = 0; i < nitems; ++i)
        values.append(items[i]);
    XFree(data);
    return values;
}

// 吸附枚举用原子（进程内单条 Display 连接，缓存一次查询往返）。特殊
// 窗口类型无需逐个内省——收录规则为"仅 NORMAL / DIALOG"，其余类型天然
// 被排除。
struct AuSnapAtoms
{
    Atom clientList = None;
    Atom windowType = None;
    Atom wmState = None;
    Atom frameExtents = None;
    Atom hidden = None;
    Atom typeNormal = None;
    Atom typeDialog = None;
};

const AuSnapAtoms &auSnapAtoms(Display *display)
{
    static AuSnapAtoms atoms = [display]() {
        AuSnapAtoms a;
        a.clientList = XInternAtom(display, "_NET_CLIENT_LIST", False);
        a.windowType = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
        a.wmState = XInternAtom(display, "_NET_WM_STATE", False);
        a.frameExtents = XInternAtom(display, "_NET_FRAME_EXTENTS", False);
        a.hidden = XInternAtom(display, "_NET_WM_STATE_HIDDEN", False);
        a.typeNormal = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
        a.typeDialog = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
        return a;
    }();
    return atoms;
}

// 窗口是否可作为窗口间吸附目标：
// - 可见（IsViewable，未映射即异虚拟桌面 / 已撤销）且未最小化
//   （_NET_WM_STATE_HIDDEN）；
// - 类型为常规顶层：_NET_WM_WINDOW_TYPE 缺省 / 含 NORMAL / 含 DIALOG。
//   面板（DOCK）、桌面层、菜单、工具条、启动画面、通知、工具窗及各类
//   弹出（TOOLTIP / COMBO / DND 等）排除——吸附到任务栏等会得到反直觉
//   落点，且这些窗口不属于用户的摆放语义。
bool auIsSnappableWindow(Display *display, Window window, const AuSnapAtoms &atoms)
{
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(display, window, &attrs)
        || attrs.map_state != IsViewable)
        return false;
    if (atoms.wmState != None && atoms.hidden != None
        && auReadLongArray(display, window, atoms.wmState, XA_ATOM).contains(atoms.hidden))
        return false;
    if (atoms.windowType == None)
        return true;
    const QVector<long> types = auReadLongArray(display, window, atoms.windowType, XA_ATOM);
    if (types.isEmpty())
        return true;   // 未通告类型：历史窗口按常规处理
    for (long type : types) {
        const Atom atom = static_cast<Atom>(type);
        if (atom == atoms.typeNormal || atom == atoms.typeDialog)
            return true;
    }
    return false;
}

// 按"当前是否圆角"同步装饰属性。必须在窗口 map 之前调用——WM 在首次
// 管理窗口时分配装饰，管理后改 motif 不会重新分配。
// 直角窗体：预写 peony 同款属性（落盘值逐字节一致），WM 分配 border-only
// 装饰 = 窗外主题阴影环（ukui-decorationrc shadow_border=30 /
// shadowcolor_alphaf=0.15），窗体几何零污染。
// 圆角窗体：显式写 motif decorations=0（Qt FramelessWindowHint 同款落盘
// 值 {2,1,0,0,0}，本 WM 实证不分配任何装饰）。**不能删除属性了事**——
// 无属性窗口被 WM 按默认装饰管理（_NET_FRAME_EXTENTS top=38 原生标题栏，
// 实测复现"双标题栏"）。WM 装饰恒为矩形、不跟随客户端圆角，混成下圆角
// 由 ARGB alpha 呈现，透明角像素后面会露出矩形边线/阴影直角（呈现为
// "矩形框包裹窗体"）——圆角窗体显式舍弃 WM 装饰保弧线平滑。
// ⚠️ 实测（底缘像素剖面）：给窗口设 XShape shape 后 KWin 会停用整套
// 装饰渲染（阴影完全消失，与 decorations=0 等效）——即"客户端圆角 +
// WM 原生阴影"在本 WM 上不可兼得，XShape 方案无收益，勿再尝试。
void auSyncBorderDecoration(WId windowId, bool borderOnly)
{
    Display *d = AuX11::display();
    if (!d)
        return;
    const Window win = static_cast<Window>(windowId);
    const Atom ukuiDecor = XInternAtom(d, "_KWIN_UKUI_DECORAION", False);
    const Atom motif = XInternAtom(d, "_MOTIF_WM_HINTS", False);
    if (!borderOnly) {
        XDeleteProperty(d, win, ukuiDecor);
        // flags=2（decorations 字段有效）+ decorations=0：无任何装饰
        const CARD32 none[5] = {0x2, 0x1, 0x0, 0x0, 0x0};
        XChangeProperty(d, win, motif, motif, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char *>(none), 5);
        XFlush(d);
        return;
    }
    // ① UKUI 装饰提示（peony XAtomHelper::setUKUIDecoraiontHint，
    //    原子名拼写与上游一致）
    const CARD32 on = 1;
    XChangeProperty(d, win, ukuiDecor, ukuiDecor, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char *>(&on), 1);
    // ② motif：请求 border-only 装饰。逐字节复刻 peony 的落盘值
    //    {3,0,1,0,2}——其 XAtomHelper 在 64 位系统上按 40 字节
    //    MotifWmHints 只拷前 20 字节（5 个 CARD32），源码里的
    //    {flags=FUNC|DECOR, functions=ALL, decorations=BORDER}
    //    实际落盘为 {3, 0, 1, 0, 2}，ukui-kwin 按此解析。
    const CARD32 hints[5] = {0x3, 0x0, 0x1, 0x0, 0x2};
    XChangeProperty(d, win, motif, motif, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char *>(hints), 5);
    XFlush(d);
}

} // namespace

// ---- 平台钩子 ----

void AuWindow::platformInit()
{
    // X11 上不设 Qt::FramelessWindowHint：那会让 Qt 在 map 时写
    // _MOTIF_WM_HINTS decorations=0（并附加 _KDE_NET_WM_WINDOW_TYPE_OVERRIDE
    // 类型），WM 因而完全不分配装饰、无阴影。改为 peony 同款机制——
    // 预写两个属性请 WM 分配"仅边框"装饰：无标题栏、无边框、
    // _NET_FRAME_EXTENTS 全 0（窗体几何零污染），由 WM 用主题绘制窗外
    // 阴影环（ukui-decorationrc shadow_border=30 / shadowcolor_alphaf=0.15）。
    // 必须在窗口首次 map 前写入（WM 在管理时分配装饰，管理后改 motif
    // 不会重新分配）。两个属性的写法逐字节复刻 peony（其 XAtomHelper
    // 在 64 位系统上按 40 字节 MotifWmHints 只拷前 20 字节，落盘为
    // {3,0,1,0,2}，ukui-kwin 吃这一套）。属性预写统一在
    // platformWinIdChanged 中进行：WinIdChange 随下方 winId() 的
    // create() 同步派发，且原生窗口因 setWindowFlag 重建（Dialog 模式
    // 切换、任务栏显隐等）时同样覆盖。
    setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    m_client = new QWidget(this);
    setupEdgeGrips();
    // Linux 默认开启边框阴影（WM 装饰通道，随 border-only 装饰自动
    // 呈现；WM 不支持时退化为无阴影，不做客户端自绘兜底）
    m_borderShadowEnabled = true;
    // 系统通道（Qt 5.15+ startSystemMove/Resize）：拖动/缩放/贴靠全部由 WM
    // 原生完成，默认开启。实测接管后本进程仅 motion/enter 事件不再投递
    // （QTBUG-69716 族，表现为 hover 失效），ConfigureNotify 正常到达，
    // 几何与标题栏跟随不受影响。Qt 5.12 无该 API 且接管后事件冻结更严重
    // （连 ConfigureNotify 也丢失），默认走手动路径 + 模块内置吸附。
    // 两种通道均可经 setSystemWindowMoveEnabled 运行时切换。
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    m_systemMoveEnabled = true;
    // 窗口间吸附跟随手动通道的默认状态：5.15+ 默认走系统通道（WM 原生
    // Quick Tiling），窗口吸附默认关闭，避免默认抢占系统贴靠行为；
    // setWindowSnapEnabled(true) 后拖动自动改走手动通道
    m_windowSnapEnabled = false;
#endif
}

void AuWindow::platformWinIdChanged()
{
    // 原生窗口（重）建后无条件重写装饰属性并复位跟踪状态。窗口首次创建与
    // setWindowFlag 重建（Dialog 模式切换、任务栏显隐等）都会走到这里：
    // 装饰属性随旧窗口销毁，新窗口必须在 map 前重新预写——WinIdChange
    // 在 create() 内、map 之前同步派发，正是写入时机。漏写（只在
    // platformInit 写一次）则重建后的窗口被 WM 按默认装饰管理，
    // 对话框呈现原生标题栏（实测复现过）。
    if (AuX11::isPlatformX11()) {
        auSyncBorderDecoration(winId(), m_borderShadowEnabled && windowRadius() <= 0);
        m_x11BorderDecorActive = m_borderShadowEnabled && windowRadius() <= 0;
    }
    // 重建后的新窗口由 Qt 重写 transient（模态无父窗时又是 clientLeader），
    // 重新规范化（见 normalizeModalTransient 注释）
    normalizeModalTransient();
    platformShadowUpdate();
}

void AuWindow::platformFirstShow()
{
    if (!AuX11::isPlatformX11())
        return;
    // 此时原生窗口已建立，透明是否生效以实际 visual 为准
    m_x11Compositing = AuX11::translucencyAvailable(winId());
    m_x11CheckedTranslucency = true;
    normalizeModalTransient();
    // 首次 map 前按最终圆角/阴影开关校正装饰取舍：WinIdChange（创建期）时
    // m_cornerRadii 还是缺省直角，构造/show 之前才 setWindowRadius 的
    // 圆角窗体（如各对话框）在此最终决定显式无装饰。本函数经 showEvent
    // 在原生窗口 map 之前调用，仍是 WM 分配装饰的有效窗口期。
    platformShadowUpdate();
    relayoutEdgeGrips();
}

// 模态窗口的 WM_TRANSIENT_FOR 规范化：无父窗口的模态对话框（应用级模态
// 的常规创建方式），Qt 默认把 transient 写成 clientLeader（未映射的 1x1
// 组长窗口）。ukui-kwin 的模态拦截依赖 transient 链解析，指向未管理窗口
// 时拦截不可靠——模态打开后短时间内点击被阻塞窗口，焦点会先落被阻塞窗
// 再弹回模态窗（实测焦点序列 模→主→模、含 _NET_ACTIVE_WINDOW=0x0 中间
// 态，主窗边框阴影与模态窗双双闪烁；纯 QDialog 同条件亦复现，非本模块
// 特有）。把 transient 指到打开模态时本应用的活动窗口（通常即被点击的
// 主窗），拦截链具体化后闪烁消失（实测 6 轮 × 3 点击零弹跳）。
// Qt 侧的模态输入阻塞不受影响（QGuiApplication 仍按 ApplicationModal
// 全局拦截输入事件）；代价是 KWin 侧的焦点拦截变为该窗口链——多窗应用
// 中点击链外窗口仍可能闪（Qt 输入阻塞兜底，事件不会送达）。
// WindowModal / 有父窗口的情形 Qt 已写好正确 transient（指向父窗），
// 不干预；活动窗口不属于本应用（程序化打开、无焦点）时保持 Qt 默认。
void AuWindow::normalizeModalTransient()
{
    if (!AuX11::isPlatformX11())
        return;
    if (windowModality() == Qt::NonModal || parentWidget())
        return;
    if (!windowHandle())
        return;
    QWindow *active = QGuiApplication::focusWindow();
    if (!active || active == windowHandle())
        return;
    for (QWidget *top : QApplication::topLevelWidgets()) {
        if (top->windowHandle() == active) {
            if (windowHandle()->transientParent() != active)
                windowHandle()->setTransientParent(active);
            return;
        }
    }
}

void AuWindow::platformResized() {}

void AuWindow::platformStateChanged() {}

void AuWindow::platformRadiusChanged()
{
    // 运行时圆角切换同步装饰取舍（KWin 实测支持 post-map 重读 motif 撤除
    // 装饰；由无装饰切回 border-only 需重映射，见 platformShadowUpdate）
    platformShadowUpdate();
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

// 最大化窗体拖动还原的 WM 原生交互移动通道（当前不再被调用，保留备用）：
// 还原现统一走手动通道（startManualMove 最大化分支 + changeEvent 状态翻转
// 后锚定落位，见 AuWindow::performTitleBarDrag / startManualMove 注释——
// 原生通道的交互移动取消最大化时，"还原尺寸的小窗口"会在最大化位置
// (0,0) 露帧，且锚定受 WM 记忆几何过期影响）。
bool AuWindow::platformStartMaximizeRestoreDrag()
{
    if (!AuX11::isPlatformX11())
        return false;
    if (!auSendX11MoveResize(winId(), QCursor::pos(), 8))
        return false;
    startGrabRecoveryPoll();
    return true;
}

// ---- 窗口间吸附：全系统候选窗口枚举 ----

// X11（xcb 会话）经根窗口 _NET_CLIENT_LIST 枚举所有受管顶层窗口，返回
// 视觉外沿矩形（客户区几何按 _NET_FRAME_EXTENTS 外扩 WM 装饰 / 阴影环）
// 换算成 Qt 逻辑全局坐标——与麒麟原生 WM 层吸附对齐视觉边缘的行为一致。
// 过滤规则见 auIsSnappableWindow()；自身窗口不在候选中。
// 坐标换算按拖动窗口所在屏幕的 devicePixelRatio 换算（混合 DPI 多屏下
// 其它屏幕的候选为近似值；麒麟环境 DPR = 1 精确）。
// 性能：全量枚举约每窗口 4~5 个 X 往返，逐 mouse-move 调用不可接受；
// 拖动期间其它顶层窗口不会移动，故结果做 150ms 短时缓存。
QVector<QRect> AuWindow::platformWindowSnapCandidates() const
{
    QVector<QRect> rects;
    Display *display = AuX11::isPlatformX11() ? AuX11::display() : nullptr;
    if (!display)
        return rects;
    static QElapsedTimer cacheTimer;
    static QVector<QRect> cache;
    static bool cacheValid = false;
    if (cacheValid && cacheTimer.isValid() && !cacheTimer.hasExpired(150))
        return cache;

    const AuSnapAtoms &atoms = auSnapAtoms(display);
    if (atoms.clientList == None)
        return rects;
    const AuX11ErrorGuard errorGuard(display);
    const QVector<long> clients = auReadLongArray(display, XDefaultRootWindow(display),
                                                  atoms.clientList, XA_WINDOW);
    if (clients.isEmpty())
        return rects;
    const Window self = static_cast<Window>(winId());
    const QScreen *screen = windowHandle() ? windowHandle()->screen() : nullptr;
    const qreal dpr = screen ? screen->devicePixelRatio() : 1.0;
    for (long clientId : clients) {
        const Window window = static_cast<Window>(clientId);
        if (window == self)
            continue;
        if (!auIsSnappableWindow(display, window, atoms))
            continue;
        // 客户区根坐标（属性 x/y 相对 WM 外框父窗口，不可直接用）
        ::Window child = None;
        int clientX = 0, clientY = 0;
        if (!XTranslateCoordinates(display, window, XDefaultRootWindow(display),
                                   0, 0, &clientX, &clientY, &child))
            continue;
        XWindowAttributes attrs;
        if (!XGetWindowAttributes(display, window, &attrs))
            continue;
        // _NET_FRAME_EXTENTS：WM 在客户区四周（左 / 右 / 上 / 下）加的
        // 装饰宽度，外扩后即用户视觉的窗口外沿
        int extents[4] = { 0, 0, 0, 0 };
        if (atoms.frameExtents != None) {
            const QVector<long> frame = auReadLongArray(display, window,
                                                        atoms.frameExtents, XA_CARDINAL);
            for (int i = 0; i < qMin(4, frame.size()); ++i)
                extents[i] = int(frame.at(i));
        }
        const QRectF logical(
            (clientX - extents[0]) / dpr, (clientY - extents[2]) / dpr,
            qreal(attrs.width + extents[0] + extents[1]) / dpr,
            qreal(attrs.height + extents[2] + extents[3]) / dpr);
        rects.append(logical.toRect());
    }
    cache = rects;
    cacheTimer.start();
    cacheValid = true;
    return rects;
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

// ---- 边框阴影平台通道 ----

// X11：阴影由 WM 的 border-only 装饰在窗外合成（见 platformInit 的
// _KWIN_UKUI_DECORAION + _MOTIF_WM_HINTS 预写），随窗口移动 / 缩放 /
// 最大化自动呈现与停用（WM wantsShadowToBeRendered），运行时无需任何
// 客户端动作。setBorderShadowEnabled 在本平台仅留存开关语义。
// X11：阴影 = WM 的 border-only 装饰（ukui-decorationrc 主题阴影环，窗外
// 合成），开关与圆角共同决定装饰取舍：直角 + 阴影开 → border-only；
// 圆角窗（radius>0）或开关关 → 显式无装饰（motif decorations=0）。
// KWin 实测支持 map 后重读 motif **撤除**装饰（即时生效）；**加回**装饰
// 不生效，需 hide/show 重映射强制重新管理（有一次闪动，仅状态切换时发生）。
// setBorderShadowEnabled / setWindowRadius 运行时切换均经此钩子生效。
void AuWindow::platformShadowUpdate()
{
    if (!AuX11::isPlatformX11())
        return;
    const bool want = m_borderShadowEnabled && windowRadius() <= 0;
    if (want == m_x11BorderDecorActive)
        return;   // 状态未变（窗口重建路径已在 WinIdChange 强制重写）
    auSyncBorderDecoration(winId(), want);
    m_x11BorderDecorActive = want;
    if (want && isVisible()) {
        // 由"无装饰"切回 border-only 且窗口已映射：延迟一拍重映射，
        // 等当前创建/显示流程结束后 withdraw+map
        QTimer::singleShot(0, this, [this] {
            if (isVisible()) {
                hide();
                show();
            }
        });
    }
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
    // 混成运行时窗口为 ARGB 透明背景：圆角由 alpha 通道平滑呈现，区域
    // 裁剪（1bit 无抗锯齿）反而产生锯齿并破坏客户端绘制，必须跳过 mask。
    // 圆角窗体的 WM 矩形装饰直角由显式无装饰解决（见 auSyncBorderDecoration
    // 注释："客户端圆角 + WM 原生阴影"在本 WM 上不可兼得，勿再尝试 XShape）。
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
    // 无混成：内容区圆角 mask 裁剪（setMask 同时设置 X11 bounding/input
    // shape，WM 装饰同样被裁）+ 阴影环保留（阴影自身带 alpha 渐隐）
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
    scheduleNativeDragSettle();   // 原生拖动收场（release 被吞，由本轮询兜底）
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
