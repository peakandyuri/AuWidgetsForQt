#include "AuWindow.h"
#include "AuTitleBar.h"
#include "AuWindowStyle.h"

#include <QDateTime>
#include <QEventLoop>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <qpa/qwindowsysteminterface_p.h>

#include <cmath>
#include <vector>

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#  include <windowsx.h>
#  include <shellapi.h>
#  include <uxtheme.h>   // MARGINS（DwmExtendFrameIntoClientArea 参数类型）
#  ifndef SM_CXPADDEDBORDER
     // Vista 引入的常量；旧版 MinGW 头文件在默认 WINVER（<0x0600）下未声明
#    define SM_CXPADDEDBORDER 92
#  endif
#  ifndef WM_NCUAHDRAWCAPTION
     // 未公开的主题重绘消息（Chromium 惯例命名），旧头文件未定义
#    define WM_NCUAHDRAWCAPTION 0x00AE
#    define WM_NCUAHDRAWFRAME 0x00AF
#  endif
#else
#  if defined(Q_OS_LINUX)
#    include <X11/Xlib.h>
#    include <X11/Xatom.h>
#    include <X11/Xutil.h>   // XImage / XPutImage（原生阴影贴图上传）
// X11 头文件中的宏与 Qt / 标准类型冲突，按 Qt 源码惯例取消定义
#    undef Bool
#    undef CursorShape
#    undef Expose
#    undef KeyPress
#    undef KeyRelease
#    undef None
#    undef Status
#    undef Unsorted
#  endif
#endif

namespace {

// 四角独立半径的圆角矩形路径（tl/tr/bl/br 分别为左上/右上/左下/右下）。
QPainterPath auRoundedRectPath(const QRectF &r, qreal tl, qreal tr, qreal bl, qreal br)
{
    QPainterPath path;
    const qreal maxR = qMin(r.width(), r.height()) / 2;
    tl = qBound<qreal>(0, tl, maxR);
    tr = qBound<qreal>(0, tr, maxR);
    bl = qBound<qreal>(0, bl, maxR);
    br = qBound<qreal>(0, br, maxR);
    if (tl <= 0 && tr <= 0 && bl <= 0 && br <= 0) {
        path.addRect(r);
        return path;
    }
    path.moveTo(r.left() + tl, r.top());
    path.lineTo(r.right() - tr, r.top());
    if (tr > 0)
        path.arcTo(r.right() - 2 * tr, r.top(), 2 * tr, 2 * tr, 90, -90);
    else
        path.lineTo(r.right(), r.top());
    path.lineTo(r.right(), r.bottom() - br);
    if (br > 0)
        path.arcTo(r.right() - 2 * br, r.bottom() - 2 * br, 2 * br, 2 * br, 0, -90);
    else
        path.lineTo(r.right(), r.bottom());
    path.lineTo(r.left() + bl, r.bottom());
    if (bl > 0)
        path.arcTo(r.left(), r.bottom() - 2 * bl, 2 * bl, 2 * bl, 270, -90);
    else
        path.lineTo(r.left(), r.bottom());
    path.lineTo(r.left(), r.top() + tl);
    if (tl > 0)
        path.arcTo(r.left(), r.top(), 2 * tl, 2 * tl, 180, -90);
    else
        path.lineTo(r.left(), r.top());
    path.closeSubpath();
    return path;
}

// 极简 Xlib 访问层（不依赖 QtX11Extras）：复用 Qt 使用的 DISPLAY，
// 进程内缓存 Display 连接，仅供 GUI 线程调用。
#if defined(Q_OS_LINUX)
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
#  endif

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
#  if defined(Q_OS_LINUX)
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
#  else
    Q_UNUSED(windowId)
    Q_UNUSED(globalPos)
    Q_UNUSED(direction)
    return false;
#  endif
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

} // namespace

/// 吸附预览浮层：手动拖动光标进入边缘吸附带时铺出目标区域（半透明主题
/// 高亮），松手应用吸附并隐藏。独立 bypass 顶层窗口：不进任务栏 / Alt+Tab，
/// 也不受 WM 管理（预览自身不能被 WM 二次摆布）。无混成（ARGB 不可用）
/// 时由调用方跳过显示，回退为无预览的直接吸附。
/// 注意：显示过即会被 Qt 无障碍桥（QAccessibleCache）缓存接口，销毁必须
/// 走 deleteLater 让缓存先行解绑，直接 delete 会在应用退出时段错误。
class AuSnapPreview : public QWidget
{
public:
    AuSnapPreview()
        : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint
                               | Qt::WindowStaysOnTopHint
                               | Qt::X11BypassWindowManagerHint)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
#ifdef Q_OS_LINUX
        setAttribute(Qt::WA_X11DoNotAcceptFocus);
#endif
    }

    void showFor(const QRect &rect)
    {
        if (geometry() != rect)
            setGeometry(rect);
        if (!isVisible())
            show();
        raise();
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter p(this);
        QColor fill = palette().color(QPalette::Highlight);
        fill.setAlpha(70);
        p.fillRect(rect(), fill);
        QColor border = fill;
        border.setAlpha(170);
        p.setPen(QPen(border, 2));
        p.drawRect(rect().adjusted(1, 1, -2, -2));
    }
};

/// 圆角裁剪浮层：盖在窗体某个角上的小方块子控件。内容区的不透明子控件
/// （表格、页签、styled 背景等）会把窗体圆角弧线以外的像素重新覆盖成直角，
/// 在角落露出锯齿。此浮层位于内容之上，paintEvent 用 CompositionMode_Clear
/// 把所属角弧线以外的像素从后备缓冲中直接清除（带抗锯齿），并按窗体边框色
/// 补画弧线描边——任意内容下圆角都保持平滑。仅混成（ARGB）环境使用；
/// 无混成时窗体走 mask 裁剪，子控件被一并裁掉，无需本浮层。
class AuCornerClip : public QWidget
{
public:
    AuCornerClip(AuWindow *window, Qt::Corner corner)
        : QWidget(window), m_window(window), m_corner(corner)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        // 最大化时窗体无圆角，浮层已被隐藏；防御式再判一次
        const int r = (m_window->isMaximized() || m_window->isFullScreen())
                          ? 0 : m_window->cornerRadius(m_corner);
        if (r <= 0)
            return;
        const QPointF c = cornerPoint();   // 弧线圆心 = 内容区角点（浮层内的局部坐标）
        QPainterPath keep;
        keep.addEllipse(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r));
        QPainterPath clearPath;
        clearPath.addRect(rect());
        clearPath -= keep;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        // 清除弧线以外的像素（子控件覆盖的部分一并抹掉，弧内内容不受影响）
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.fillPath(clearPath, QColor(Qt::transparent));
        // 补画弧线描边，与窗体直边处的边框衔接
        const AuWindowStyle *st = m_window->style();
        if (st->windowBorderColor().isValid()) {
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.setPen(QPen(st->windowBorderColor(), 1));
            p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r),
                      startAngle(), 90 * 16);
        }
    }

private:
    QPointF cornerPoint() const
    {
        const QRect b = rect();
        switch (m_corner) {
        case Qt::TopLeftCorner:     return QPointF(b.right(), b.bottom());
        case Qt::TopRightCorner:    return QPointF(b.left(), b.bottom());
        case Qt::BottomLeftCorner:  return QPointF(b.right(), b.top());
        case Qt::BottomRightCorner: return QPointF(b.left(), b.top());
        }
        return QPointF();
    }

    int startAngle() const
    {
        // QPainter 角度：0°=3 点钟方向，逆时针为正
        switch (m_corner) {
        case Qt::TopLeftCorner:     return 90 * 16;    // 12点 → 9点
        case Qt::TopRightCorner:    return 0;          // 3点 → 12点
        case Qt::BottomRightCorner: return 270 * 16;   // 6点 → 3点
        case Qt::BottomLeftCorner:  return 180 * 16;   // 9点 → 6点
        }
        return 0;
    }

    AuWindow *m_window;
    Qt::Corner m_corner;
};

#ifdef Q_OS_WIN
namespace {
// 可见窗口矩形：Win10 的 WS_POPUP|WS_THICKFRAME 窗口，GetWindowRect 返回的
// 矩形含 ~7px 不可见调整边框（DWM 只渲染内芯）；DWMWA_EXTENDED_FRAME_BOUNDS
// 给出真实可见边界。命中测试 / 边框热区必须以可见矩形为基准，否则热区会
// 落在不可见条带上（边框无光标、无法缩放）。取不到时回退窗口矩形。
RECT auVisibleFrameBounds(HWND hwnd)
{
    RECT rc;
    typedef HRESULT(WINAPI * DwmGetWindowAttributePtr)(HWND, DWORD, PVOID, DWORD);
    static DwmGetWindowAttributePtr pDwa = []() {
        HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        return dwm ? reinterpret_cast<DwmGetWindowAttributePtr>(
                         GetProcAddress(dwm, "DwmGetWindowAttribute"))
                   : nullptr;
    }();
    if (!pDwa || FAILED(pDwa(hwnd, 9 /* DWMWA_EXTENDED_FRAME_BOUNDS */,
                             &rc, sizeof(rc)))) {
        GetWindowRect(hwnd, &rc);
    }
    return rc;
}
} // namespace
#endif

#ifdef Q_OS_WIN
/// Windows 内容区容器：Qt 在某些时序下（实测：显示带属主关系的子窗体后）
/// 会把本容器提升为原生子窗口（WS_CHILD），其矩形覆盖整个客户区——包括
/// 边框缩放热区。系统命中测试先询问该原生 child，其默认回答 HTCLIENT 会让
/// 窗体本身永远收不到边框命中（表现为：边框无缩放光标、无法拖拽缩放，
/// 但标题栏拖动 / 内容区点击不受影响，任务栏缩略图亦受扰）。此处覆盖
/// nativeEvent：光标落在窗体边框热区内时返回 HTTRANSPARENT，让系统穿透
/// 本 child 继续向父（窗体）做命中测试，边框判定交还给窗体的
/// WM_NCHITTEST 处理。容器保持非原生时本函数不会被调用，零影响。
class AuClientArea : public QWidget
{
public:
    explicit AuClientArea(AuWindow *window)
        : QWidget(window), m_window(window)
    {
    }

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override
    {
        if (eventType == QByteArrayLiteral("windows_generic_MSG")) {
            MSG *msg = static_cast<MSG *>(message);
            if (msg->message == WM_NCHITTEST) {
                HWND win = reinterpret_cast<HWND>(m_window->winId());
                if (!IsZoomed(win) && !m_window->isFullScreen() && !m_window->isMaximized()) {
                    // 以窗体的 DWM 可见矩形为基准（本控件横向铺满客户区）：
                    // 处于左右 / 底边框热区内时返回 HTTRANSPARENT，让系统穿透
                    // 本原生 child 继续命中父窗体（不处理则本 child 会吞掉
                    // 边框命中，边框缩放失效——Qt 在显示属主子窗体后会把
                    // 本容器提升为原生 child，实测为可复现的固定行为）。
                    // 热区用系统标准边框厚（不加宽：过宽会侵入标题栏 /
                    // 操作按钮区，把标题栏拖动误判为缩放）
                    const int frame = GetSystemMetrics(SM_CXSIZEFRAME)
                                      + GetSystemMetrics(SM_CXPADDEDBORDER);
                    const RECT rc = auVisibleFrameBounds(win);
                    const QPoint global(GET_X_LPARAM(msg->lParam),
                                        GET_Y_LPARAM(msg->lParam));
                    const QPoint local(global.x() - rc.left, global.y() - rc.top);
                    const int w = rc.right - rc.left;
                    const int h = rc.bottom - rc.top;
                    if (local.x() < frame
                        || local.x() >= w - frame
                        || local.y() >= h - frame) {
                        *result = HTTRANSPARENT;
                        return true;
                    }
                }
            }
        }
        return QWidget::nativeEvent(eventType, message, result);
    }

private:
    AuWindow *m_window;
};
#endif

AuWindow::AuWindow(QWidget *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , m_style(nullptr)
    , m_clientLayout(nullptr)
    , m_adoptedLayout(nullptr)
    , m_centralWidget(nullptr)
    , m_cornerRadii{0, 0, 0, 0}
    , m_closeInterception(false)
    , m_forceClosing(false)
    , m_taskbarVisible(true)
    , m_dialogMode(false)
    , m_result(0)
    , m_eventLoop(nullptr)
    , m_shownOnce(false)
    , m_dwmCornerOk(false)
    , m_x11Compositing(false)
    , m_x11CheckedTranslucency(false)
    , m_manualResizing(false)
    , m_manualMoving(false)
    // 系统通道（Qt 5.15+ startSystemMove/Resize）：拖动/缩放/贴靠全部由 WM
    // 原生完成，默认开启。实测接管后本进程仅 motion/enter 事件不再投递
    // （QTBUG-69716 族，表现为 hover 失效），ConfigureNotify 正常到达，
    // 几何与标题栏跟随不受影响。Qt 5.12 无该 API 且接管后事件冻结更严重
    // （连 ConfigureNotify 也丢失），默认走手动路径 + 模块内置吸附。
    // 两种通道均可经 setSystemWindowMoveEnabled 运行时切换。
#if defined(Q_OS_LINUX) && QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    , m_systemMoveEnabled(true)
#else
    , m_systemMoveEnabled(false)
#endif
    , m_edgeSnapEnabled(true)
    , m_snapPreview(nullptr)
    , m_snapped(false)
    , m_wmGeometryBroken(false)
    , m_wmGeometryPending(false)
    , m_systemResizePending(false)
    , m_systemResizeSentMs(0)
    , m_pressGlobal(0, 0)
    , m_cornerClips{nullptr, nullptr, nullptr, nullptr}
    , m_grabRecoveryTimer(nullptr)
    , m_awaitingRelease(false)
{
#ifdef Q_OS_WIN
    // Windows：保留 WS_THICKFRAME 等原生样式并剥离 WS_CAPTION（见
    // applyWindowStyles），在 nativeEvent 中去掉可见边框，以换取 DWM
    // 阴影 / Snap / 原生动画，且杜绝 DWM 向客户区顶部重绘原生标题栏。
    // 手动拖动路径的边缘吸附（为 X11 无系统 Snap 场景设计）在 Windows
    // 上必须关闭：拖顶/贴边由系统原生 Snap 处理，本模块的 setGeometry
    // 贴靠会与系统最大化/还原状态互相污染（无法还原、边框热区失效）。
    m_edgeSnapEnabled = false;
#else
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
#endif

    m_titleBar = new AuTitleBar(this);
#ifdef Q_OS_WIN
    m_client = new AuClientArea(this);   // 边框热区 HTTRANSPARENT（见类注释）
#else
    m_client = new QWidget(this);
#endif
    m_client->setFocusPolicy(Qt::NoFocus);
    m_clientLayout = new QVBoxLayout(m_client);
    m_clientLayout->setContentsMargins(0, 0, 0, 0);
    m_clientLayout->setSpacing(0);

    connectTitleBar();
    m_titleBar->setTitle(windowTitle());
    m_titleBar->setIcon(windowIcon());

#ifndef Q_OS_WIN
    setupEdgeGrips();
#endif
    AuWindowStyle::registerGlobalObserver(this);
}

AuWindow::~AuWindow()
{
    AuWindowStyle::unregisterGlobalObserver(this);
    // deleteLater 而非直接 delete：预览浮层显示过即被 Qt 无障碍桥
    // （QAccessibleCache）缓存接口，直接删除会在应用退出时留下悬空
    // 缓存条目导致段错误（QAccessibleCache::~QAccessibleCache）；
    // deleteLater 经正常对象销毁流程让缓存先行解绑。
    if (m_snapPreview) {
        m_snapPreview->deleteLater();
        m_snapPreview = nullptr;
    }
    delete m_style;
    m_style = nullptr;
    if (m_eventLoop)
        m_eventLoop->exit(0);
}

void AuWindow::connectTitleBar()
{
    connect(m_titleBar, &AuTitleBar::minimizeButtonClicked,
            this, &QWidget::showMinimized);
    connect(m_titleBar, &AuTitleBar::maximizeButtonClicked, this, [this]() {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
    });
    connect(m_titleBar, &AuTitleBar::closeButtonClicked, this, &QWidget::close);
    connect(m_titleBar, &AuTitleBar::titleDoubleClicked, this, [this]() {
        if (m_titleBar->isButtonVisible(AuTitleBar::ButtonMaximize)) {
            if (isMaximized())
                showNormal();
            else
                showMaximized();
        }
    });
}

// ---- 内容区 ----

QWidget *AuWindow::clientWidget() const
{
    return m_client;
}

void AuWindow::setCentralWidget(QWidget *widget)
{
    if (!widget || widget == m_centralWidget)
        return;
    delete m_centralWidget;
    m_centralWidget = widget;
    if (!m_clientLayout) {
        // 内容区已被 setupUi 转移的布局占用，退化为直接挂到内容区
        widget->setParent(m_client);
        return;
    }
    widget->setParent(m_client);
    m_clientLayout->addWidget(widget);
}

QWidget *AuWindow::centralWidget() const
{
    return m_centralWidget;
}

// ---- 标题栏 ----

AuTitleBar *AuWindow::titleBar() const
{
    return m_titleBar;
}

void AuWindow::setTitleBar(AuTitleBar *bar)
{
    if (!bar || bar == m_titleBar)
        return;
    if (bar->hostWindow() != this) {
        qWarning("AuWindow::setTitleBar: 标题栏必须以本窗体构造（new MyTitleBar(this)）");
        return;
    }
    const QRect oldGeometry = m_titleBar->geometry();
    AuTitleBar *old = m_titleBar;
    m_titleBar = nullptr;
    old->deleteLater();
    m_titleBar = bar;
    if (old->parentWidget() == this)
        bar->setGeometry(oldGeometry);
    bar->setTitle(windowTitle());
    bar->setIcon(windowIcon());
    bar->updateMaximizeState(isMaximized());
    connectTitleBar();
    relayout();
    update();
}

void AuWindow::addTitleBarWidget(QWidget *widget, AuTitleBar::Slot slot, bool grabMouse)
{
    m_titleBar->addWidget(widget, slot, grabMouse);
}

void AuWindow::removeTitleBarWidget(QWidget *widget)
{
    m_titleBar->removeWidget(widget);
}

void AuWindow::setTitleBarWidgetGrabMouse(QWidget *widget, bool grab)
{
    m_titleBar->setWidgetGrabMouse(widget, grab);
}

// ---- 操作按钮 ----

void AuWindow::setButtonVisible(AuTitleBar::Button button, bool visible)
{
    m_titleBar->setButtonVisible(button, visible);
#ifdef Q_OS_WIN
    // WS_MAXIMIZEBOX 跟随最大化按钮可见性（决定 Aero Snap / 双击最大化
    // 是否可用）；需在原生窗口存在后重新应用样式
    if (winId())
        applyWindowStyles();
#endif
}

bool AuWindow::isButtonVisible(AuTitleBar::Button button) const
{
    return m_titleBar->isButtonVisible(button);
}

// ---- 圆角 ----

void AuWindow::setWindowRadius(int radius)
{
    radius = qMax(0, radius);
    if (m_cornerRadii[0] == radius && m_cornerRadii[1] == radius
        && m_cornerRadii[2] == radius && m_cornerRadii[3] == radius)
        return;
    for (int &r : m_cornerRadii)
        r = radius;
#ifdef Q_OS_WIN
    applyWindowRoundedCorners();
#else
    updateMask();
#endif
    relayout(); // 四角裁剪浮层 / 标题栏圆弧跟随
    update();
}

void AuWindow::setCornerRadius(Qt::Corner corner, int radius)
{
    radius = qMax(0, radius);
    if (m_cornerRadii[int(corner)] == radius)
        return;
    m_cornerRadii[int(corner)] = radius;
#ifdef Q_OS_WIN
    applyWindowRoundedCorners();
#else
    updateMask();
#endif
    relayout();
    update();
}

int AuWindow::cornerRadius(Qt::Corner corner) const
{
#ifdef Q_OS_WIN
    // 生效半径：Win10 无系统圆角（直角保 DWM 阴影），强制 0——否则绘制层
    // 会在不透明窗口上画圆角路径，圆角外露出调色板底色（暗色主题下的
    // 白角）。Win11 返回真实值，圆角由 DWM 裁剪（客户端按方形绘制即可）。
    if (!m_dwmCornerOk)
        return 0;
#endif
    return m_cornerRadii[int(corner)];
}

int AuWindow::windowRadius() const
{
#ifdef Q_OS_WIN
    if (!m_dwmCornerOk)
        return 0;
#endif
    return qMax(qMax(m_cornerRadii[0], m_cornerRadii[1]),
                qMax(m_cornerRadii[2], m_cornerRadii[3]));
}

// ---- 关闭拦截 ----

void AuWindow::setCloseInterceptionEnabled(bool enable)
{
    m_closeInterception = enable;
}

bool AuWindow::isCloseInterceptionEnabled() const
{
    return m_closeInterception;
}

void AuWindow::forceClose()
{
    m_forceClosing = true;
    if (m_eventLoop) {
        QEventLoop *loop = m_eventLoop;
        m_eventLoop = nullptr;
        loop->exit(m_result);
        emit finished(m_result);
    }
    // 若本调用发生在 closeEvent 的同步槽内（拦截 → 业务确认 → forceClose），
    // 此刻外层 close 尚未返回，Qt 的 close 重入保护会吞掉第二次 close。
    // 将真正关闭排入事件循环，待外层 close 流程结束后执行。
    QMetaObject::invokeMethod(this, [this]() {
        if (!m_forceClosing)
            return;
        close();               // m_forceClosing=true：closeEvent 不再拦截
        m_forceClosing = false; // close 同步返回后再清除标志
    }, Qt::QueuedConnection);
}

// ---- X11 系统移动（可选） ----

void AuWindow::setSystemWindowMoveEnabled(bool enable)
{
    m_systemMoveEnabled = enable;
}

bool AuWindow::isSystemWindowMoveEnabled() const
{
    return m_systemMoveEnabled;
}

// ---- 任务栏 ----

void AuWindow::setTaskbarVisible(bool visible)
{
    if (m_taskbarVisible == visible)
        return;
    m_taskbarVisible = visible;
#ifdef Q_OS_WIN
    applyTaskbarVisibility();
#elif defined(Q_OS_LINUX)
    if (AuX11::isPlatformX11()) {
        // X11：Tool 窗口即跳过任务栏（_NET_WM_STATE_SKIP_TASKBAR 由 Qt 设置）
        setWindowFlag(Qt::Tool, !visible);
    } else {
        qWarning("AuWindow::setTaskbarVisible: 当前平台不支持运行中切换任务栏可见性");
    }
#else
    qWarning("AuWindow::setTaskbarVisible: 当前平台不支持");
#endif
}

bool AuWindow::isTaskbarVisible() const
{
    return m_taskbarVisible;
}

// ---- Dialog / 模态 ----

void AuWindow::setDialogMode(bool dialog)
{
    if (m_dialogMode == dialog)
        return;
    m_dialogMode = dialog;
    if (isVisible())
        qWarning("AuWindow::setDialogMode: 建议在 show() 之前调用（切换会重建原生窗口）");
    setWindowFlag(Qt::Dialog, dialog);
}

bool AuWindow::isDialogMode() const
{
    return m_dialogMode;
}

void AuWindow::setModality(Modality modality)
{
    setWindowModality(modality == Modality::ApplicationModal ? Qt::ApplicationModal
                                                             : Qt::WindowModal);
}

AuWindow::Modality AuWindow::modality() const
{
    return windowModality() == Qt::ApplicationModal ? Modality::ApplicationModal
                                                    : Modality::WindowModal;
}

int AuWindow::exec()
{
    if (m_eventLoop)
        return -1;
    setResult(0);
    if (windowModality() == Qt::NonModal)
        setWindowModality(parentWidget() ? Qt::WindowModal : Qt::ApplicationModal);
    show();

    QEventLoop loop(this);
    m_eventLoop = &loop;
    const int code = loop.exec();
    m_eventLoop = nullptr;
    return code;
}

void AuWindow::done(int result)
{
    setResult(result);
    hide();
    if (m_eventLoop) {
        QEventLoop *loop = m_eventLoop;
        m_eventLoop = nullptr;
        loop->exit(result);
    }
    emit finished(result);
    if (result == Accepted)
        emit accepted();
    else if (result == Rejected)
        emit rejected();
}

void AuWindow::accept()
{
    done(Accepted);
}

void AuWindow::reject()
{
    done(Rejected);
}

int AuWindow::result() const
{
    return m_result;
}

void AuWindow::setResult(int result)
{
    m_result = result;
}

// ---- 样式 ----

void AuWindow::setStyle(AuWindowStyle *style)
{
    if (m_style == style)
        return;
    delete m_style;
    m_style = style;
    relayout();
#ifdef Q_OS_WIN
    applyWindowRoundedCorners();   // DWM 边框线明暗跟随窗体背景色
#endif
    update();
}

AuWindowStyle *AuWindow::style() const
{
    return m_style ? m_style : AuWindowStyle::globalStyle();
}

void AuWindow::onGlobalStyleChanged()
{
    if (m_style)
        return; // 独立样式不受全局影响
    relayout();
#ifdef Q_OS_WIN
    applyWindowRoundedCorners();   // DWM 边框线明暗跟随窗体背景色
#endif
    update();
}

// ---- 事件 ----

bool AuWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::WinIdChange:
#ifdef Q_OS_WIN
        if (winId()) {
            applyWindowStyles();
            applyWindowRoundedCorners();
            applyTaskbarVisibility();   // 属主窗口需 APPWINDOW 才有任务栏按钮
        }
#endif
        break;
    case QEvent::MouseButtonRelease:
        noteMouseRelease();
        break;
    case QEvent::WindowTitleChange:
    case QEvent::WindowIconChange:
        if (m_titleBar) {
            m_titleBar->setTitle(windowTitle());
            m_titleBar->setIcon(windowIcon());
        }
        break;
    default:
        break;
    }
    return QWidget::event(event);
}

void AuWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_shownOnce) {
        m_shownOnce = true;
#if !defined(Q_OS_WIN) && defined(Q_OS_LINUX)
        if (AuX11::isPlatformX11()) {
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
#endif
        // 布局劫持兜底
        if (layout() && layout() != m_adoptedLayout)
            adoptLayout(layout());
    }
    // Dialog 首次显示时居中于父窗口（事件循环中执行，避免被 WM 的默认放置覆盖）
    if (m_dialogMode && parentWidget() && parentWidget()->window() != this) {
        QWidget *pw = parentWidget()->window();
        QMetaObject::invokeMethod(this, [this, pw]() {
            if (!isVisible() || !pw)
                return;
            const QPoint offset((pw->width() - width()) / 2,
                                (pw->height() - height()) / 2);
            move(pw->mapToGlobal(offset));
        }, Qt::QueuedConnection);
    }
    relayout();
}

void AuWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
#ifdef Q_OS_WIN
    applyWindowRoundedCorners(); // Win10 窗口区域需跟随尺寸
#endif
    relayout();
}

void AuWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    switch (event->type()) {
    case QEvent::WindowStateChange: {
        const bool max = isMaximized();
        m_titleBar->updateMaximizeState(max);
        emit maximizeStateChanged(max);
#ifndef Q_OS_WIN
        relayout(); // 最大化时收起 / 恢复阴影留白
#else
        // Windows：最大化/还原瞬间 Qt 的 resize 优化会把旧帧缓冲 blit 进
        // 新缓冲，只重绘新暴露的边条——标题栏等几何变化未覆盖到的旧像素
        // 会残留（未最大化时的标题栏残影）。relayout + 同步全量重绘立即
        // 清除；再排队一次全量重绘兜底（StateChange 与 Resize 两种到达
        // 顺序都能覆盖）。
        relayout();
        repaint();
        QMetaObject::invokeMethod(this, [this] { repaint(); }, Qt::QueuedConnection);
#endif
        break;
    }
    case QEvent::ActivationChange:
        update();
        break;
    default:
        break;
    }
}

void AuWindow::closeEvent(QCloseEvent *event)
{
    if (m_closeInterception && !m_forceClosing) {
        event->ignore();
        emit closeIntercepted();
        return;
    }
    if (m_eventLoop && !m_forceClosing) {
        // exec 模式下用户关闭 = reject（QDialog 语义）
        event->ignore();
        reject();
        return;
    }
    QWidget::closeEvent(event);
}

void AuWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_eventLoop) {
        reject();
        return;
    }
    QWidget::keyPressEvent(event);
}

void AuWindow::childEvent(QChildEvent *event)
{
    QWidget::childEvent(event);
    // setupUi(this) / setLayout() 安装到窗体上的布局：延迟转移到内容区。
    // ChildAdded 时机布局可能尚未构造完（qobject_cast 失败），因此统一排入
    // 事件队列，待对象完全构造后再判定；ChildPolished 时对象必然构造完成。
    if (event->added() || event->polished()) {
        QObject *child = event->child();
        if (child->isWidgetType() || child == m_adoptedLayout)
            return;
        QPointer<QObject> guard(child);
        QMetaObject::invokeMethod(this, [this, guard]() {
            if (!guard || guard->parent() != this)
                return;
            if (QLayout *lay = qobject_cast<QLayout *>(guard.data()))
                adoptLayout(lay);
        }, Qt::QueuedConnection);
    }
}

void AuWindow::adoptLayout(QLayout *layout)
{
    if (!layout || layout->parent() != this)
        return;
    if (layout == m_adoptedLayout)
        return;
    // 内容区已有布局（占位或旧的用户布局）：先移除（QLayout 析构不删除受管控件）
    if (QLayout *old = m_client->layout()) {
        if (old == m_clientLayout) {
            m_clientLayout = nullptr;
            if (m_centralWidget) {
                // 占位布局中挂着的中央控件一并移除（其内容已被用户布局取代）
                m_centralWidget->deleteLater();
                m_centralWidget = nullptr;
            }
        } else {
            qWarning("AuWindow: 内容区已存在先前的用户布局，将被替换（受管控件保留但不再受布局管理）");
        }
        delete old;
    }
    // QWidget::setLayout 会把布局连同受管控件一起转移到内容区
    m_client->setLayout(layout);
    m_adoptedLayout = layout;
}

// ---- 几何 ----

int AuWindow::effectiveMargin() const
{
    // 阴影留白环：Windows 阴影由 DWM 在窗口外绘制；X11 对无边框窗口没有
    // WM 侧阴影通道（_KDE_NET_WM_SHADOW 需客户端供图，按需求不做客户端
    // 阴影），故所有平台窗口均为纯内容矩形。
    return 0;
}

void AuWindow::relayout()
{
    const int margin = effectiveMargin();
    const QRect content = rect().adjusted(margin, margin, -margin, -margin);
    const int tbH = (m_titleBar && !m_titleBar->isHidden())
                        ? qMin(m_titleBar->sizeHint().height(), content.height())
                        : 0;
    if (m_titleBar)
        m_titleBar->setGeometry(content.left(), content.top(), content.width(), tbH);
    if (m_client)
        m_client->setGeometry(content.left(), content.top() + tbH,
                              content.width(), qMax(0, content.height() - tbH));
#ifndef Q_OS_WIN
    relayoutEdgeGrips();
#endif
    updateCornerClips(content);
    updateMask();
}

// ---- 四角圆角裁剪浮层 ----

// 混成环境下内容子控件可能把圆角弧外像素覆盖成直角（锯齿）；
// 在四个角上摆放 AuCornerClip（内容之上）实时清除弧外像素。
void AuWindow::updateCornerClips(const QRect &content)
{
#ifdef Q_OS_WIN
    Q_UNUSED(content)
    // Windows：Win11 DWM 圆角 / Win10 窗口区域由系统裁剪全部内容，无需浮层
#else
    const bool maximized = isMaximized() || isFullScreen();
    bool composited = true;
#  if defined(Q_OS_LINUX)
    if (AuX11::isPlatformX11())
        composited = m_x11Compositing;   // 无混成走 mask 路径，浮层无意义
#  endif
    const bool needed = composited && !maximized && windowRadius() > 0
                        && !content.isEmpty();
    for (int i = 0; i < 4; ++i) {
        if (!needed) {
            if (m_cornerClips[i])
                m_cornerClips[i]->hide();
            continue;
        }
        if (!m_cornerClips[i])
            m_cornerClips[i] = new AuCornerClip(this, Qt::Corner(i));
        const int r = m_cornerRadii[i];
        const QRect square = [i, &content, r]() {
            switch (Qt::Corner(i)) {
            case Qt::TopLeftCorner:
                return QRect(content.left(), content.top(), r, r);
            case Qt::TopRightCorner:
                return QRect(content.right() - r + 1, content.top(), r, r);
            case Qt::BottomLeftCorner:
                return QRect(content.left(), content.bottom() - r + 1, r, r);
            case Qt::BottomRightCorner:
            default:
                return QRect(content.right() - r + 1, content.bottom() - r + 1, r, r);
            }
        }();
        if (m_cornerClips[i]->geometry() != square)
            m_cornerClips[i]->setGeometry(square);
        m_cornerClips[i]->setVisible(r > 0);
        m_cornerClips[i]->raise();
        m_cornerClips[i]->update();
    }
#endif
}

void AuWindow::ensurePolishedFrame()
{
#ifdef Q_OS_WIN
    applyWindowStyles();
    applyWindowRoundedCorners();
    applyTaskbarVisibility();
#endif
}

// ---- 绘制 ----

void AuWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    const AuWindowStyle *st = style();
    const int margin = effectiveMargin();
    const QRect content = rect().adjusted(margin, margin, -margin, -margin);

    const bool rounded = !isMaximized() && !isFullScreen()
                         && windowRadius() > 0;
    if (rounded) {
        // 抗锯齿：混成环境下窗口为 ARGB 透明背景，圆角靠 alpha 通道平滑
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPainterPath path = auRoundedRectPath(
            QRectF(content), m_cornerRadii[0], m_cornerRadii[1],
            m_cornerRadii[2], m_cornerRadii[3]);
        painter.setPen(Qt::NoPen);
        painter.setBrush(st->windowBackgroundColor());
        painter.drawPath(path);
        if (st->windowBorderColor().isValid()) {
            painter.setPen(QPen(st->windowBorderColor(), 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        }
        painter.setRenderHint(QPainter::Antialiasing, false);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(st->windowBackgroundColor());
        painter.drawRect(content);
        if (st->windowBorderColor().isValid()) {
            painter.setPen(QPen(st->windowBorderColor(), 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(content).adjusted(0.5, 0.5, -0.5, -0.5));
        }
    }
}

#ifndef Q_OS_WIN
void AuWindow::updateMask()
{
    const bool noMaskNeeded = windowRadius() <= 0 || isMaximized() || isFullScreen();
#if defined(Q_OS_LINUX)
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
#endif
    if (noMaskNeeded) {
        if (!mask().isNull())
            clearMask();
        return;
    }
    const int margin = effectiveMargin();
    const QRect content = rect().adjusted(margin, margin, -margin, -margin);
    const QPainterPath path = auRoundedRectPath(
        QRectF(content), m_cornerRadii[0], m_cornerRadii[1],
        m_cornerRadii[2], m_cornerRadii[3]);
    const QRegion rounded(path.toFillPolygon().toPolygon());
    const QRegion outer(rect());
    // 内容区圆角裁剪 + 阴影环保留（阴影自身带 alpha 渐隐）
    setMask(rounded | (outer - QRegion(content)));
}
#else
void AuWindow::updateMask() {}
#endif

// ---- 手动拖动 / 缩放（跨平台兜底路径；X11 细节由函数内的平台宏裁剪，
//      Windows 走原生 HTCAPTION / 边框热区，本节代码仅作链接兜底） ----

/// 边缘缩放手柄：贴窗口边缘的透明子控件，Qt 按悬停位置自动切换缩放光标，
/// 按下后发起缩放（_NET_WM_MOVERESIZE 原生路径，WM 忽略时回退手动）。
class AuEdgeGrip : public QWidget
{
public:
    AuEdgeGrip(AuWindow *window, Qt::Edges edges)
        : QWidget(window), m_window(window), m_edges(edges)
    {
        setCursor(edgeCursor(edges));
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
    }

    static Qt::CursorShape edgeCursor(Qt::Edges edges)
    {
        if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge))
            return Qt::SizeFDiagCursor;
        if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge))
            return Qt::SizeBDiagCursor;
        if (edges == Qt::LeftEdge || edges == Qt::RightEdge)
            return Qt::SizeHorCursor;
        return Qt::SizeVerCursor;
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            m_window->performBorderResize(m_edges, event->globalPos());
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        Q_UNUSED(event)
        m_window->noteMouseRelease();
        if (m_window)
            m_window->releaseMouse();
    }

private:
    AuWindow *m_window;
    Qt::Edges m_edges;
};

// 拖动标题栏：优先交给窗口管理器/系统，获得完整的原生行为——拖顶最大化
// 预览、边缘半屏、snap-assist、拖出屏幕、最大化窗口还原跟随；系统不接
// 管时回退为手动拖动。
void AuWindow::performTitleBarDrag(const QPoint &pressGlobal)
{
#ifdef Q_OS_WIN
    // Windows：标题栏拖动一律交给系统移动循环（SC_MOVE）。到达本函数说明
    // 按下落在 HTCLIENT 区（标题栏上的自定义组件），系统不会自发进入移动
    // 循环；先释放进程鼠标捕获，再重放一次 HTCAPTION 按下，让 DefWindowProc
    // 启动原生拖动——Aero Snap / Snap Assist / 还原矩形全部由系统维护。
    // Qt 5.12 无 startSystemMove，若走 Qt 手动 move() 拖动会完全绕过系统
    // 吸附（无 Assist、还原矩形污染），且手动路径自带的边缘吸附会在
    // Windows 上与系统最大化状态互相污染（实测：拖顶后无法还原、边框热区
    // 失效、焦点切换闪现经典边框）。
    Q_UNUSED(pressGlobal)
    if (windowHandle())
        windowHandle()->setMouseGrabEnabled(false);
    if (m_titleBar)
        m_titleBar->releaseMouse();
    ReleaseCapture();
    m_titleBar->resetDragState();
    PostMessageW(reinterpret_cast<HWND>(winId()), WM_NCLBUTTONDOWN, HTCAPTION,
                 MAKELPARAM(QCursor::pos().x(), QCursor::pos().y()));
    return;
#else
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    // Qt 5.15+：官方 startSystemMove（内部即 _NET_WM_MOVERESIZE，且已修复
    // 接管后事件管线冻结的 QTBUG-69716），跨 X11/Wayland。
    if (m_systemMoveEnabled && windowHandle() && windowHandle()->startSystemMove()) {
        startGrabRecoveryPoll();
        return;
    }
#else
    // Qt 5.12：手写 EWMH 消息（button=0 + 解除 Qt 连接隐式抓取），已知
    // 接管后 motion/enter 永久丢失（QTBUG-69716），仅显式开启时使用。
    if (m_systemMoveEnabled && auSendX11MoveResize(winId(), QCursor::pos(), 8)) {
        startGrabRecoveryPoll();
        return;
    }
#endif
    startManualMove(pressGlobal);
#endif
}

void AuWindow::startManualMove(const QPoint &pressGlobal)
{
    m_manualMoving = true;
    m_manualStartGlobal = pressGlobal;
    m_manualStartPos = pos();
    m_manualStartSize = size();
#ifndef Q_OS_WIN
    // 拖出贴靠状态：先还原吸附前尺寸再跟随（Windows 同款行为）。
    if (m_snapped) {
        m_snapped = false;
        if (isMaximized()) {
            // 顶部吸附走的是最大化状态：尽力还原（ukui-kwin 在按住期间
            // 可能忽略 showNormal，未落地则该次保持最大化拖动无效）
            showNormal();
        }
        const QRect pre = m_preSnapGeometry;
        if (pre.isValid() && !isMaximized()) {
            // 保持抓取点在标题栏内的相对水平位置，垂直偏移不变
            const qreal ratio = m_manualStartSize.width() > 0
                    ? qreal(pressGlobal.x() - m_manualStartPos.x()) / m_manualStartSize.width()
                    : 0.0;
            const QPoint newPos(pressGlobal.x() - int(qreal(pre.width()) * ratio),
                                m_manualStartPos.y());
            setGeometry(QRect(newPos, pre.size()));
            m_manualStartPos = newPos;
            m_manualStartSize = pre.size();
        }
    }
#endif
    grabMouse();
}

void AuWindow::startManualResize(Qt::Edges edges, const QPoint &pressGlobal)
{
    m_manualResizing = true;
    m_manualEdges = edges;
    m_manualStartGlobal = pressGlobal;
    m_manualStartPos = pos();
    m_manualStartSize = size();
    m_snapped = false;   // 手动缩放后不再是吸附几何，还原基准作废
    grabMouse();
}

// 发起边缘缩放：优先系统原生缩放（KWin 路径，含相对边缘吸附行为），
// 未接管时（仍收到本进程事件）回退手动缩放。
void AuWindow::performBorderResize(Qt::Edges edges, const QPoint &pressGlobal)
{
    m_pressGlobal = pressGlobal;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    if (m_systemMoveEnabled && windowHandle()
        && windowHandle()->startSystemResize(edges)) {
        m_systemResizePending = true;
        m_systemResizeSentMs = QDateTime::currentMSecsSinceEpoch();
        m_pendingEdges = edges;
        startGrabRecoveryPoll();
        return;
    }
#else
    if (m_systemMoveEnabled
        && auSendX11MoveResize(winId(), QCursor::pos(), auEdgesToMoveResizeDirection(edges))) {
        m_systemResizePending = true;
        m_systemResizeSentMs = QDateTime::currentMSecsSinceEpoch();
        m_pendingEdges = edges;
        startGrabRecoveryPoll();
        return;
    }
#endif
    startManualResize(edges, pressGlobal);
}

// ---- 边缘吸附（手动拖动路径） ----

void AuWindow::setEdgeSnapEnabled(bool enable)
{
    m_edgeSnapEnabled = enable;
#ifndef Q_OS_WIN
    if (!enable)
        hideSnapPreview();
#endif
}

bool AuWindow::isEdgeSnapEnabled() const
{
    return m_edgeSnapEnabled;
}

// 光标落在哪个吸附带：顶部（需标题栏有最大化按钮）、左 / 右屏幕边缘。
// 以光标位置判定（用户预期：鼠标靠近边缘才出现吸附）。手动拖动时窗口
// 会被屏幕边缘钳制，但光标会继续越过钳制位置走到边缘带，因此抓取点
// 在标题栏内的偏移不影响触发。
AuWindow::SnapEdge AuWindow::edgeSnapTarget(const QPoint &globalPos) const
{
    if (!m_edgeSnapEnabled)
        return SnapEdge::None;
    QWindow *wh = windowHandle();
    const QRect avail = wh ? wh->screen()->availableGeometry() : QRect();
    if (!avail.isValid())
        return SnapEdge::None;
    const int zone = 10;
    if (m_titleBar && m_titleBar->isButtonVisible(AuTitleBar::ButtonMaximize)
        && globalPos.y() - avail.top() <= zone)
        return SnapEdge::Top;
    if (globalPos.x() - avail.left() <= zone)
        return SnapEdge::Left;
    if (avail.right() - globalPos.x() <= zone)
        return SnapEdge::Right;
    return SnapEdge::None;
}

// 吸附目标区域：顶部为整个可用区，左右各半屏。
QRect AuWindow::edgeSnapRect(SnapEdge edge) const
{
    QWindow *wh = windowHandle();
    const QRect avail = wh ? wh->screen()->availableGeometry() : QRect();
    if (!avail.isValid())
        return QRect();
    const int half = avail.width() / 2;
    switch (edge) {
    case SnapEdge::Top:
        return avail;
    case SnapEdge::Left:
        return QRect(avail.left(), avail.top(), half, avail.height());
    case SnapEdge::Right:
        return QRect(avail.left() + avail.width() - half, avail.top(),
                     avail.width() - half, avail.height());
    default:
        break;
    }
    return QRect();
}

void AuWindow::showSnapPreview(SnapEdge edge)
{
#if defined(Q_OS_LINUX)
    // 无混成时窗口无 alpha 通道，铺屏预览会是不透明色块，跳过（直接吸附）
    if (AuX11::isPlatformX11() && !m_x11Compositing)
        return;
#endif
    if (!m_snapPreview)
        m_snapPreview = new AuSnapPreview();
    m_snapPreview->showFor(edgeSnapRect(edge));
}

void AuWindow::hideSnapPreview()
{
    if (m_snapPreview && m_snapPreview->isVisible())
        m_snapPreview->hide();
}

// 松手应用吸附：顶部最大化；左右半屏按阴影环补偿外扩（可见内容区铺满半屏）。
// 记录吸附前的浮动几何，供下次拖动标题栏时还原跟随。
void AuWindow::applyEdgeSnap(SnapEdge edge)
{
    hideSnapPreview();
    if (edge == SnapEdge::None)
        return;
    if (!isMaximized())
        m_preSnapGeometry = QRect(m_manualStartPos, m_manualStartSize);
    m_snapped = true;
    switch (edge) {
    case SnapEdge::Top:
        showMaximized();
        break;
    case SnapEdge::Left:
    case SnapEdge::Right: {
        const QRect r = edgeSnapRect(edge);
        if (!r.isValid())
            break;
        const int m = effectiveMargin();
        setGeometry(r.x() - m, r.y() - m, r.width() + 2 * m, r.height() + 2 * m);
        break;
    }
    default:
        break;
    }
}

// X11：发送 _NET_MOVERESIZE_WINDOW（EWMH 分页器消息）仅携带 x 字段，
// 由 WM 直接应用窗口 x 位置——不经过普通移动请求（ConfigureRequest）的
// 边界钳制，窗口可拖出屏幕左右，与原生窗口拖动行为一致。
// 注：实测 ukui-kwin 对同时携带 y/w/h 的请求处理异常（被忽略），
// 故只发 x；y 与缩放走普通通道。
bool AuWindow::sendNetMoveResizeWindow(const QPoint &pos)
{
#if !defined(Q_OS_WIN) && defined(Q_OS_LINUX)
    if (!AuX11::isPlatformX11())
        return false;
    Display *display = AuX11::display();
    if (!display)
        return false;
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = static_cast<Window>(winId());
    ev.xclient.message_type = XInternAtom(display, "_NET_MOVERESIZE_WINDOW", False);
    ev.xclient.format = 32;
    // flags: bit0=x 为绝对值；bit8-10 = gravity（NorthWest=1）
    ev.xclient.data.l[0] = 0x1 | (1 << 8);
    ev.xclient.data.l[1] = pos.x();
    XSendEvent(display, XDefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(display);
    return true;
#else
    Q_UNUSED(pos)
    return false;
#endif
}

void AuWindow::applyPositionViaWM(const QPoint &pos)
{
#if !defined(Q_OS_WIN) && defined(Q_OS_LINUX)
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
    if (!sendNetMoveResizeWindow(pos))
        m_wmGeometryBroken = true;   // 消息发送失败，回退普通移动
#else
    move(pos);
#endif
}

void AuWindow::manualResizeStep(const QPoint &globalPos)
{
    const QPoint delta = globalPos - m_manualStartGlobal;
    const QSize minSize = minimumSize().expandedTo(QSize(1, 1));
    const QSize maxSize = maximumSize(); // 默认极大值，钳制无副作用
    QRect geo(m_manualStartPos, m_manualStartSize);

    // 移动中的边按 [最小尺寸, 最大尺寸] 钳制（固定尺寸时 min==max → 该边不可动）
    if (m_manualEdges.testFlag(Qt::LeftEdge)) {
        int left = m_manualStartPos.x() + delta.x();
        left = qBound(geo.right() + 1 - maxSize.width(),
                      left,
                      geo.right() + 1 - minSize.width());
        geo.setLeft(left);
    }
    if (m_manualEdges.testFlag(Qt::RightEdge)) {
        int right = m_manualStartPos.x() + m_manualStartSize.width() - 1 + delta.x();
        right = qBound(geo.left() + minSize.width() - 1,
                       right,
                       geo.left() + maxSize.width() - 1);
        geo.setRight(right);
    }
    if (m_manualEdges.testFlag(Qt::TopEdge)) {
        int top = m_manualStartPos.y() + delta.y();
        top = qBound(geo.bottom() + 1 - maxSize.height(),
                     top,
                     geo.bottom() + 1 - minSize.height());
        geo.setTop(top);
    }
    if (m_manualEdges.testFlag(Qt::BottomEdge)) {
        int bottom = m_manualStartPos.y() + m_manualStartSize.height() - 1 + delta.y();
        bottom = qBound(geo.top() + minSize.height() - 1,
                        bottom,
                        geo.top() + maxSize.height() - 1);
        geo.setBottom(bottom);
    }
    setGeometry(geo);
}

// X11：在窗口边缘布置透明缩放手柄。阴影环（margin>0）或窗口边缘（margin=0）
// 即手柄区域，光标由 Qt 依据手柄的 cursor 自动切换。
void AuWindow::setupEdgeGrips()
{
    if (!m_edgeGrips.isEmpty())
        return;
    struct GripDef { Qt::Edges edges; };
    const GripDef defs[8] = {
        { Qt::TopEdge }, { Qt::BottomEdge }, { Qt::LeftEdge }, { Qt::RightEdge },
        { Qt::LeftEdge | Qt::TopEdge }, { Qt::RightEdge | Qt::TopEdge },
        { Qt::LeftEdge | Qt::BottomEdge }, { Qt::RightEdge | Qt::BottomEdge }
    };
    for (const GripDef &def : defs) {
        AuEdgeGrip *grip = new AuEdgeGrip(this, def.edges);
        grip->setObjectName(QStringLiteral("auwindow_edge_grip"));
        m_edgeGrips.append(grip);
    }
    relayoutEdgeGrips();
}

void AuWindow::relayoutEdgeGrips()
{
    if (m_edgeGrips.isEmpty())
        return;
    const bool maximized = isMaximized() || isFullScreen();
    // 固定尺寸（min==max）的维度不提供缩放手柄
    const bool wResizable = minimumWidth() != maximumWidth();
    const bool hResizable = minimumHeight() != maximumHeight();
    const int depth = qMax(effectiveMargin(), 6);   // 手柄厚度：阴影环宽或 6px
    const int w = width(), h = height();
    auto place = [&](int index, const QRect &geo, bool resizable) {
        QWidget *grip = m_edgeGrips.at(index);
        grip->setGeometry(geo);
        grip->setVisible(!maximized && resizable && !geo.isEmpty());
        grip->raise();
    };
    // 0=N 1=S 2=W 3=E 4=NW 5=NE 6=SW 7=SE
    place(0, QRect(0, 0, w, depth), hResizable);
    place(1, QRect(0, h - depth, w, depth), hResizable);
    place(2, QRect(0, depth, depth, qMax(0, h - 2 * depth)), wResizable);
    place(3, QRect(w - depth, depth, depth, qMax(0, h - 2 * depth)), wResizable);
    place(4, QRect(0, 0, depth, depth), wResizable && hResizable);
    place(5, QRect(w - depth, 0, depth, depth), wResizable && hResizable);
    place(6, QRect(0, h - depth, depth, depth), wResizable && hResizable);
    place(7, QRect(w - depth, h - depth, depth, depth), wResizable && hResizable);
}

void AuWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_manualResizing) {
        manualResizeStep(QCursor::pos());
        return;
    }
    if (m_manualMoving) {
        const QPoint newPos = m_manualStartPos + (QCursor::pos() - m_manualStartGlobal);
        applyPositionViaWM(newPos);
        const SnapEdge snap = edgeSnapTarget(QCursor::pos());
        if (snap != SnapEdge::None)
            showSnapPreview(snap);
        else
            hideSnapPreview();
        return;
    }
#ifndef Q_OS_WIN
    if (m_systemResizePending) {
        // 已发系统缩放请求却持续收到本进程事件（超出接管宽限期）：
        // WM 未接管，回退手动缩放。宽限期内忽略在途事件，避免与 WM
        // 的指针抓取相撞（同 AuTitleBar 拖动回退）。
        if (QDateTime::currentMSecsSinceEpoch() - m_systemResizeSentMs > 150) {
            m_systemResizePending = false;
            if (event->buttons() & Qt::LeftButton) {
                startManualResize(m_pendingEdges, m_pressGlobal);
                return;
            }
        }
    }
#endif
    QWidget::mouseMoveEvent(event);
}

void AuWindow::mouseReleaseEvent(QMouseEvent *event)
{
    noteMouseRelease();
    m_systemResizePending = false;
    if (m_manualResizing || m_manualMoving) {
        if (m_manualMoving)
            applyEdgeSnap(edgeSnapTarget(event->globalPos()));
        else
            hideSnapPreview();
        m_manualResizing = false;
        m_manualMoving = false;
        releaseMouse();
    }
    QWidget::mouseReleaseEvent(event);
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
#if defined(Q_OS_LINUX)
    if (!AuX11::isPlatformX11())
        return;
    m_awaitingRelease = true;
    if (!m_grabRecoveryTimer) {
        m_grabRecoveryTimer = new QTimer(this);
        m_grabRecoveryTimer->setInterval(25);
        connect(m_grabRecoveryTimer, &QTimer::timeout, this, &AuWindow::pollGrabRecovery);
    }
    m_grabRecoveryTimer->start();
#endif
}

void AuWindow::pollGrabRecovery()
{
#if defined(Q_OS_LINUX)
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
#endif
}

void AuWindow::noteMouseRelease()
{
    m_awaitingRelease = false;
    if (m_grabRecoveryTimer)
        m_grabRecoveryTimer->stop();
}

#ifdef Q_OS_WIN

// ---- Windows：原生无边框实现 ----

void AuWindow::applyWindowStyles()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    // 保留 WS_THICKFRAME（系统缩放热区 / DWM 阴影 / Aero Snap）等原生样式，
    // 剥离 WS_CAPTION：caption 框架走 DWM 合成路径绘制，WM_NCACTIVATE 的
    // lParam=-1 只能拦住 DefWindowProc 的重绘、拦不住 DWM 自身的经典边框
    // 呈现（实测：带 caption 时启动闪经典边框、焦点切换持续闪烁；无 caption
    // 时两者皆无）。保留 Qt 默认的 WS_POPUP 基底。原生矩形与可见边界的
    // ~7px 差异（不可见调整边框）由命中测试按 DWM 可见矩形对齐解决。
    // Snap / 拖动 / 双击最大化由 WM_NCHITTEST 的 HTCAPTION 提供。
    //
    // WS_MAXIMIZEBOX 动态判定：系统 Aero Snap（拖顶最大化 / 贴边半屏）与
    // 双击标题栏最大化只对带最大化框的窗口生效。固定尺寸（min==max）或
    // 隐藏了最大化按钮的窗体不置位——拖拽到屏幕边缘不触发任何吸附。
    LONG_PTR wanted = WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX;
    const bool resizable = minimumSize() != maximumSize();
    if (resizable && m_titleBar && m_titleBar->isButtonVisible(AuTitleBar::ButtonMaximize))
        wanted |= WS_MAXIMIZEBOX;
    const LONG_PTR updated = (style & ~(WS_CAPTION | WS_MAXIMIZEBOX)) | wanted;
    if (updated == style)
        return;
    SetWindowLongPtrW(hwnd, GWL_STYLE, updated);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                     | SWP_NOACTIVATE);
    // 无 caption 的窗口 DWM 默认不绘制框架阴影；向客户区扩展 1px 边框
    // 强制 DWM 恢复完整框架渲染（外阴影 + 系统细边框，Chromium 同款手法）
    typedef HRESULT(WINAPI * DwmExtendFrameIntoClientAreaPtr)(HWND, const MARGINS *);
    static DwmExtendFrameIntoClientAreaPtr pExtend = []() {
        HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        return dwm ? reinterpret_cast<DwmExtendFrameIntoClientAreaPtr>(
                         GetProcAddress(dwm, "DwmExtendFrameIntoClientArea"))
                   : nullptr;
    }();
    if (pExtend) {
        const MARGINS margins = {0, 0, 0, 1};
        pExtend(hwnd, &margins);
    }
}

void AuWindow::applyWindowRoundedCorners()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;
    // 注意读原始半径（m_cornerRadii）：windowRadius() 在 Win10 上会强制
    // 返回生效值 0，此处需要真实设置值来判定 DWM 偏好
    const int maxR = qMax(qMax(m_cornerRadii[0], m_cornerRadii[1]),
                          qMax(m_cornerRadii[2], m_cornerRadii[3]));
    const int pref = maxR <= 0 ? 1        // DWMWCP_DONOTROUND
                     : maxR <= 5 ? 3      // DWMWCP_ROUNDSMALL（4px）
                                 : 2;     // DWMWCP_ROUND（8px）
    typedef HRESULT(WINAPI * DwmSetWindowAttributePtr)(HWND, DWORD, LPCVOID, DWORD);
    static DwmSetWindowAttributePtr pDwmSetWindowAttribute = []() {
        HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        return dwm ? reinterpret_cast<DwmSetWindowAttributePtr>(
                         GetProcAddress(dwm, "DwmSetWindowAttribute"))
                   : nullptr;
    }();
    if (pDwmSetWindowAttribute
        && SUCCEEDED(pDwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */,
                                            &pref, sizeof(pref)))) {
        m_dwmCornerOk = true;   // Win11+：系统圆角 + 配套 DWM 阴影
    } else {
        // Win10 及以下：放弃窗口区域（SetWindowRgn）圆角。区域会把窗口转入
        // 区域框架模式——DWM 停止渲染该窗口框架（阴影消失）、激活切换时
        // 退回经典 GDI 边框（闪现原始边框）、吸附记账退化。取舍（已与使用
        // 方确认）：保系统原生阴影，圆角降级为直角。setWindowRadius() /
        // setCornerRadius() 在 Win10 上无视觉效果（Win11 仍生效）。
        m_dwmCornerOk = false;
    }
    // DWM 框架元素（边框线等）跟随窗体明暗主题：暗色窗体下 DWM 边框线也
    // 是暗色，避免暗色界面四周出现一圈突兀的浅色描边。Win10 1809+ /
    // Win11 支持；不支持时静默忽略。随样式切换重调本函数即可刷新。
    if (pDwmSetWindowAttribute) {
        const QColor bg = style()->windowBackgroundColor();
        const BOOL dark = bg.lightness() < 128;
        pDwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */,
                               &dark, sizeof(dark));
    }
}

void AuWindow::applyTaskbarVisibility()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    // WS_EX_APPWINDOW：有属主（owner）的窗口默认不进任务栏（系统对话框规
    // 则）；置上该扩展样式强制获得任务栏按钮（同时具备缩略图预览）。
    // 隐藏任务栏时清掉它并置 WS_EX_TOOLWINDOW（连 Alt+Tab 一并隐藏）。
    LONG_PTR wanted = m_taskbarVisible ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW;
    LONG_PTR clear = m_taskbarVisible ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW;
    if (((exStyle & wanted) == wanted) && ((exStyle & clear) == 0))
        return;
    exStyle = (exStyle & ~clear) | wanted;
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                     | SWP_NOACTIVATE);
}

bool AuWindow::handleNcCalcSize(void *message, long *result)
{
    MSG *msg = static_cast<MSG *>(message);
    *result = 0;
    if (!msg->wParam)
        return true;
    NCCALCSIZE_PARAMS *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
    const HWND hwnd = msg->hwnd;
    if (IsZoomed(hwnd)) {
        // 最大化时窗口矩形比工作区大一圈边框，把客户区缩回工作区
        const int frameX = GetSystemMetrics(SM_CXSIZEFRAME)
                           + GetSystemMetrics(SM_CXPADDEDBORDER);
        const int frameY = GetSystemMetrics(SM_CYSIZEFRAME)
                           + GetSystemMetrics(SM_CXPADDEDBORDER);
        params->rgrc[0].left += frameX;
        params->rgrc[0].top += frameY;
        params->rgrc[0].right -= frameX;
        params->rgrc[0].bottom -= frameY;
        // 自动隐藏的任务栏所在边额外让出 2px，避免误触唤出
        APPBARDATA state = {sizeof(state)};
        if ((SHAppBarMessage(ABM_GETSTATE, &state) & ABS_AUTOHIDE) != 0) {
            APPBARDATA pos = {sizeof(pos)};
            SHAppBarMessage(ABM_GETTASKBARPOS, &pos);
            switch (pos.uEdge) {
            case ABE_LEFT:   params->rgrc[0].left += 2; break;
            case ABE_TOP:    params->rgrc[0].top += 2; break;
            case ABE_RIGHT:  params->rgrc[0].right -= 2; break;
            case ABE_BOTTOM: params->rgrc[0].bottom -= 2; break;
            default: break;
            }
        }
    }
    return true; // 返回 0：移除全部可见非客户区（保留 DWM 阴影 / Snap / 动画）
}

bool AuWindow::handleNcHitTest(void *message, long *result)
{
    MSG *msg = static_cast<MSG *>(message);
    const HWND hwnd = msg->hwnd;
    const QPoint global(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
    // 以 DWM 可见矩形为基准（见 auVisibleFrameBounds）：原生矩形含不可见
    // 调整边框，直接用它判热区会把缩放热区挪到不可见条带上
    const RECT rc = auVisibleFrameBounds(hwnd);
    const int winW = rc.right - rc.left;
    const int winH = rc.bottom - rc.top;
    const QPoint local(global.x() - rc.left, global.y() - rc.top);
    LRESULT hit = HTCLIENT;

    // 边框缩放热区（最大化 / 全屏时禁用；固定尺寸的轴不提供热区——
    // 固定尺寸对话框不应出现缩放光标）
    if (!IsZoomed(hwnd) && !isFullScreen()) {
        // 热区用系统标准边框厚，不加宽：过宽会侵入标题栏 / 操作按钮区，
        // 把标题栏拖动误判为缩放（吸附后标题栏出现上下拉伸光标即此因）
        const int frame = GetSystemMetrics(SM_CXSIZEFRAME)
                          + GetSystemMetrics(SM_CXPADDEDBORDER);
        const QSize minS = minimumSize();
        const QSize maxS = maximumSize();
        const bool hResizable = minS.width() != maxS.width();
        const bool vResizable = minS.height() != maxS.height();
        const bool left = hResizable && local.x() < frame;
        const bool right = hResizable && local.x() >= winW - frame;
        const bool top = vResizable && local.y() < frame;
        const bool bottom = vResizable && local.y() >= winH - frame;
        if (top && left)
            hit = HTTOPLEFT;
        else if (top && right)
            hit = HTTOPRIGHT;
        else if (bottom && left)
            hit = HTBOTTOMLEFT;
        else if (bottom && right)
            hit = HTBOTTOMRIGHT;
        else if (top)
            hit = HTTOP;
        else if (bottom)
            hit = HTBOTTOM;
        else if (left)
            hit = HTLEFT;
        else if (right)
            hit = HTRIGHT;
    }

    // 标题栏空白区交给系统作为标题栏：原生拖动 / 边缘分屏（Aero Snap）/ 双击最大化
    if (hit == HTCLIENT && m_titleBar && m_titleBar->isVisible()) {
        const QPoint tbPos = m_titleBar->mapFromGlobal(global);
        if (m_titleBar->rect().contains(tbPos) && m_titleBar->hitTestCaption(tbPos))
            hit = HTCAPTION;
    }

    *result = static_cast<long>(hit);
    return true;
}

bool AuWindow::handleMinMaxInfo(void *message, long *result)
{
    MSG *msg = static_cast<MSG *>(message);
    MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO *>(msg->lParam);

    // 无 WS_CAPTION 的弹出式窗口，系统默认最大化到整个显示器：盖住任务栏，
    // 且四周不外溢边框——与 WM_NCCALCSIZE 的最大化内缩不匹配（客户区缩小
    // 一圈边框后，顶部/四周会露出一圈无人绘制的残留条带）。这里改写最大化
    // 目标为工作区，并按有 caption 窗口的惯例让边框外溢到屏幕外：客户区经
    // NCCALCSIZE 内缩后恰好铺满工作区，任务栏保持可见、残留消失。
    HMONITOR monitor = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info;
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        const int frameX = GetSystemMetrics(SM_CXSIZEFRAME)
                           + GetSystemMetrics(SM_CXPADDEDBORDER);
        const int frameY = GetSystemMetrics(SM_CYSIZEFRAME)
                           + GetSystemMetrics(SM_CXPADDEDBORDER);
        mmi->ptMaxSize.x = (info.rcWork.right - info.rcWork.left) + 2 * frameX;
        mmi->ptMaxSize.y = (info.rcWork.bottom - info.rcWork.top) + 2 * frameY;
        mmi->ptMaxPosition.x = info.rcWork.left - frameX;
        mmi->ptMaxPosition.y = info.rcWork.top - frameY;
    }

    // 原生边框缩放遵守窗体最小/最大宽高（最大跟踪尺寸不钳制时，固定尺寸
    // 窗口仍可被系统缩放循环拉大——热区判定之外的第二道保险）
    mmi->ptMinTrackSize.x = qMax<LONG>(minimumWidth(), mmi->ptMinTrackSize.x);
    mmi->ptMinTrackSize.y = qMax<LONG>(minimumHeight(), mmi->ptMinTrackSize.y);
    mmi->ptMaxTrackSize.x = qMin<LONG>(maximumWidth(), mmi->ptMaxTrackSize.x);
    mmi->ptMaxTrackSize.y = qMin<LONG>(maximumHeight(), mmi->ptMaxTrackSize.y);
    *result = 0;
    return true;
}

bool AuWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    if (eventType != QByteArrayLiteral("windows_generic_MSG"))
        return QWidget::nativeEvent(eventType, message, result);
    MSG *msg = static_cast<MSG *>(message);
    switch (msg->message) {
    case WM_NCACTIVATE:
        // 激活/失活统一传 lParam=-1：DefWindowProc 更新激活状态但跳过非
        // 客户区重绘。无 WS_CAPTION 时系统仍会在激活切换瞬间以传统路径
        // 呈现一帧经典框架（DWM 合成侧，日志证实无 WM_NCPAINT 到达窗口），
        // -1 让 USER32 连这一帧也不画。早期版本曾怀疑它破坏吸附记账，
        // 现已证实当时真凶是手动拖动路径（已改为原生转发，见
        // performTitleBarDrag），本处理与吸附不再冲突。
        *result = DefWindowProcW(msg->hwnd, msg->message, msg->wParam, -1);
        return true;
    case WM_NCPAINT:
    case WM_NCUAHDRAWCAPTION:   // 未公开的主题消息：视觉样式开启时经此
    case WM_NCUAHDRAWFRAME:     // 重绘标题栏 / 边框（Chromium 同款处理）
        // 客户区已覆盖整个窗口，任何非客户区绘制都是幽灵残迹，直接吞掉
        *result = 0;
        return true;
    case WM_NCMOUSEMOVE:
        // 悬停标题栏空白区时系统会重绘原生按钮悬停态（幽灵来源）；
        // 吞掉不影响拖动 / Snap（由 WM_NCLBUTTONDOWN 系消息驱动）
        *result = 0;
        return true;
    case WM_NCCALCSIZE:
        return handleNcCalcSize(message, result);
    case WM_NCHITTEST:
        return handleNcHitTest(message, result);
    case WM_GETMINMAXINFO:
        return handleMinMaxInfo(message, result);
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCRBUTTONDOWN:
        // 直接交系统处理：原生标题栏拖动 / Snap 布局 / 双击最大化 / 边框缩放
        *result = DefWindowProcW(msg->hwnd, msg->message, msg->wParam, msg->lParam);
        return true;
    default:
        break;
    }
    return QWidget::nativeEvent(eventType, message, result);
}

#endif // Q_OS_WIN
