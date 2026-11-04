#include "AuWindow.h"
#include "AuBorderShadow.h"
#include "AuWindow_p.h"

#include <QCursor>
#include <QDateTime>
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
#include <cstdlib>
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

/// 窗口管理器是否支持 KDE 阴影协议（_KDE_NET_WM_SHADOW）：KWin / ukui-kwin
/// 在根窗口的 _NET_SUPPORTED 中通告该原子。结果按连接缓存（WM 不中途更换
/// 协议支持）。
inline bool wmSupportsKdeShadow()
{
    static bool s_supported = []() {
        Display *d = display();
        if (!d)
            return false;
        Atom shadow = XInternAtom(d, "_KDE_NET_WM_SHADOW", True);
        if (shadow == None)
            return false;
        Atom supported = XInternAtom(d, "_NET_SUPPORTED", True);
        if (supported == None)
            return false;
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long nitems = 0, bytesAfter = 0;
        unsigned char *data = nullptr;
        if (XGetWindowProperty(d, XDefaultRootWindow(d), supported, 0, 65536, False,
                               XA_ATOM, &actualType, &actualFormat,
                               &nitems, &bytesAfter, &data) != Success
            || actualType != XA_ATOM || !data) {
            return false;
        }
        const Atom *atoms = reinterpret_cast<const Atom *>(data);
        bool found = false;
        for (unsigned long i = 0; i < nitems && !found; ++i)
            found = (atoms[i] == shadow);
        XFree(data);
        return found;
    }();
    return s_supported;
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

// ============================================================================
// KDE 阴影协议通道（_KDE_NET_WM_SHADOW）：ukui-kwin / KWin 系窗口管理器上
// 无边框窗口阴影的标准来源——客户端提供 8 块阴影贴图（4 角 + 4 条 1px 边条，
// 九宫格切法与 peony / qt5-ukui-platformtheme 的 ConfigShadowHelper 一致），
// 写入窗口的 _KDE_NET_WM_SHADOW 属性后由 WM 在窗体矩形之外合成。窗体几何
// 不变、缩放零重绘开销，规避了 peony 主窗口因 QGraphicsEffect 随 resize
// 反复全图模糊造成的撕裂（peony 源码 main-window.cpp:1702 注释的问题）。
// 贴图渲染与客户端阴影环共用 AuWindowDetail::auRenderBorderShadowTile；
// 对齐推导（全部按设备像素）：属性边距 = 渐隐环 b，角贴图边长 = b + 圆角
// 半径 r，KWin 将角贴图摆在窗体 ± b 的 outerRect 四角 → 贴图内 (b, b) 处
// 的窗口矩形弧线恰好落在窗角，r 半径的挖空区由 ARGB 窗口的透明角像素
// 透出阴影。最大化 / 全屏 WM 自动停用阴影（KWin wantsShadowToBeRendered），
// 客户端无需处理。状态以 QObject 子对象挂在窗体上，随窗体析构自动释放。
// 注：圆角半径 > 模糊环宽时角贴图弧外阴影略有裁剪（ukui 同样如此），
// 样式默认值（blur 24~28 ≥ 常用半径）不受影响。
// ============================================================================
class AuX11KdeShadow : public QObject
{
public:
    explicit AuX11KdeShadow(AuWindow *window)
        : QObject(window), m_window(window)
    {
        setObjectName(QStringLiteral("auwindow_kde_shadow"));
    }

    ~AuX11KdeShadow() override { freeTiles(); }

    /// tile：设备像素九宫格位图（AuWindow::ensureShadowTile 缓存产物）。
    /// blur / radii：贴图的逻辑键值（与渲染参数一致），dpr：设备像素比。
    void apply(const QImage &tile, int blur, const int radii[4], qreal dpr)
    {
        Display *d = AuX11::display();
        if (!d || tile.isNull() || tile.format() != QImage::Format_ARGB32_Premultiplied)
            return;
        const Window win = static_cast<Window>(m_window->winId());
        if (!win)
            return;
        if (m_win != win) {          // 原生窗口重建：旧贴图随旧窗口销毁
            freeTiles();
            m_win = win;
        }

        const int S = tile.width();
        const int b = qRound(blur * dpr);              // 渐隐环（设备像素）
        const int r[4] = {
            qRound(radii[0] * dpr), qRound(radii[1] * dpr),
            qRound(radii[2] * dpr), qRound(radii[3] * dpr)
        };
        const int R = qMax(qMax(r[0], r[1]), qMax(r[2], r[3]));
        const int corner[4] = { b + r[0], b + r[1], b + r[2], b + r[3] };
        const int center = b + R;                      // 内窗矩形中心条位置

        // 九宫格切图（顺序即属性中的贴图顺序：上 / 右上 / 右 / 右下 /
        // 下 / 左下 / 左 / 左上），角贴图原尺寸摆放、1px 边条由 WM 拉伸
        QImage top      = tile.copy(center, 0, 1, b + R);
        QImage topRight = tile.copy(S - corner[1], 0, corner[1], corner[1]);
        QImage right    = tile.copy(S - corner[1], center, corner[1], 1);
        QImage bottomRight = tile.copy(S - corner[3], S - corner[3], corner[3], corner[3]);
        QImage bottom   = tile.copy(center, S - (b + R), 1, b + R);
        QImage bottomLeft  = tile.copy(0, S - corner[2], corner[2], corner[2]);
        QImage left     = tile.copy(0, center, corner[2], 1);
        QImage topLeft  = tile.copy(0, 0, corner[0], corner[0]);

        Pixmap newTiles[8] = { None, None, None, None, None, None, None, None };
        const QImage *imgs[8] = { &top, &topRight, &right, &bottomRight,
                                  &bottom, &bottomLeft, &left, &topLeft };
        const ::Window root = XDefaultRootWindow(d);
        bool ok = true;
        for (int i = 0; i < 8 && ok; ++i) {
            const QImage &img = *imgs[i];
            if (img.isNull() || img.format() != QImage::Format_ARGB32_Premultiplied) {
                ok = false;
                break;
            }
            newTiles[i] = uploadPixmap(d, root, img);
            if (newTiles[i] == None)
                ok = false;
        }
        if (!ok) {
            for (Pixmap px : newTiles)
                if (px != None)
                    XFreePixmap(d, px);
            return;
        }
        freeTiles();
        for (int i = 0; i < 8; ++i)
            m_tiles[i] = newTiles[i];

        // 属性：8 个贴图 XID + 4 个边距（上 / 右 / 下 / 左，设备像素），
        // 类型 XA_CARDINAL / 格式 32 —— 与 KWin、kwindowsystem 的
        // KWindowShadow X11 后端逐字段一致
        CARD32 data[12] = {
            CARD32(m_tiles[0]), CARD32(m_tiles[1]), CARD32(m_tiles[2]), CARD32(m_tiles[3]),
            CARD32(m_tiles[4]), CARD32(m_tiles[5]), CARD32(m_tiles[6]), CARD32(m_tiles[7]),
            CARD32(b), CARD32(b), CARD32(b), CARD32(b)
        };
        const Atom atom = XInternAtom(d, "_KDE_NET_WM_SHADOW", False);
        XChangeProperty(d, m_win, atom, XA_CARDINAL, 32, PropModeReplace,
                        reinterpret_cast<unsigned char *>(data), 12);
        XFlush(d);
        m_applied = true;
    }

    void clear()
    {
        if (m_applied && m_win) {
            if (Display *d = AuX11::display()) {
                XDeleteProperty(d, m_win,
                                XInternAtom(d, "_KDE_NET_WM_SHADOW", False));
                XFlush(d);
            }
        }
        m_applied = false;
        freeTiles();
    }

private:
    /// 创建 32 位深 ARGB 贴图并上传位图。XDestroyImage 会 free 图像数据，
    /// 因此位图先拷入 malloc 缓冲（析构时由 Xlib 释放），避免悬空指针。
    static Pixmap uploadPixmap(Display *d, ::Window root, const QImage &img)
    {
        const int w = img.width(), h = img.height();
        if (w <= 0 || h <= 0 || w > 4096 || h > 4096)
            return None;
        char *buf = static_cast<char *>(malloc(size_t(w) * 4 * size_t(h)));
        if (!buf)
            return None;
        memcpy(buf, img.constBits(), size_t(img.bytesPerLine()) * size_t(h));
        XImage *ximg = XCreateImage(d, nullptr, 32, ZPixmap, 0,
                                    buf, w, h, 32, w * 4);
        if (!ximg) {
            free(buf);
            return None;
        }
        const Pixmap px = XCreatePixmap(d, root, w, h, 32);
        if (!px) {
            XDestroyImage(ximg);
            return None;
        }
        GC gc = XCreateGC(d, px, 0, nullptr);
        XPutImage(d, px, gc, ximg, 0, 0, 0, 0, w, h);
        XFreeGC(d, gc);
        XDestroyImage(ximg);      // 释放 malloc 缓冲
        return px;
    }

    void freeTiles()
    {
        if (Display *d = AuX11::display()) {
            for (Pixmap &px : m_tiles) {
                if (px != None)
                    XFreePixmap(d, px);
                px = None;
            }
        } else {
            for (Pixmap &px : m_tiles)
                px = None;
        }
    }

    AuWindow *m_window;
    ::Window m_win = None;
    Pixmap m_tiles[8] = { None, None, None, None, None, None, None, None };
    bool m_applied = false;
};

// ---- 平台钩子 ----

void AuWindow::platformInit()
{
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    m_client = new QWidget(this);
    setupEdgeGrips();
    // Linux 默认开启边框阴影（ukui-kwin / KWin 走 WM 合成，其它混成 WM
    // 走客户端阴影环；无混成时经 effectiveMargin 自动收起）
    m_borderShadowEnabled = true;
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

void AuWindow::platformWinIdChanged()
{
    // 原生窗口重建（Dialog 模式切换等）：旧窗口的阴影属性随窗口销毁，
    // 新窗口需重新上传贴图并写属性（首显前 m_x11Compositing 未检测，
    // 此处先清空，platformFirstShow 再按能力应用）
    platformShadowUpdate();
}

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
    // 混成能力已定：应用 / 清除 KDE 阴影协议贴图
    platformShadowUpdate();
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

// ---- 边框阴影平台通道 ----

// ukui-kwin / KWin 支持 KDE 阴影协议且混成运行时，阴影由 WM 在窗体矩形外
// 合成（KWin wantsShadowToBeRendered 在最大化 / 全屏时自动停用），窗体不
// 留阴影环；其余混成 WM（mutter / xfwm 等）回退为客户端自绘阴影环。
bool AuWindow::platformShadowPaintedExternally() const
{
    return AuX11::isPlatformX11() && m_x11Compositing
           && AuX11::wmSupportsKdeShadow();
}

void AuWindow::platformShadowUpdate()
{
    if (!AuX11::isPlatformX11())
        return;   // Wayland 等无 KDE 阴影协议：客户端阴影环经公共代码生效
    AuX11KdeShadow *state = findChild<AuX11KdeShadow *>();
    if (!m_borderShadowEnabled || !m_x11Compositing
        || !AuX11::wmSupportsKdeShadow()) {
        if (state)
            state->clear();
        return;
    }
    // 与客户端绘制共用同一贴图缓存（键：样式阴影参数 / 生效四角半径 / dpr）
    ensureShadowTile();
    if (m_shadowTile.isNull()) {
        if (state)
            state->clear();
        return;
    }
    if (!state)
        state = new AuX11KdeShadow(this);
    state->apply(m_shadowTile, m_shadowTileBlur, m_shadowTileRadii, m_shadowTileDpr);
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
