#include "AuWindow.h"
#include "AuTitleBar.h"
#include "AuWindowStyle.h"
#include "AuWindow_p.h"

#include <QDateTime>
#include <QCursor>
#include <QEventLoop>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

// ============================================================================
// AuWindow 跨平台实现。平台相关的原生代码（Win32 / DWM、Xlib / EWMH）分别
// 位于 AuWindowWin.cpp 与 AuWindowX11.cpp，其它平台见 AuWindowFallback.cpp；
// 本文件不含任何平台条件编译，平台差异经 platformXxx() 钩子进入各平台文件。
// ============================================================================

/// 本进程 AuWindow 注册表：窗口间吸附在非 X11 平台（无法枚举系统窗口）的
/// 候选来源。单线程 UI 使用，无需加锁。
static QVector<AuWindow *> &auWindowRegistry()
{
    static QVector<AuWindow *> registry;
    return registry;
}

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
        // X11 专属属性（不接收焦点），其它平台忽略
        setAttribute(Qt::WA_X11DoNotAcceptFocus);
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

AuWindow::AuWindow(QWidget *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , m_style(nullptr)
    // m_titleBar / m_client 必须在初始化列表置空：platformInit() 内的
    // winId() 会同步创建原生窗口并派发 WindowIconChange / WinIdChange /
    // Resize 事件，event() 与 relayout() 在此期间解引用这两个指针——构造
    // 函数体里才赋值的话，读到的是栈残留垃圾（栈上构造的窗体必现崩溃
    // 风险，是否触发取决于栈残留，core 已实证：this 落在栈区）
    , m_titleBar(nullptr)
    , m_client(nullptr)
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
    , m_borderShadowEnabled(false)   // platformInit() 按平台能力设默认值
    , m_manualResizing(false)
    , m_manualMoving(false)
    , m_manualRestorePending(false)
    , m_manualRestoreTargetRect()
    , m_systemMoveEnabled(false)   // 系统通道在 platformInit() 中按平台能力开启
    , m_edgeSnapEnabled(true)
    , m_windowSnapEnabled(true)    // platformInit() 按平台/Qt 版本设默认值
    , m_snapPreview(nullptr)
    , m_snapped(false)
    , m_nativeDragActive(false)
    , m_inNativeMoveLoop(false)
    , m_nativeDragGen(0)
    , m_normalRestoreAssert()
    , m_x11BorderDecorActive(false)
    , m_wmGeometryBroken(false)
    , m_wmGeometryPending(false)
    , m_systemResizePending(false)
    , m_systemResizeSentMs(0)
    , m_pressGlobal(0, 0)
    , m_normalSizeHint(0, 0)
    , m_normalGeometry()
    , m_cornerClips{nullptr, nullptr, nullptr, nullptr}
    , m_grabRecoveryTimer(nullptr)
    , m_awaitingRelease(false)
    , m_creationGeometry()
{
    // 平台差异（窗口标志 / 属性、内容区容器、边缘缩放手柄、系统移动通道）
    platformInit();

    // 进入窗口间吸附的候选注册表（非 X11 平台的吸附目标来源）
    auWindowRegistry().append(this);

    m_titleBar = new AuTitleBar(this);
    m_client->setFocusPolicy(Qt::NoFocus);
    m_clientLayout = new QVBoxLayout(m_client);
    m_clientLayout->setContentsMargins(0, 0, 0, 0);
    m_clientLayout->setSpacing(0);

    connectTitleBar();
    m_titleBar->setTitle(windowTitle());
    m_titleBar->setIcon(windowIcon());

    AuWindowStyle::registerGlobalObserver(this);
}

AuWindow::~AuWindow()
{
    auWindowRegistry().removeOne(this);
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
    platformButtonVisibilityChanged();
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
    platformRadiusChanged();
    relayout(); // 四角裁剪浮层 / 标题栏圆弧跟随
    update();
}

void AuWindow::setCornerRadius(Qt::Corner corner, int radius)
{
    radius = qMax(0, radius);
    if (m_cornerRadii[int(corner)] == radius)
        return;
    m_cornerRadii[int(corner)] = radius;
    platformRadiusChanged();
    relayout();
    update();
}

int AuWindow::cornerRadius(Qt::Corner corner) const
{
    return platformEffectiveRadius(m_cornerRadii[int(corner)]);
}

int AuWindow::windowRadius() const
{
    return platformEffectiveRadius(qMax(qMax(m_cornerRadii[0], m_cornerRadii[1]),
                                        qMax(m_cornerRadii[2], m_cornerRadii[3])));
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

// ---- 系统移动通道（拖动 / 缩放是否交给 WM / 系统原生完成） ----

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
    if (!platformSetTaskbarVisible(visible))
        qWarning("AuWindow::setTaskbarVisible: 当前平台不支持运行中切换任务栏可见性");
}

bool AuWindow::isTaskbarVisible() const
{
    return m_taskbarVisible;
}

// ---- 边框阴影 ----

void AuWindow::setBorderShadowEnabled(bool enabled)
{
    if (m_borderShadowEnabled == enabled)
        return;
    m_borderShadowEnabled = enabled;
    platformShadowUpdate();
}

bool AuWindow::isBorderShadowEnabled() const
{
    return m_borderShadowEnabled;
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

    // 循环不能以 this 为父对象：exec 期间窗口可能被销毁（如开启关闭拦截
    // + WA_DeleteOnClose 时点关闭按钮），QObject 析构会连带 delete 这个
    // 栈上对象。与 QDialog::exec 相同：无父对象循环 + QPointer 守卫；
    // 窗口析构会经 m_eventLoop->exit() 退出循环（见 ~AuWindow）。
    QPointer<AuWindow> guard(this);
    QEventLoop loop;
    m_eventLoop = &loop;
    const int code = loop.exec();
    if (!guard)
        return code; // 窗口已在循环内销毁，不能再访问成员
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
    // 全局样式指针归 setGlobalStyle 管理（仅在设置新全局样式时释放旧的）：
    // setStyle 传入它时直接跳过——不接管所有权、不改变当前样式状态；
    // 恢复跟随全局请显式传 nullptr
    if (style && style == AuWindowStyle::globalStyle())
        return;
    if (m_style == style)
        return;
    delete m_style;
    m_style = style;
    relayout();
    platformStyleChanged();
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
    platformStyleChanged();
    update();
}

// ---- 事件 ----

bool AuWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::WinIdChange:
        platformWinIdChanged();
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

// ---- 显示与默认摆放 ----

// 首次显示的默认摆放：居中于可见父窗口（无父窗口 / 父窗口隐藏时居中于
// 光标所在屏幕的工作区）。在 setVisible(true) 中、原生窗口创建之前调用：
// 居中结果经 move() 写入 widget 几何，创建原生窗口时即为显式位置——系统
// 默认摆放（无边框弹出窗口的左上角落在屏幕 / 父窗口中心）不再生效，也
// 不存在"先按默认位置显示、再跳到居中位置"的闪动。用户已显式定位
//（调过 move()/setGeometry()，WA_Moved，与 QDialog::adjustPosition 同一
// 判定）或指定最大化 / 最小化 / 全屏状态时不干预。
void AuWindow::adjustDefaultPosition()
{
    if (testAttribute(Qt::WA_Moved))
        return;
    if (isMaximized() || isMinimized() || isFullScreen())
        return;

    QWidget *parent = parentWidget();
    QScreen *screen = nullptr;
    QPoint center;
    if (parent)
        parent = parent->window();
    if (parent && parent->isVisible()) {
        screen = QGuiApplication::screenAt(parent->mapToGlobal(parent->rect().center()));
        center = parent->geometry().center();
    }
    if (!screen) {
        screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        if (!screen)
            return;
        center = screen->availableGeometry().center();
    }

    const QRect avail = screen->availableGeometry();
    QPoint pos(center.x() - width() / 2, center.y() - height() / 2);
    // 超出工作区的部分钳回屏幕内（窗口大于工作区时钳到左上角）
    pos.setX(qBound(avail.left(), pos.x(), qMax(avail.left(), avail.right() - width() + 1)));
    pos.setY(qBound(avail.top(), pos.y(), qMax(avail.top(), avail.bottom() - height() + 1)));
    move(pos);
}

void AuWindow::setVisible(bool visible)
{
    if (visible && !isVisible()) {
        adjustDefaultPosition();
        // 原生窗口创建前的期望几何：Qt 创建窗口时按通用样式推算边框放大
        // 原生矩形，widget 会被二次放大（见 AuWindowWin 的首显校正）
        m_creationGeometry = geometry();
    }
    QWidget::setVisible(visible);
}

void AuWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_shownOnce) {
        m_shownOnce = true;
        platformFirstShow();
        // 布局劫持兜底
        if (layout() && layout() != m_adoptedLayout)
            adoptLayout(layout());
    }
    relayout();
}

void AuWindow::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    // 还原纠正断言保护窗：KWin 取消最大化的两步落位（步骤 b：首个 motion
    // 把窗口移到它记忆的位置）会在我们纠正后几十毫秒到达并拽回过期记忆
    // 位置——窗内收到不匹配的 Move 重新断言（自身 setGeometry 的回声几何
    // 相等，不会递归；用户按下时 AuTitleBar/startManualMove 等已撤防）
    if (m_normalRestoreAssert.isValid() && geometry() != m_normalRestoreAssert)
        setGeometry(m_normalRestoreAssert);
    // 最大化拖拽还原（手动通道）：WM 取消最大化落位后首个 Move 到达时
    // 收养为跟随基准（见 adoptManualRestorePosition；未落地时它自行拒绝）
    if (m_manualRestorePending && !isMaximized())
        adoptManualRestorePosition();
    // 记录非最大化浮动几何（与 resizeEvent 的记录配套，含位置）：最大化
    // 还原的落位由 WM 的记忆几何决定，实测 KWin 会把按钮还原位置丢到
    // (0,0)，changeEvent 的 max→normal 分支据此纠正。
    // 还原过渡期内跳过：WM 落地帧的几何可能是其过期记忆，若收入基准会
    // 把下一次还原也拽到错误位置（过渡期由 adoptManualRestorePosition
    // 的目标矩形接管，不需要本记录）。
    // 原生拖动进行中跳过：拖动途中的位置是过渡态，基准由
    // scheduleNativeDragSettle 在收场时按终态决定
    if (!isMaximized() && !isFullScreen() && !m_snapped
        && !m_manualRestorePending && !m_nativeDragActive)
        m_normalGeometry = geometry();
}

void AuWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // 记录非最大化浮动尺寸与几何：供最大化还原（系统通道 / 按钮均经 WM
    // 记忆几何）后纠正 KWin 的过期记忆尺寸与位置。
    // 还原过渡期 / 原生拖动进行中跳过（同 moveEvent）
    if (!isMaximized() && !isFullScreen() && !m_snapped
        && !m_manualRestorePending && !m_nativeDragActive) {
        m_normalSizeHint = size();
        m_normalGeometry = geometry();
    }
    platformResized();
    relayout();
}

void AuWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    switch (event->type()) {
    case QEvent::WindowStateChange: {
        const bool max = isMaximized();
        m_titleBar->updateMaximizeState(max);
        // 仅处理"从最大化还原到正常态"：WindowStateChange 在最小化 / 全屏
        // 切换时同样到达（那些状态下 isMaximized() 同为 false），不判定的话
        // 最大化态进入全屏的瞬间会被 resize / setGeometry 回正常几何，打断
        // 全屏；最小化也不动几何（还原交给 un-minimize 时 WM 的记忆几何）
        const bool wasMax = static_cast<QWindowStateChangeEvent *>(event)->oldState()
                            & Qt::WindowMaximized;
        if (wasMax && !max && !isMinimized() && !isFullScreen()) {
            if (m_snapped && m_preSnapGeometry.isValid()) {
                // 吸附态（拖顶最大化 / 贴边半屏）经还原按钮 / 双击还原：
                // 位置应回到吸附前的浮动几何（= 进入最大化前按下处的原始
                // 位置），而非 WM 记忆的（拖动末位置）还原几何
                m_snapped = false;
                m_normalSizeHint = m_preSnapGeometry.size();
                setGeometry(m_preSnapGeometry);
                armNormalRestoreAssert(m_preSnapGeometry);
            } else if (m_normalGeometry.isValid() && geometry() != m_normalGeometry
                       && !m_manualMoving && !m_snapRestoreTarget.isValid()
                       && !m_nativeDragActive && !m_inNativeMoveLoop) {
                // 最大化还原的落位与尺寸均由 WM 的记忆决定，记忆可能过期
                // （KWin 按 WM_CLASS 记忆历史几何，实测按钮还原位置被丢到
                // (0,0)），纠正回进入最大化前的浮动几何。仅按钮 / 双击还原
                // 路径应用——拖拽还原的位置由下方 m_snapRestoreTarget 锚定
                // （纠正位置会把窗口拉回原位打断拖拽）。
                // Windows：系统原生移动循环（拖拽还原）进行中不得纠正——
                // setGeometry 会与移动循环逐帧互搏，且布防的断言保护窗会在
                // 松手贴靠最大化瞬间把窗口拖回最大化前位置
                setGeometry(m_normalGeometry);
                armNormalRestoreAssert(m_normalGeometry);
            } else if (m_normalSizeHint.isValid() && size() != m_normalSizeHint
                       && !m_snapRestoreTarget.isValid() && !m_manualRestorePending
                       && !m_inNativeMoveLoop) {
                // m_normalGeometry 尚未记录（首显前即最大化等极端时序）时的
                // 尺寸兜底
                resize(m_normalSizeHint);
            }
            if (!m_manualMoving && !m_manualRestorePending && !m_nativeDragActive
                && !m_inNativeMoveLoop
                && m_snapRestoreTarget.isValid()
                && geometry() != m_snapRestoreTarget) {
                // 拖动还原（最大化态）的光标锚定落位：状态翻转后窗口已脱离
                // 最大化态，此刻的 ConfigureRequest 必被 WM 受理，锚定位置
                // 单帧到位。⭐ 此前两次闪帧回归的教训：showNormal 后立即
                // setGeometry / 状态翻转时 resize——请求落在尚未脱离最大化
                // 态的窗口上（仍在最大化位置如 (0,0)），把"还原尺寸的小
                // 窗口"闪现在屏幕角。锚定必须也只能在这里发。本目标随本
                // 状态变化清空，一次性。
                setGeometry(m_snapRestoreTarget);
                armNormalRestoreAssert(m_snapRestoreTarget);
            }
        }
        // 锚点保护在任何状态变化时作废（含最大化 / 最小化 / 全屏切换），
        // 防止过期目标在后续无关还原中被套用
        m_snapRestoreTarget = QRect();
        emit maximizeStateChanged(max);
        relayout(); // 最大化时收起 / 恢复阴影留白（Windows 另由平台钩子清除残影）
        platformStateChanged();
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
    if (m_eventLoop) {
        if (event->key() == Qt::Key_Escape) {
            reject();
            return;
        }
        // QDialog 的默认按钮语义：回车优先触发当前焦点的 QPushButton（QDialog
        // 内按钮均为 autoDefault，回车总是触发焦点按钮），焦点不在按钮上时触发
        // setDefault(true) 的默认按钮。被焦点控件吃掉的回车（多行文本换行、
        // 表格进入编辑等）不会传播到这里，与 QDialog 的按键传播行为一致。
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            if (auto *focused = qobject_cast<QPushButton *>(focusWidget())) {
                if (focused->isEnabled()) {
                    focused->click();
                    return;
                }
            }
            const auto buttons = findChildren<QPushButton *>();
            for (QPushButton *btn : buttons) {
                if (btn->isDefault() && btn->isVisible() && btn->isEnabled()) {
                    btn->click();
                    return;
                }
            }
        }
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
    // 阴影仅经 WM 在窗体矩形外合成（X11 _KDE_NET_WM_SHADOW），窗体几何
    // 即内容区，客户端自绘阴影环（窗体矩形含留白环的 peony 同款语义）
    // 已按需求移除——自绘时阴影占窗体几何、参与缩放命中与点击判定，
    // 体验差。留白恒为 0；函数保留供缩放手柄厚度 / 吸附补偿等取值点。
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
    relayoutEdgeGrips();   // Windows 未创建手柄，空表直接返回
    updateCornerClips(content);
    updateMask();
}

// ---- 四角圆角裁剪浮层 ----

// 混成环境下内容子控件可能把圆角弧外像素覆盖成直角（锯齿）；
// 在四个角上摆放 AuCornerClip（内容之上）实时清除弧外像素。
void AuWindow::updateCornerClips(const QRect &content)
{
    // Windows：Win11 DWM 圆角 / Win10 窗口区域由系统裁剪全部内容，无需浮层
    if (platformClipsWindowShape()) {
        Q_UNUSED(content)
        return;
    }
    const bool maximized = isMaximized() || isFullScreen();
    const bool composited = platformIsComposited();   // 无混成走 mask 路径，浮层无意义
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
}

void AuWindow::ensurePolishedFrame()
{
    // 与原生窗口句柄建立后（WinIdChange）一致的平台处理集
    platformWinIdChanged();
}

// ---- 边框阴影（X11 由 WM 的 border-only 装饰绘制，见 platformInit；
//      Windows 由 DWM 提供；此处无客户端通道） ----

// ---- 绘制 ----

void AuWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    const AuWindowStyle *st = style();
    const QRect content = rect();   // 阴影经 WM 在窗体矩形外合成，窗体即内容区

    const bool rounded = !isMaximized() && !isFullScreen()
                         && windowRadius() > 0;
    if (rounded) {
        // 抗锯齿：混成环境下窗口为 ARGB 透明背景，圆角靠 alpha 通道平滑
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPainterPath path = AuWindowDetail::auWindowBorderPath(
            QRectF(content), rounded, m_cornerRadii[0], m_cornerRadii[1],
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
            painter.drawPath(AuWindowDetail::auWindowBorderPath(QRectF(content), false, 0, 0, 0, 0));
        }
    }
}

// ---- 手动拖动 / 缩放（跨平台兜底路径；系统原生接管经
//      platformStartSystemMove / platformStartSystemResize 进入各平台
//      实现文件，本节为接管失败或无系统能力时的 Qt 级回退） ----

/// 边缘缩放手柄：贴窗口边缘的透明子控件，Qt 按悬停位置自动切换缩放光标，
/// 按下后发起缩放（系统原生路径，WM 忽略时回退手动）。
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
    // 窗口间磁吸只能拦截手动拖动路径（系统原生通道的拖动循环对本进程
    // 不可见，无法逐帧修正位置）：启用时无条件改走手动拖动，含最大化
    // 还原（startManualMove 的最大化分支手动 showNormal + 挂起目标矩形，
    // 此前该分支在 Windows 上不可达，现于窗口吸附开启时同样服务）。
    // 最大化状态下拖标题栏 = 还原：走手动通道（startManualMove 的最大化
    // 分支：showNormal + 比例锚定目标挂起，WM 落地后单帧应用并 1:1 跟随；
    // 原生交互移动通道 platformStartMaximizeRestoreDrag 已验证在 ukui-kwin
    // 上还原尺寸小窗会在最大化位置 (0,0) 露帧，不再使用）。
    // ⚠️ m_snapped 必须路由回手动通道：边缘吸附态（拖顶最大化 / 半屏贴靠）
    // 的还原靠 startManualMove 的 m_snapped 分支（回到吸附前浮动几何），
    // 走系统通道的话 WM 只会按当前（贴靠）尺寸原生平移——窗口拖不出吸附
    // 尺寸（实测 Qt 5.15.2：贴靠态拖动 960x932 平移，800x600 永不恢复）。
    // ⚠️ 还原全程客户端不得插入几何请求：showNormal 后 Qt 侧状态立即翻转
    // 但 X 侧仍是最大化几何，过渡期的 move/resize 要么被 WM 忽略、要么污
    // 染其还原记忆（实测 Qt 5.15.2 + ukui-kwin）。落位由 WM 异步完成，
    // 客户端只在 adoptManualRestorePosition 的落地判据满足后单帧介入。
    if (m_windowSnapEnabled || m_snapped) {
        startManualMove(pressGlobal);
        return;
    }
    if (isMaximized()) {
        // 最大化态拖标题栏 = 还原：交 WM 原生交互移动（取消最大化 + 比例
        // 锚定跟随一体完成），拖回屏幕顶部松手即得 WM 原生贴顶——系统自带
        // 预览与最大化效果，而非模块自绘吸附（用户实测要求原生观感）。
        // 期间冻结浮动几何记录、撤防还原断言、屏蔽 changeEvent 纠正：
        // 几何归 WM，客户端任何插手都会与拖动循环逐帧互搏（实测窗口在
        // 锚定位与过期记忆位之间高频翻转）。WM 未接管（AuTitleBar 150ms
        // 宽限期判定）回退手动还原；手动通道（m_systemMoveEnabled=false）
        // 维持原有手动还原语义（Qt 5.12 默认，回归基线）。
        //
        // Windows：与 X11 不同，手动通道的松手吸附是关闭的（m_edgeSnapEnabled
        // = false，拖顶/贴边由系统 Snap 负责）——最大化拖拽还原走手动通道时
        // "还原后不松手拖回顶部松手"不会有任何贴靠动作（实测复现 bug）。
        // 故 Windows 上不再受 m_systemMoveEnabled 闸门限制，一律尝试原生
        // 通道（platformStartMaximizeRestoreDrag 重发 HTCAPTION 按下，
        // DefWindowProc 移动循环原生完成还原 + 再次贴靠）。
#ifdef Q_OS_WIN
        m_nativeDragActive = true;
        ++m_nativeDragGen;
        m_normalRestoreAssert = QRect();
        if (platformStartMaximizeRestoreDrag())
            return;
        m_nativeDragActive = false;
#else
        if (m_systemMoveEnabled) {
            m_nativeDragActive = true;
            ++m_nativeDragGen;
            m_normalRestoreAssert = QRect();
            if (platformStartMaximizeRestoreDrag())
                return;
            m_nativeDragActive = false;
        }
#endif
        startManualMove(pressGlobal);
        return;
    }
    // 系统原生拖动开始：冻结浮动几何记录（拖动途中的位置是过渡态，不得
    // 成为还原基准），收场时按终态定基准（见 scheduleNativeDragSettle）
    m_nativeDragActive = true;
    ++m_nativeDragGen;
    if (platformStartSystemMove())
        return;
    m_nativeDragActive = false;
    startManualMove(pressGlobal);
}

void AuWindow::startManualMove(const QPoint &pressGlobal)
{
    m_normalRestoreAssert = QRect();   // 新拖动手势：撤防还原断言保护窗
    m_manualMoving = true;
    m_manualStartGlobal = pressGlobal;
    m_manualStartPos = pos();
    m_manualStartSize = size();
    // 拖出贴靠状态：先还原吸附前尺寸再跟随（Windows 同款行为）。Windows
    // 手动路径仅在窗口间吸附开启时可达（默认拖动一律原生转发），其余
    // 服务 X11 / 其它平台。
    // 时序注意：showNormal() 后 Qt 侧窗口状态同步清零，但 WM 侧还原异步，
    // isMaximized() 不能用作"还原已落地"的守卫——还原几何（KWin 按
    // WM_CLASS 的过期记忆）由挂起的目标矩形在落地收养时显式 setGeometry
    // 兜住（见 maximizeRestoreTarget / adoptManualRestorePosition），
    // 过渡期内绝不插请求。
    if (m_snapped) {
        m_snapped = false;
        m_snapRestoreTarget = QRect();   // 旧还原目标作废（下面按需重设）
        const QRect pre = m_preSnapGeometry;
        if (pre.isValid()) {
            // 保持抓取点在窗口内的相对位置（X/Y 两轴同一比例算法）：还原
            // 后光标仍落在标题栏上，随后 1:1 跟随。Y 轴若取最大化态的 y
            // （≈0，即 m_manualStartPos.y()）会先落屏幕顶再被拖动拉回，
            // 呈现"先还原到 0,0 再跳回"的漂移闪烁
            const qreal ratioX = m_manualStartSize.width() > 0
                    ? qreal(pressGlobal.x() - m_manualStartPos.x()) / m_manualStartSize.width()
                    : 0.0;
            const qreal ratioY = m_manualStartSize.height() > 0
                    ? qreal(pressGlobal.y() - m_manualStartPos.y()) / m_manualStartSize.height()
                    : 0.0;
            const QPoint newPos(pressGlobal.x() - int(qreal(pre.width()) * ratioX),
                                pressGlobal.y() - int(qreal(pre.height()) * ratioY));
            if (isMaximized()) {
                // 拖顶最大化：状态翻转期插入几何请求会闪帧 / 被 WM 忽略
                // （与最大化分支同理，且 changeEvent 在 showNormal 内同步
                // 触发）。目标矩形挂起，showNormal 后由收养机制在 WM 落位
                // 后应用（adoptManualRestorePosition，此时请求必被受理）
                m_manualRestoreTargetRect = QRect(newPos, pre.size());
                m_manualRestorePending = true;   // 必须先于 showNormal
                showNormal();
            } else {
                // 半屏贴边：窗口未进最大化态，直接落位（无竞态）
                setGeometry(QRect(newPos, pre.size()));
                m_manualStartPos = newPos;
                m_manualStartSize = pre.size();
            }
        } else if (isMaximized()) {
            // 拖顶最大化走的是最大化状态：尽力还原（ukui-kwin 在按住期间
            // 可能忽略 showNormal，未落地则该次保持最大化拖动无效）
            m_manualRestoreTargetRect = QRect();
            m_manualRestorePending = true;
            showNormal();
        }
    } else if (isMaximized()) {
        // 按钮 / 双击 / WM 拖顶进入的最大化（非贴靠态）：拖标题栏还原。
        //
        // ⚠️ 落位时序（三次闪帧回归 + 5.15 还原位置错误的定案）：
        // showNormal() 后 Qt 侧 isMaximized() 立即清零，但 X 侧几何仍是
        // 最大化值，WM 取消最大化是异步落地的。此过渡期内【不得】输出任何
        // 几何请求——实测（Qt 5.15.2 + ukui-kwin）：
        //  ① 过渡期的 move() 被 WM 忽略、resize() 会被 WM 当作还原尺寸记
        //     忆（污染其后所有还原落位）；
        //  ② 过渡期读 pos()/size() 得到的仍是最大化值，拿去当跟随基准会把
        //     窗口锚定在最大化位置 (0,0)（"还原后标题栏落在鼠标处"）。
        // 因此按下时刻只把目标挂起（最大化前浮动几何 + 抓取点比例锚定，
        // 与吸附态还原同算法），一律等 WM 落地后由 adoptManualRestorePosition
        // 单帧应用；落地前的跟随帧一律抑制（既不输出几何，也不改基准）。
        // ⚠️ pending 必须先于 showNormal 置位——changeEvent 在 showNormal
        // 内同步触发，后置位则状态翻转瞬间的尺寸兜底会闪帧。
        m_manualRestoreTargetRect = maximizeRestoreTarget(pressGlobal);
        m_manualRestorePending = true;
        showNormal();
    }
    grabMouse();
}

// 最大化态拖标题栏还原的目标矩形：以进入最大化前的浮动几何
// （m_normalGeometry，缺省回退 QWidget::normalGeometry()）为基准，保持抓取
// 点在窗口内的相对位置（X/Y 两轴同一比例算法）——还原后光标仍落在标题栏
// 上，随后 1:1 跟随。无任何已知几何时返回无效矩形（收养 WM 落位兜底）。
QRect AuWindow::maximizeRestoreTarget(const QPoint &pressGlobal) const
{
    const QRect base = m_normalGeometry.isValid() ? m_normalGeometry
                                                  : normalGeometry();
    if (!base.isValid())
        return QRect();
    const qreal ratioX = m_manualStartSize.width() > 0
            ? qreal(pressGlobal.x() - m_manualStartPos.x()) / m_manualStartSize.width()
            : 0.0;
    const qreal ratioY = m_manualStartSize.height() > 0
            ? qreal(pressGlobal.y() - m_manualStartPos.y()) / m_manualStartSize.height()
            : 0.0;
    return QRect(QPoint(pressGlobal.x() - int(qreal(base.width()) * ratioX),
                        pressGlobal.y() - int(qreal(base.height()) * ratioY)),
                 base.size());
}

bool AuWindow::adoptManualRestorePosition()
{
    // 落地守卫：showNormal() 后 Qt 侧状态立即翻转，但 X 侧几何仍是最大化
    // 值、WM 还原异步落地。未落地时收养会把最大化位置 / 尺寸当成跟随基准
    // （"还原后标题栏落在鼠标处"的根因），此时严禁输出任何几何请求。
    if (isMaximized())
        return false;
    if (pos() == m_manualStartPos && size() == m_manualStartSize)
        return false;   // 几何仍是最大化快照：WM 尚未落位
    m_manualRestorePending = false;
    m_snapRestoreTarget = QRect();
    if (m_manualRestoreTargetRect.isValid()) {
        // 已知还原目标（吸附态 / 最大化态拖拽还原）：落到比例锚定目标。
        // 此刻已脱离最大化态，请求必被 WM 受理，单帧到位；位移基准保持
        // 按下点（原始语义：newPos + (cursor - press)，已发生的拖动位移
        // 从锚定位 1:1 延续）
        const QRect target = m_manualRestoreTargetRect;
        m_manualRestoreTargetRect = QRect();
        setGeometry(target);
        m_manualStartPos = target.topLeft();
        m_manualStartSize = target.size();
        // m_manualStartGlobal 保持 startManualMove 置位的按下点
    } else {
        // 无预知目标的兜底：以 WM 实际落点为跟随基准（此刻必已落地）。
        // 不得以预判位置为基准——那会造成"从落点跳向预判点"的二次闪动
        m_manualStartPos = pos();
        m_manualStartSize = size();
        m_manualStartGlobal = QCursor::pos();   // 从当前光标起 1:1 位移跟随
        // KWin 记忆尺寸过期时对齐回浮动尺寸：此刻已落地，resize 不可见
        // （对比状态翻转瞬间的 resize = 最大化位置闪帧）
        if (m_normalGeometry.isValid() && size() != m_normalGeometry.size())
            resize(m_normalGeometry.size());
    }
    return true;
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
    m_normalRestoreAssert = QRect();   // 新缩放手势：撤防还原断言保护窗
    m_pressGlobal = pressGlobal;
    if (platformStartSystemResize(edges)) {
        m_systemResizePending = true;
        m_systemResizeSentMs = QDateTime::currentMSecsSinceEpoch();
        m_pendingEdges = edges;
        startGrabRecoveryPoll();
        return;
    }
    startManualResize(edges, pressGlobal);
}

// ---- 边缘吸附（手动拖动路径） ----

void AuWindow::setEdgeSnapEnabled(bool enable)
{
    m_edgeSnapEnabled = enable;
    if (!enable)
        hideSnapPreview();
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
    // 无混成时窗口无 alpha 通道，铺屏预览会是不透明色块，跳过（直接吸附）
    if (!platformIsComposited())
        return;
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
    m_snapRestoreTarget = QRect();   // 新的吸附使旧还原目标作废
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

// ---- 窗口间吸附（手动拖动路径） ----

void AuWindow::setWindowSnapEnabled(bool enable)
{
    m_windowSnapEnabled = enable;
}

bool AuWindow::isWindowSnapEnabled() const
{
    return m_windowSnapEnabled;
}

// 吸附目标矩形汇集：优先平台全系统枚举（X11：_NET_CLIENT_LIST，返回的
// 是视觉外沿矩形，与麒麟原生对齐视觉边缘一致）；枚举不可用（其它平台 /
// 非 xcb 会话）回退本进程 AuWindow 注册表（内容区几何，无边框窗体无
// WM 外框，视觉边即窗口边）。
QVector<QRect> AuWindow::windowSnapCandidateRects() const
{
    QVector<QRect> rects = platformWindowSnapCandidates();
    if (!rects.isEmpty())
        return rects;
    const QVector<AuWindow *> &registry = auWindowRegistry();
    rects.reserve(registry.size());
    for (AuWindow *w : registry) {
        if (w == this || !w->isVisible() || w->isMinimized())
            continue;
        rects.append(w->geometry());
    }
    return rects;
}

// 磁吸修正：X/Y 两轴独立判定，各取 |偏移| 最小且落在吸附带内的候选。
// 双轴独立 ⇒ 水平贴合的同时自动对齐相邻边（顶边对齐等），与麒麟原生
// 行为一致；允许负偏移（已越过对方边缘时拉回贴合）。吸附带边界天然
// 形成磁吸粘滞：贴合后继续拖动，原始位置超出吸附带即释放（窗口跳出
// 吸附带距离，与常见磁吸实现一致）。手动拖动中尺寸不变，me 用 size()。
QPoint AuWindow::applyWindowSnap(const QPoint &proposedPos) const
{
    const QVector<QRect> candidates = windowSnapCandidateRects();
    if (candidates.isEmpty())
        return proposedPos;
    const int zone = 10;   // 吸附带宽度，与 edgeSnapTarget() 一致
    const QRect me(proposedPos, size());
    int bestDx = zone + 1;   // 哨兵：吸附带外（0 也是合法修正值，不可作初值）
    int bestDy = zone + 1;
    for (const QRect &c : candidates) {
        // x 轴：左缘贴对方右缘（从左侧靠近）/ 右缘贴对方左缘（从右侧
        // 靠近）/ 左右缘分别与对方同侧缘对齐（并肩摆放）
        const int candsX[4] = {
            c.right() + 1 - me.left(),   // me.left  → c.right+1（边缘相接）
            c.left() - me.right() - 1,   // me.right → c.left-1
            c.left() - me.left(),        // me.left  → c.left（左缘对齐）
            c.right() - me.right()       // me.right → c.right（右缘对齐）
        };
        for (int d : candsX) {
            const int ad = qAbs(d);
            if (ad <= zone && ad < qAbs(bestDx))
                bestDx = d;
        }
        const int candsY[4] = {
            c.bottom() + 1 - me.top(),   // me.top    → c.bottom+1
            c.top() - me.bottom() - 1,   // me.bottom → c.top-1
            c.top() - me.top(),          // me.top    → c.top（顶缘对齐）
            c.bottom() - me.bottom()     // me.bottom → c.bottom（底缘对齐）
        };
        for (int d : candsY) {
            const int ad = qAbs(d);
            if (ad <= zone && ad < qAbs(bestDy))
                bestDy = d;
        }
    }
    QPoint result = proposedPos;
    if (qAbs(bestDx) <= zone)
        result.rx() += bestDx;
    if (qAbs(bestDy) <= zone)
        result.ry() += bestDy;
    return result;
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
        // 最大化拖拽还原：WM 落地前抑制跟随帧（不输出任何几何请求、不改
        // 跟随基准——基准仍是按下时刻快照，落地后从目标矩形起算全程位移）；
        // 落地当帧应用目标矩形并从目标起 1:1 跟随
        if (m_manualRestorePending && !adoptManualRestorePosition())
            return;
        // 跟随帧接管落位：锚点保护期结束，其后 WM 异步还原的过期几何由
        // 逐帧绝对定位自然覆盖，changeEvent 不再纠正（否则会把已拖动的
        // 位移拽回锚点）
        m_snapRestoreTarget = QRect();
        QPoint newPos = m_manualStartPos + (QCursor::pos() - m_manualStartGlobal);
        if (m_windowSnapEnabled)
            newPos = applyWindowSnap(newPos);   // 窗口间磁吸：贴近其它窗口时修正落点
        applyPositionViaWM(newPos);
        const SnapEdge snap = edgeSnapTarget(QCursor::pos());
        if (snap != SnapEdge::None)
            showSnapPreview(snap);
        else
            hideSnapPreview();
        return;
    }
    platformMouseMoveEvent(event);
    QWidget::mouseMoveEvent(event);
}

void AuWindow::mouseReleaseEvent(QMouseEvent *event)
{
    noteMouseRelease();
    m_systemResizePending = false;
    if (m_manualResizing || m_manualMoving) {
        if (m_manualMoving) {
            if (m_manualRestorePending) {
                // 还原过渡期内松手：收养尚未发生，放弃挂起目标——窗口交给
                // WM 自行落位（其后的 max→normal changeEvent 纠正会按
                // m_normalGeometry 兜底），不得把最大化快照误当吸附前几何
                m_manualRestorePending = false;
                m_manualRestoreTargetRect = QRect();
            } else {
                applyEdgeSnap(edgeSnapTarget(event->globalPos()));
            }
        } else {
            hideSnapPreview();
        }
        m_manualResizing = false;
        m_manualMoving = false;
        releaseMouse();
    }
    QWidget::mouseReleaseEvent(event);
}

void AuWindow::noteMouseRelease()
{
    m_awaitingRelease = false;
    if (m_grabRecoveryTimer)
        m_grabRecoveryTimer->stop();
    scheduleNativeDragSettle();
    // 无条件松抓：真实 release 被 WM 吞掉、由其它路径（抓取恢复轮询 / 标题
    // 栏 / 手柄的 release 槽）先清掉拖动态时，若只在 mouseReleaseEvent 的
    // 拖动态分支里 releaseMouse，抓取会永久滞留——后续所有鼠标事件都被
    // 路由到本窗体，标题栏再也收不到按下，表现为"还原后窗口无法拖动、
    // 必须再点一次标题栏"（Qt 5.15.2 实测症状2）
    if (QWidget::mouseGrabber() == this)
        releaseMouse();
}

// 还原纠正断言保护（500ms 窗）：纠正后 KWin 的两步落位步骤 b（移到其
// 记忆位置）会迟到几十毫秒，窗内 moveEvent 据此重新断言；到期自动撤防，
// 用户按下 / 发起新手势时由调用点提前撤防。
void AuWindow::armNormalRestoreAssert(const QRect &target)
{
    m_normalRestoreAssert = target;
    QTimer::singleShot(500, this, [this, target] {
        if (m_normalRestoreAssert == target)
            m_normalRestoreAssert = QRect();
    });
}

// 原生拖动收场：延迟定基准——release 与 WM 的收场动作存在竞态（实测
// ukui-kwin：release 先 ~13ms 到达、最大化 StateChange 后到；原生贴靠的
// 落地 ConfigureNotify 也可能晚于首个 250ms 拍），立即判定会误判收场态。
// 每拍 250ms，至多 retries 拍：
//  - 几何仍在变（WM 摆放中）→ 再等一拍；
//  - 以最大化 / 全屏收场 → 不记录：基准保持拖动起点（期间记录已冻结），
//    还原 / 按钮还原回到拖动前位置，而非顶缘附近的拖动终点（"位置漂移"）；
//  - 常规收场且尺寸未变 → 终点即用户摆放，照常记录；
//  - 常规收场但尺寸被 WM 改变（拖缘触发原生贴靠）→ 保留拖动起点基准
//    （贴靠前的浮动几何）：贴靠态的后续最大化还原据此回到原始浮动尺寸，
//    而非贴靠尺寸（用户报告"原生半贴→最大化→拖拽还原变成半贴"）；
//    反向的贴靠拖出还原（WM 恢复贴靠前尺寸 = 冻结基准尺寸）则照常记录。
// 期间用户又开始新拖动 → 代次不符，本定时器作废（新拖动重新冻结记录）。
void AuWindow::scheduleNativeDragSettle(int retries)
{
    if (!m_nativeDragActive)
        return;
    const int gen = m_nativeDragGen;
    const QRect expect = geometry();
    QTimer::singleShot(250, this, [this, gen, expect, retries] {
        if (!m_nativeDragActive || m_nativeDragGen != gen)
            return;
        if (isMaximized() || isFullScreen()) {
            m_nativeDragActive = false;   // 最大化 / 全屏收场：基准保持拖动起点
            return;
        }
        if (retries > 0 && geometry() != expect) {
            scheduleNativeDragSettle(retries - 1);   // WM 仍在摆放，再等一拍
            return;
        }
        m_nativeDragActive = false;
        if (!m_snapped && size() == m_normalGeometry.size())
            m_normalGeometry = geometry();
    });
}
