#include "AgWindow.h"
#include "AgTitleBar.h"
#include "AgWindowStyle.h"

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

#include <cmath>
#include <vector>

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#  include <windowsx.h>
#  include <shellapi.h>
#else
#  if defined(Q_OS_LINUX)
#    include <X11/Xlib.h>
#    include <X11/Xatom.h>
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

// 圆角矩形的带符号距离：>0 在矩形外（到最近边的距离），<=0 在矩形内。
qreal roundedRectSDF(qreal px, qreal py, const QRectF &rect, qreal radius)
{
    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    const qreal hx = rect.width() / 2 - radius;
    const qreal hy = rect.height() / 2 - radius;
    const qreal dx = qAbs(px - cx) - hx;
    const qreal dy = qAbs(py - cy) - hy;
    const qreal ox = qMax(dx, qreal(0));
    const qreal oy = qMax(dy, qreal(0));
    const qreal outside = std::sqrt(ox * ox + oy * oy);
    const qreal inside = qMin(qMax(dx, dy), qreal(0));
    return outside + inside - radius;
}

#ifndef Q_OS_WIN
#  if defined(Q_OS_LINUX)
// 极简 Xlib 访问层（不依赖 QtX11Extras）：复用 Qt 使用的 DISPLAY，
// 进程内缓存 Display 连接，仅供 GUI 线程调用。
namespace AgX11 {

inline bool isPlatformX11()
{
    return QGuiApplication::platformName() == QLatin1String("xcb");
}

inline Display *display()
{
    static Display *s_display = XOpenDisplay(nullptr);
    return s_display;
}

/// 是否 KWin 系窗口管理器（ukui-kwin 等）：支持 _KDE_NET_WM_SHADOW 原生阴影协议
inline bool isKWinLike()
{
    Display *d = display();
    if (!d)
        return false;
    Atom check = XInternAtom(d, "_NET_SUPPORTING_WM_CHECK", False);
    Atom type;
    int format;
    unsigned long n = 0, left = 0;
    unsigned char *data = nullptr;
    Window wmCheck = 0;
    if (XGetWindowProperty(d, XDefaultRootWindow(d), check, 0, 1, False,
                           XA_WINDOW, &type, &format, &n, &left, &data) == Success
        && data) {
        if (n == 1 && type == XA_WINDOW)
            wmCheck = *reinterpret_cast<Window *>(data);
        XFree(data);
    }
    if (!wmCheck)
        return false;
    Atom name = XInternAtom(d, "_NET_WM_NAME", False);
    Atom utf8 = XInternAtom(d, "UTF8_STRING", False);
    data = nullptr;
    bool kwinLike = false;
    if (XGetWindowProperty(d, wmCheck, name, 0, 64, False,
                           utf8, &type, &format, &n, &left, &data) == Success && data) {
        const QString wmName = QString::fromUtf8(reinterpret_cast<const char *>(data), int(n));
        kwinLike = wmName.contains(QStringLiteral("KWin"), Qt::CaseInsensitive);
        XFree(data);
    }
    return kwinLike;
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

} // namespace AgX11
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
bool agSendX11MoveResize(WId windowId, const QPoint &globalPos, int direction)
{
#  if defined(Q_OS_LINUX)
    if (!AgX11::isPlatformX11())
        return false;
    Display *display = AgX11::display();
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
int agEdgesToMoveResizeDirection(Qt::Edges edges)
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
#endif // !Q_OS_WIN

} // namespace

/// 吸附预览浮层：手动拖动光标进入边缘吸附带时铺出目标区域（半透明主题
/// 高亮），松手应用吸附并隐藏。独立 bypass 顶层窗口：不进任务栏 / Alt+Tab，
/// 也不受 WM 管理（预览自身不能被 WM 二次摆布）。无混成（ARGB 不可用）
/// 时由调用方跳过显示，回退为无预览的直接吸附。
/// 注意：显示过即会被 Qt 无障碍桥（QAccessibleCache）缓存接口，销毁必须
/// 走 deleteLater 让缓存先行解绑，直接 delete 会在应用退出时段错误。
class AgSnapPreview : public QWidget
{
public:
    AgSnapPreview()
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

AgWindow::AgWindow(QWidget *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , m_style(nullptr)
    , m_clientLayout(nullptr)
    , m_adoptedLayout(nullptr)
    , m_centralWidget(nullptr)
    , m_windowRadius(0)
    , m_closeInterception(false)
    , m_forceClosing(false)
    , m_taskbarVisible(true)
    , m_dialogMode(false)
    , m_result(0)
    , m_eventLoop(nullptr)
    , m_shadowKey(0)
    , m_shownOnce(false)
    , m_dwmCornerOk(false)
    , m_x11Compositing(false)
    , m_x11CheckedTranslucency(false)
    , m_manualResizing(false)
    , m_manualMoving(false)
    // 原生拖动 / 缩放通道默认值：Qt 5.15+ 的 QWindow::startSystemMove()
    // 已官方修复 QTBUG-69716（WM 接管后事件管线冻结），默认开启以获得
    // WM 原生贴靠；Qt 5.12 存在该 bug（5.15 前未修），默认走 Qt 手动
    // 路径 + 应用侧边缘吸附，原生消息通道仅可经 setSystemWindowMoveEnabled
    // 显式开启（已知副作用：接管后本进程 motion/enter 永久丢失）。
    , m_systemMoveEnabled(
#if defined(Q_OS_LINUX) && QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
          true
#else
          false
#endif
      )
    , m_edgeSnapEnabled(true)
    , m_snapPreview(nullptr)
    , m_snapped(false)
    , m_wmGeometryBroken(false)
    , m_wmGeometryPending(false)
    , m_kwinLike(false)
    , m_systemResizePending(false)
    , m_systemResizeSentMs(0)
    , m_pressGlobal(0, 0)
{
#ifdef Q_OS_WIN
    // Windows：保留原生窗口样式（WS_THICKFRAME / WS_CAPTION 等），
    // 在 nativeEvent 中去掉可见边框，以换取 DWM 阴影 / Snap / 原生动画。
#else
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
#endif

    m_titleBar = new AgTitleBar(this);
    m_client = new QWidget(this);
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
    AgWindowStyle::registerGlobalObserver(this);
}

AgWindow::~AgWindow()
{
    AgWindowStyle::unregisterGlobalObserver(this);
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

void AgWindow::connectTitleBar()
{
    connect(m_titleBar, &AgTitleBar::minimizeButtonClicked,
            this, &QWidget::showMinimized);
    connect(m_titleBar, &AgTitleBar::maximizeButtonClicked, this, [this]() {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
    });
    connect(m_titleBar, &AgTitleBar::closeButtonClicked, this, &QWidget::close);
    connect(m_titleBar, &AgTitleBar::titleDoubleClicked, this, [this]() {
        if (m_titleBar->isButtonVisible(AgTitleBar::ButtonMaximize)) {
            if (isMaximized())
                showNormal();
            else
                showMaximized();
        }
    });
}

// ---- 内容区 ----

QWidget *AgWindow::clientWidget() const
{
    return m_client;
}

void AgWindow::setCentralWidget(QWidget *widget)
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

QWidget *AgWindow::centralWidget() const
{
    return m_centralWidget;
}

// ---- 标题栏 ----

AgTitleBar *AgWindow::titleBar() const
{
    return m_titleBar;
}

void AgWindow::setTitleBar(AgTitleBar *bar)
{
    if (!bar || bar == m_titleBar)
        return;
    if (bar->hostWindow() != this) {
        qWarning("AgWindow::setTitleBar: 标题栏必须以本窗体构造（new MyTitleBar(this)）");
        return;
    }
    const QRect oldGeometry = m_titleBar->geometry();
    AgTitleBar *old = m_titleBar;
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

void AgWindow::addTitleBarWidget(QWidget *widget, AgTitleBar::Slot slot, bool grabMouse)
{
    m_titleBar->addWidget(widget, slot, grabMouse);
}

void AgWindow::removeTitleBarWidget(QWidget *widget)
{
    m_titleBar->removeWidget(widget);
}

void AgWindow::setTitleBarWidgetGrabMouse(QWidget *widget, bool grab)
{
    m_titleBar->setWidgetGrabMouse(widget, grab);
}

// ---- 操作按钮 ----

void AgWindow::setButtonVisible(AgTitleBar::Button button, bool visible)
{
    m_titleBar->setButtonVisible(button, visible);
}

bool AgWindow::isButtonVisible(AgTitleBar::Button button) const
{
    return m_titleBar->isButtonVisible(button);
}

// ---- 圆角 ----

void AgWindow::setWindowRadius(int radius)
{
    radius = qMax(0, radius);
    if (m_windowRadius == radius)
        return;
    m_windowRadius = radius;
    m_shadowKey = 0; // 阴影贴片圆角随窗体圆角变化
#ifdef Q_OS_WIN
    applyWindowRoundedCorners();
#else
    updateMask();
#endif
    update();
}

int AgWindow::windowRadius() const
{
    return m_windowRadius;
}

// ---- 关闭拦截 ----

void AgWindow::setCloseInterceptionEnabled(bool enable)
{
    m_closeInterception = enable;
}

bool AgWindow::isCloseInterceptionEnabled() const
{
    return m_closeInterception;
}

void AgWindow::forceClose()
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

void AgWindow::setSystemWindowMoveEnabled(bool enable)
{
    m_systemMoveEnabled = enable;
}

bool AgWindow::isSystemWindowMoveEnabled() const
{
    return m_systemMoveEnabled;
}

// ---- 任务栏 ----

void AgWindow::setTaskbarVisible(bool visible)
{
    if (m_taskbarVisible == visible)
        return;
    m_taskbarVisible = visible;
#ifdef Q_OS_WIN
    applyTaskbarVisibility();
#elif defined(Q_OS_LINUX)
    if (AgX11::isPlatformX11()) {
        // X11：Tool 窗口即跳过任务栏（_NET_WM_STATE_SKIP_TASKBAR 由 Qt 设置）
        setWindowFlag(Qt::Tool, !visible);
    } else {
        qWarning("AgWindow::setTaskbarVisible: 当前平台不支持运行中切换任务栏可见性");
    }
#else
    qWarning("AgWindow::setTaskbarVisible: 当前平台不支持");
#endif
}

bool AgWindow::isTaskbarVisible() const
{
    return m_taskbarVisible;
}

// ---- Dialog / 模态 ----

void AgWindow::setDialogMode(bool dialog)
{
    if (m_dialogMode == dialog)
        return;
    m_dialogMode = dialog;
    if (isVisible())
        qWarning("AgWindow::setDialogMode: 建议在 show() 之前调用（切换会重建原生窗口）");
    setWindowFlag(Qt::Dialog, dialog);
}

bool AgWindow::isDialogMode() const
{
    return m_dialogMode;
}

void AgWindow::setModality(Modality modality)
{
    setWindowModality(modality == Modality::ApplicationModal ? Qt::ApplicationModal
                                                             : Qt::WindowModal);
}

AgWindow::Modality AgWindow::modality() const
{
    return windowModality() == Qt::ApplicationModal ? Modality::ApplicationModal
                                                    : Modality::WindowModal;
}

int AgWindow::exec()
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

void AgWindow::done(int result)
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

void AgWindow::accept()
{
    done(Accepted);
}

void AgWindow::reject()
{
    done(Rejected);
}

int AgWindow::result() const
{
    return m_result;
}

void AgWindow::setResult(int result)
{
    m_result = result;
}

// ---- 样式 ----

void AgWindow::setStyle(AgWindowStyle *style)
{
    if (m_style == style)
        return;
    delete m_style;
    m_style = style;
    m_shadowKey = 0;
    relayout();
    update();
}

AgWindowStyle *AgWindow::style() const
{
    return m_style ? m_style : AgWindowStyle::globalStyle();
}

void AgWindow::onGlobalStyleChanged()
{
    if (m_style)
        return; // 独立样式不受全局影响
    m_shadowKey = 0;
    relayout();
    update();
}

// ---- 事件 ----

bool AgWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::WinIdChange:
#ifdef Q_OS_WIN
        if (winId()) {
            applyWindowStyles();
            applyWindowRoundedCorners();
        }
#endif
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

void AgWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_shownOnce) {
        m_shownOnce = true;
#if !defined(Q_OS_WIN) && defined(Q_OS_LINUX)
        if (AgX11::isPlatformX11()) {
            // 此时原生窗口已建立，透明是否生效以实际 visual 为准
            m_x11Compositing = AgX11::translucencyAvailable(winId());
            m_x11CheckedTranslucency = true;
            m_kwinLike = AgX11::isKWinLike();
            m_shadowKey = 0;
            // Qt 的 FramelessWindowHint 会附加 _KDE_NET_WM_WINDOW_TYPE_OVERRIDE，
            // KWin 对该类型禁用拖动预览 / 边缘吸附；覆盖为普通 NORMAL 类型
            Display *display = AgX11::display();
            if (display) {
                Atom normal = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
                XChangeProperty(display, static_cast<Window>(winId()),
                                XInternAtom(display, "_NET_WM_WINDOW_TYPE", False),
                                XA_ATOM, 32, PropModeReplace,
                                reinterpret_cast<unsigned char *>(&normal), 1);
                XFlush(display);
            }
            applyNativeShadow();
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

void AgWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
#ifdef Q_OS_WIN
    applyWindowRoundedCorners(); // Win10 窗口区域需跟随尺寸
#endif
    relayout();
}

void AgWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    switch (event->type()) {
    case QEvent::WindowStateChange: {
        const bool max = isMaximized();
        m_titleBar->updateMaximizeState(max);
        emit maximizeStateChanged(max);
#ifndef Q_OS_WIN
        relayout(); // 最大化时收起 / 恢复阴影留白
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

void AgWindow::closeEvent(QCloseEvent *event)
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

void AgWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_eventLoop) {
        reject();
        return;
    }
    QWidget::keyPressEvent(event);
}

void AgWindow::childEvent(QChildEvent *event)
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

void AgWindow::adoptLayout(QLayout *layout)
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
            qWarning("AgWindow: 内容区已存在先前的用户布局，将被替换（受管控件保留但不再受布局管理）");
        }
        delete old;
    }
    // QWidget::setLayout 会把布局连同受管控件一起转移到内容区
    m_client->setLayout(layout);
    m_adoptedLayout = layout;
}

// ---- 几何 ----

int AgWindow::effectiveMargin() const
{
#ifdef Q_OS_WIN
    return 0; // 阴影由 DWM 提供，无需留白
#else
    if (isMaximized() || isFullScreen())
        return 0;
#  if defined(Q_OS_LINUX)
    if (!AgX11::isPlatformX11())
        return style()->shadowMargin();
    if (m_kwinLike)
        return 0; // KWin 上阴影由 _KDE_NET_WM_SHADOW 原生提供，窗口即纯内容
    if (!m_x11Compositing)
        return 0; // 无混成时透明窗会黑底，退化为直角无阴影
#  endif
    return style()->shadowMargin();
#endif
}

void AgWindow::relayout()
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
    updateMask();
}

void AgWindow::ensurePolishedFrame()
{
#ifdef Q_OS_WIN
    applyWindowStyles();
    applyWindowRoundedCorners();
    applyTaskbarVisibility();
#endif
}

// ---- 绘制 ----

void AgWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    const AgWindowStyle *st = style();
    const int margin = effectiveMargin();
    const QRect content = rect().adjusted(margin, margin, -margin, -margin);

#ifndef Q_OS_WIN
    if (margin > 0)
        drawShadow(painter, content);
#endif

    const int radius = (m_windowRadius > 0 && !isMaximized() && !isFullScreen())
                           ? m_windowRadius : 0;
    if (radius > 0) {
        // 抗锯齿：混成环境下窗口为 ARGB 透明背景，圆角靠 alpha 通道平滑
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath path;
        path.addRoundedRect(QRectF(content), radius, radius);
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
void AgWindow::drawShadow(QPainter &painter, const QRect &contentRect)
{
    const AgWindowStyle *st = style();
    const int m = effectiveMargin();
    if (m <= 0 || contentRect.isEmpty())
        return;
    const int blur = qMin(st->shadowBlurRadius(), m - 1);
    const int radius = (isMaximized() || isFullScreen()) ? 0 : m_windowRadius;
    const QColor color = st->shadowColor();

    const quint32 key = (quint32(m) << 20) | (quint32(blur) << 12)
                        | (quint32(radius) << 8) | quint32(color.rgba() & 0xFF);
    if (m_shadowKey != key || m_shadowTile.isNull()) {
        // 阴影贴片：3m x 3m，中心为 m x m 内容区圆角矩形（四周各留 m 阴影带）。
        // 按圆角矩形 SDF + 高斯距离场逐像素计算 alpha：贴边处最深（color.alphaF），
        // 向外柔和衰减，观感与系统原生阴影一致（盒式模糊方案峰值过低、扩散过窄）。
        const int tile = m * 3;
        QImage img(tile, tile, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        const qreal sigma = qBound<qreal>(2.0, qreal(blur) / 2.0, qreal(m) / 1.5);
        const qreal a0 = color.alphaF();
        const QRectF contentSq(m, m, m, m);
        const qreal corner = qMin<qreal>(radius, m / 2);
        const qreal twoSigmaSq = 2 * sigma * sigma;
        for (int y = 0; y < tile; ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < tile; ++x) {
                const qreal d = roundedRectSDF(x + 0.5, y + 0.5, contentSq, corner);
                const qreal a = d <= 0 ? a0 : a0 * std::exp(-(d * d) / twoSigmaSq);
                const int ai = int(a * 255 + 0.5);
                line[x] = ai <= 0 ? 0 : qPremultiply(qRgba(color.red(), color.green(), color.blue(), ai));
            }
        }
        m_shadowTile = QPixmap::fromImage(img);
        m_shadowKey = key;
    }

    // 九宫格：四角原样；四边取远离圆角的纯渐变行/列拉伸（阴影沿边方向均匀）
    const QPixmap &t = m_shadowTile;
    painter.drawPixmap(QRect(0, 0, m, m), t, QRect(0, 0, m, m));
    painter.drawPixmap(QRect(width() - m, 0, m, m), t, QRect(2 * m, 0, m, m));
    painter.drawPixmap(QRect(0, height() - m, m, m), t, QRect(0, 2 * m, m, m));
    painter.drawPixmap(QRect(width() - m, height() - m, m, m), t, QRect(2 * m, 2 * m, m, m));
    painter.drawPixmap(QRect(m, 0, contentRect.width(), m), t, QRect(3 * m / 2, 0, 1, m));
    painter.drawPixmap(QRect(m, height() - m, contentRect.width(), m), t, QRect(3 * m / 2, 2 * m, 1, m));
    painter.drawPixmap(QRect(0, m, m, contentRect.height()), t, QRect(0, 3 * m / 2, m, 1));
    painter.drawPixmap(QRect(width() - m, m, m, contentRect.height()), t, QRect(2 * m, 3 * m / 2, m, 1));
}

void AgWindow::updateMask()
{
    const bool noMaskNeeded = m_windowRadius <= 0 || isMaximized() || isFullScreen();
#if defined(Q_OS_LINUX)
    // 混成运行时窗口为 ARGB 透明背景：圆角与阴影由 alpha 通道平滑呈现，
    // 区域裁剪（1bit 无抗锯齿）反而产生锯齿并破坏阴影，必须跳过。
    // 仅无混成（透明不可用）时才用 mask 裁出圆角。
    if (!noMaskNeeded && AgX11::isPlatformX11() && m_x11Compositing)
        return;
#endif
    if (noMaskNeeded) {
        if (!mask().isNull())
            clearMask();
        return;
    }
    const int margin = effectiveMargin();
    const QRect content = rect().adjusted(margin, margin, -margin, -margin);
    QPainterPath path;
    path.addRoundedRect(QRectF(content), m_windowRadius, m_windowRadius);
    const QRegion rounded(path.toFillPolygon().toPolygon());
    const QRegion outer(rect());
    // 内容区圆角裁剪 + 阴影环保留（阴影自身带 alpha 渐隐）
    setMask(rounded | (outer - QRegion(content)));
}
#else
void AgWindow::updateMask() {}
#endif

// ---- 非 Windows 平台：拖动 / 缩放 ----

#ifndef Q_OS_WIN

// X11：启用 KWin 原生阴影协议——由窗口管理器在窗口外绘制与原生窗口
// 完全相同的主题阴影（含 overlap 与窗体圆角对齐）。非 KWin 系 WM 上
// 回退为自绘阴影（见 drawShadow / effectiveMargin）。
void AgWindow::applyNativeShadow()
{
#if !defined(Q_OS_WIN) && defined(Q_OS_LINUX)
    if (!m_kwinLike || !AgX11::isPlatformX11())
        return;
    Display *display = AgX11::display();
    if (!display)
        return;
    const int extent = 40;
    const int overlap = qMax(m_windowRadius + 8, 16);
    long data[12] = { extent, extent, extent, extent,
                      extent, extent, extent, extent,
                      overlap, overlap, overlap, overlap };
    XChangeProperty(display, static_cast<Window>(winId()),
                    XInternAtom(display, "_KDE_NET_WM_SHADOW", False),
                    XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<unsigned char *>(data), 12);
    XFlush(display);
#endif
}

/// 边缘缩放手柄：贴窗口边缘的透明子控件，Qt 按悬停位置自动切换缩放光标，
/// 按下后发起缩放（_NET_WM_MOVERESIZE 原生路径，WM 忽略时回退手动）。
class AgEdgeGrip : public QWidget
{
public:
    AgEdgeGrip(AgWindow *window, Qt::Edges edges)
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
        if (m_window)
            m_window->releaseMouse();
    }

private:
    AgWindow *m_window;
    Qt::Edges m_edges;
};

// 拖动标题栏：优先交给窗口管理器（_NET_WM_MOVERESIZE），获得完整的原生
// 行为——拖顶最大化预览、边缘半屏、snap-assist、拖出屏幕、最大化窗口
// 还原跟随；WM 忽略消息时回退为手动拖动。
void AgWindow::performTitleBarDrag(const QPoint &pressGlobal)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    // Qt 5.15+：官方 startSystemMove（内部即 _NET_WM_MOVERESIZE，且已修复
    // 接管后事件管线冻结的 QTBUG-69716），跨 X11/Wayland。
    if (m_systemMoveEnabled && windowHandle() && windowHandle()->startSystemMove())
        return;
#else
    // Qt 5.12：手写 EWMH 消息（button=0 + 解除 Qt 连接隐式抓取），已知
    // 接管后 motion/enter 永久丢失（QTBUG-69716），仅显式开启时使用。
    if (m_systemMoveEnabled && agSendX11MoveResize(winId(), QCursor::pos(), 8))
        return;
#endif
    startManualMove(pressGlobal);
}

void AgWindow::startManualMove(const QPoint &pressGlobal)
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

void AgWindow::startManualResize(Qt::Edges edges, const QPoint &pressGlobal)
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
void AgWindow::performBorderResize(Qt::Edges edges, const QPoint &pressGlobal)
{
    m_pressGlobal = pressGlobal;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    if (m_systemMoveEnabled && windowHandle()
        && windowHandle()->startSystemResize(edges)) {
        m_systemResizePending = true;
        m_systemResizeSentMs = QDateTime::currentMSecsSinceEpoch();
        m_pendingEdges = edges;
        return;
    }
#else
    if (m_systemMoveEnabled
        && agSendX11MoveResize(winId(), QCursor::pos(), agEdgesToMoveResizeDirection(edges))) {
        m_systemResizePending = true;
        m_systemResizeSentMs = QDateTime::currentMSecsSinceEpoch();
        m_pendingEdges = edges;
        return;
    }
#endif
    startManualResize(edges, pressGlobal);
}

// ---- 边缘吸附（手动拖动路径） ----

void AgWindow::setEdgeSnapEnabled(bool enable)
{
    m_edgeSnapEnabled = enable;
#ifndef Q_OS_WIN
    if (!enable)
        hideSnapPreview();
#endif
}

bool AgWindow::isEdgeSnapEnabled() const
{
    return m_edgeSnapEnabled;
}

// 光标落在哪个吸附带：顶部（需标题栏有最大化按钮）、左 / 右屏幕边缘。
// 以光标位置判定（用户预期：鼠标靠近边缘才出现吸附）。手动拖动时窗口
// 会被屏幕边缘钳制，但光标会继续越过钳制位置走到边缘带，因此抓取点
// 在标题栏内的偏移不影响触发。
AgWindow::SnapEdge AgWindow::edgeSnapTarget(const QPoint &globalPos) const
{
    if (!m_edgeSnapEnabled)
        return SnapEdge::None;
    QWindow *wh = windowHandle();
    const QRect avail = wh ? wh->screen()->availableGeometry() : QRect();
    if (!avail.isValid())
        return SnapEdge::None;
    const int zone = 10;
    if (m_titleBar && m_titleBar->isButtonVisible(AgTitleBar::ButtonMaximize)
        && globalPos.y() - avail.top() <= zone)
        return SnapEdge::Top;
    if (globalPos.x() - avail.left() <= zone)
        return SnapEdge::Left;
    if (avail.right() - globalPos.x() <= zone)
        return SnapEdge::Right;
    return SnapEdge::None;
}

// 吸附目标区域：顶部为整个可用区，左右各半屏。
QRect AgWindow::edgeSnapRect(SnapEdge edge) const
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

void AgWindow::showSnapPreview(SnapEdge edge)
{
#if defined(Q_OS_LINUX)
    // 无混成时窗口无 alpha 通道，铺屏预览会是不透明色块，跳过（直接吸附）
    if (AgX11::isPlatformX11() && !m_x11Compositing)
        return;
#endif
    if (!m_snapPreview)
        m_snapPreview = new AgSnapPreview();
    m_snapPreview->showFor(edgeSnapRect(edge));
}

void AgWindow::hideSnapPreview()
{
    if (m_snapPreview && m_snapPreview->isVisible())
        m_snapPreview->hide();
}

// 松手应用吸附：顶部最大化；左右半屏按阴影环补偿外扩（可见内容区铺满半屏）。
// 记录吸附前的浮动几何，供下次拖动标题栏时还原跟随。
void AgWindow::applyEdgeSnap(SnapEdge edge)
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
bool AgWindow::sendNetMoveResizeWindow(const QPoint &pos)
{
#if !defined(Q_OS_WIN) && defined(Q_OS_LINUX)
    if (!AgX11::isPlatformX11())
        return false;
    Display *display = AgX11::display();
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

void AgWindow::applyPositionViaWM(const QPoint &pos)
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

void AgWindow::manualResizeStep(const QPoint &globalPos)
{
    const QPoint delta = globalPos - m_manualStartGlobal;
    const QSize minSize = minimumSize().expandedTo(QSize(1, 1));
    QRect geo(m_manualStartPos, m_manualStartSize);

    if (m_manualEdges.testFlag(Qt::LeftEdge)) {
        int left = m_manualStartPos.x() + delta.x();
        const int minLeft = geo.right() + 1 - minSize.width();
        left = qMin(left, minLeft);
        geo.setLeft(left);
    }
    if (m_manualEdges.testFlag(Qt::RightEdge)) {
        int right = m_manualStartPos.x() + m_manualStartSize.width() - 1 + delta.x();
        const int minRight = geo.left() + minSize.width() - 1;
        right = qMax(right, minRight);
        geo.setRight(right);
    }
    if (m_manualEdges.testFlag(Qt::TopEdge)) {
        int top = m_manualStartPos.y() + delta.y();
        const int minTop = geo.bottom() + 1 - minSize.height();
        top = qMin(top, minTop);
        geo.setTop(top);
    }
    if (m_manualEdges.testFlag(Qt::BottomEdge)) {
        int bottom = m_manualStartPos.y() + m_manualStartSize.height() - 1 + delta.y();
        const int minBottom = geo.top() + minSize.height() - 1;
        bottom = qMax(bottom, minBottom);
        geo.setBottom(bottom);
    }
    setGeometry(geo);
}

// X11：在窗口边缘布置透明缩放手柄。阴影环（margin>0）或窗口边缘（margin=0）
// 即手柄区域，光标由 Qt 依据手柄的 cursor 自动切换。
void AgWindow::setupEdgeGrips()
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
        AgEdgeGrip *grip = new AgEdgeGrip(this, def.edges);
        grip->setObjectName(QStringLiteral("agwindow_edge_grip"));
        m_edgeGrips.append(grip);
    }
    relayoutEdgeGrips();
}

void AgWindow::relayoutEdgeGrips()
{
    if (m_edgeGrips.isEmpty())
        return;
    const bool maximized = isMaximized() || isFullScreen();
    const int depth = qMax(effectiveMargin(), 6);   // 手柄厚度：阴影环宽或 6px
    const int w = width(), h = height();
    auto place = [&](int index, const QRect &geo) {
        QWidget *grip = m_edgeGrips.at(index);
        grip->setGeometry(geo);
        grip->setVisible(!maximized && !geo.isEmpty());
        grip->raise();
    };
    // 0=N 1=S 2=W 3=E 4=NW 5=NE 6=SW 7=SE
    place(0, QRect(0, 0, w, depth));
    place(1, QRect(0, h - depth, w, depth));
    place(2, QRect(0, depth, depth, qMax(0, h - 2 * depth)));
    place(3, QRect(w - depth, depth, depth, qMax(0, h - 2 * depth)));
    place(4, QRect(0, 0, depth, depth));
    place(5, QRect(w - depth, 0, depth, depth));
    place(6, QRect(0, h - depth, depth, depth));
    place(7, QRect(w - depth, h - depth, depth, depth));
}

void AgWindow::mouseMoveEvent(QMouseEvent *event)
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
        // 的指针抓取相撞（同 AgTitleBar 拖动回退）。
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

void AgWindow::mouseReleaseEvent(QMouseEvent *event)
{
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
#endif // !Q_OS_WIN

#ifdef Q_OS_WIN

// ---- Windows：原生无边框实现 ----

void AgWindow::applyWindowStyles()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const LONG_PTR wanted = WS_THICKFRAME | WS_CAPTION | WS_SYSMENU
                             | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    if ((style & wanted) == wanted)
        return;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style | wanted);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                     | SWP_NOACTIVATE);
}

void AgWindow::applyWindowRoundedCorners()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;
    const int pref = m_windowRadius <= 0 ? 1        // DWMWCP_DONOTROUND
                     : m_windowRadius <= 5 ? 3      // DWMWCP_ROUNDSMALL（4px）
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
        m_dwmCornerOk = true;
        SetWindowRgn(hwnd, nullptr, FALSE);
        return; // Win11+：系统圆角 + 配套 DWM 阴影
    }
    // Win10 及以下：窗口区域实现圆角（阴影随区域收缩）
    if (m_windowRadius > 0 && !IsZoomed(hwnd)) {
        HRGN rgn = CreateRoundRectRgn(0, 0, width() + 1, height() + 1,
                                      m_windowRadius * 2, m_windowRadius * 2);
        SetWindowRgn(hwnd, rgn, TRUE);
    } else {
        SetWindowRgn(hwnd, nullptr, TRUE);
    }
}

void AgWindow::applyTaskbarVisibility()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    const LONG_PTR wanted = m_taskbarVisible ? 0 : WS_EX_TOOLWINDOW;
    if (((exStyle & WS_EX_TOOLWINDOW) != 0) == !m_taskbarVisible)
        return;
    if (m_taskbarVisible)
        exStyle &= ~WS_EX_TOOLWINDOW;
    else
        exStyle |= WS_EX_TOOLWINDOW;
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                     | SWP_NOACTIVATE);
}

bool AgWindow::handleNcCalcSize(void *message, long *result)
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

bool AgWindow::handleNcHitTest(void *message, long *result)
{
    MSG *msg = static_cast<MSG *>(message);
    const HWND hwnd = msg->hwnd;
    const QPoint global(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
    const QPoint local = mapFromGlobal(global);

    // 边框缩放热区（最大化 / 全屏时禁用）
    if (!IsZoomed(hwnd) && !isFullScreen()) {
        const int frame = GetSystemMetrics(SM_CXSIZEFRAME)
                          + GetSystemMetrics(SM_CXPADDEDBORDER);
        const bool left = local.x() < frame;
        const bool right = local.x() >= width() - frame;
        const bool top = local.y() < frame;
        const bool bottom = local.y() >= height() - frame;
        LRESULT hit = HTCLIENT;
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
        if (hit != HTCLIENT) {
            *result = static_cast<long>(hit);
            return true;
        }
    }

    // 标题栏空白区交给系统作为标题栏：原生拖动 / 边缘分屏（Aero Snap）/ 双击最大化
    if (m_titleBar && m_titleBar->isVisible()) {
        const QPoint tbPos = m_titleBar->mapFrom(this, local);
        if (m_titleBar->rect().contains(tbPos) && m_titleBar->hitTestCaption(tbPos)) {
            *result = HTCAPTION;
            return true;
        }
    }

    *result = HTCLIENT;
    return true;
}

bool AgWindow::handleMinMaxInfo(void *message, long *result)
{
    MSG *msg = static_cast<MSG *>(message);
    MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO *>(msg->lParam);
    // 原生边框缩放遵守窗体最小宽高
    mmi->ptMinTrackSize.x = qMax<LONG>(minimumWidth(), mmi->ptMinTrackSize.x);
    mmi->ptMinTrackSize.y = qMax<LONG>(minimumHeight(), mmi->ptMinTrackSize.y);
    *result = 0;
    return true;
}

bool AgWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    if (eventType != QByteArrayLiteral("windows_generic_MSG"))
        return QWidget::nativeEvent(eventType, message, result);
    MSG *msg = static_cast<MSG *>(message);
    switch (msg->message) {
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
