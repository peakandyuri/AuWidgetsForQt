#include "AuWindow.h"
#include "AuTitleBar.h"
#include "AuWindowStyle.h"
#include "AuBorderShadow.h"
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
    , m_shadowTileColor()
    , m_shadowTileBlur(0)
    , m_shadowTileRadii{0, 0, 0, 0}
    , m_shadowTileDpr(1.0)
    , m_shadowTileValid(false)
    , m_manualResizing(false)
    , m_manualMoving(false)
    , m_systemMoveEnabled(false)   // 系统通道在 platformInit() 中按平台能力开启
    , m_edgeSnapEnabled(true)
    , m_snapPreview(nullptr)
    , m_snapped(false)
    , m_wmGeometryBroken(false)
    , m_wmGeometryPending(false)
    , m_systemResizePending(false)
    , m_systemResizeSentMs(0)
    , m_pressGlobal(0, 0)
    , m_normalSizeHint(0, 0)
    , m_cornerClips{nullptr, nullptr, nullptr, nullptr}
    , m_grabRecoveryTimer(nullptr)
    , m_awaitingRelease(false)
    , m_creationGeometry()
{
    // 平台差异（窗口标志 / 属性、内容区容器、边缘缩放手柄、系统移动通道）
    platformInit();

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
    invalidateShadowTile();      // 贴图弧形跟随四角半径
    platformShadowUpdate();      // X11：WM 合成贴图重传
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
    invalidateShadowTile();
    platformShadowUpdate();
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
    relayout();     // 阴影环宽变化 → 内容区 / 手柄 / 裁剪浮层跟随
    update();
}

bool AuWindow::isBorderShadowEnabled() const
{
    return m_borderShadowEnabled;
}

void AuWindow::invalidateShadowTile()
{
    m_shadowTileValid = false;
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
    if (m_style == style)
        return;
    delete m_style;
    m_style = style;
    invalidateShadowTile();      // 阴影色 / 模糊 / 留白可能变化
    platformShadowUpdate();      // X11：刷新 WM 合成贴图；Windows：DWM 跟随
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
    invalidateShadowTile();
    platformShadowUpdate();
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

void AuWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // 记录非最大化浮动尺寸：供最大化还原（系统通道 / 按钮均经 WM 记忆几何）
    // 后纠正 KWin 的过期记忆尺寸
    if (!isMaximized() && !isFullScreen() && !m_snapped)
        m_normalSizeHint = size();
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
        if (!max && m_normalSizeHint.isValid() && size() != m_normalSizeHint) {
            // 最大化还原的几何由 WM 的记忆决定，记忆可能过期（KWin 按
            // WM_CLASS 记忆历史尺寸），纠正回进入最大化前的浮动尺寸
            resize(m_normalSizeHint);
        }
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
    // 阴影留白环（peony BorderShadowEffect 同款窗口语义：窗体矩形含环，
    // 内容区内缩）：仅客户端阴影通道生效。WM 合成通道（X11
    // _KDE_NET_WM_SHADOW，ukui-kwin / KWin）阴影绘制在窗体矩形之外，
    // 环宽保持 0、窗体几何不受影响。最大化 / 全屏收起；无混成（ARGB
    // 不可用，阴影画不出来）同样收起。
    const int margin = style()->shadowMargin();
    if (margin <= 0 || !m_borderShadowEnabled)
        return 0;
    if (isMaximized() || isFullScreen())
        return 0;
    if (!platformIsComposited())
        return 0;
    if (platformShadowPaintedExternally())
        return 0;
    return margin;
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

// ---- 边框阴影（客户端绘制通道） ----

// 九宫格贴图缓存：一次模糊（qt_blurImage）反复绘制，缩放窗口零模糊开销
// （ukui ConfigShadowHelper 同款策略）。键：颜色 / 模糊半径 / 生效四角
// 半径 / 设备像素比。
void AuWindow::ensureShadowTile()
{
    const AuWindowStyle *st = style();
    const QColor color = st->shadowColor();
    // 模糊半径即渐隐预算：超出 shadowMargin 留白的部分会被窗体边缘裁出
    // 硬边，钳到留白宽度内（默认 blur 28 / margin 24 → 按 24 渲染）
    const int blur = qMin(st->shadowBlurRadius(), st->shadowMargin());
    const qreal dpr = devicePixelRatioF();
    const int radii[4] = {
        cornerRadius(Qt::TopLeftCorner), cornerRadius(Qt::TopRightCorner),
        cornerRadius(Qt::BottomLeftCorner), cornerRadius(Qt::BottomRightCorner)
    };
    if (m_shadowTileValid && m_shadowTileColor == color && m_shadowTileBlur == blur
        && m_shadowTileRadii[0] == radii[0] && m_shadowTileRadii[1] == radii[1]
        && m_shadowTileRadii[2] == radii[2] && m_shadowTileRadii[3] == radii[3]
        && m_shadowTileDpr == dpr) {
        return;
    }
    m_shadowTile = AuWindowDetail::auRenderBorderShadowTile(
        color, blur, radii[0], radii[1], radii[2], radii[3], dpr);
    m_shadowTileColor = color;
    m_shadowTileBlur = blur;
    for (int i = 0; i < 4; ++i)
        m_shadowTileRadii[i] = radii[i];
    m_shadowTileDpr = dpr;
    m_shadowTileValid = true;
}

// 围绕内容区 content 绘制阴影环（peony / ukui 九宫格摆法）：4 块角贴图
// 原尺寸对齐窗角弧线，4 条 1px 边条拉伸填充直边段。角贴图会盖到内容区
// 的圆角挖空区——贴图内部（弧线内）已在渲染时清除为透明，弧外的模糊
// 阴影恰好从 ARGB 窗口的透明角像素透出，与 KWin 的 WM 侧合成行为一致。
// 贴图为设备像素位图（未设 DPR 标志）：源矩形一律以逻辑像素给出、经
// dpr 换算成设备像素源区域，目标矩形为 painter 逻辑坐标。
void AuWindow::paintBorderShadow(QPainter *painter, const QRect &content)
{
    ensureShadowTile();
    const QImage &tile = m_shadowTile;
    if (tile.isNull())
        return;

    const int b = m_shadowTileBlur;
    if (b <= 0 || content.isEmpty())
        return;
    const int rTL = m_shadowTileRadii[0], rTR = m_shadowTileRadii[1];
    const int rBL = m_shadowTileRadii[2], rBR = m_shadowTileRadii[3];
    const int R = qMax(qMax(rTL, rTR), qMax(rBL, rBR));
    // 角贴图边长 = b + 对应角半径（1:1 摆放，不拉伸；直角窗即纯环段 b²）
    const int cTL = b + rTL, cTR = b + rTR, cBL = b + rBL, cBR = b + rBR;
    const int stripLen = b + R;          // 边条源条长（伸入内容区至最大弧深）
    const qreal d = m_shadowTileDpr;     // 逻辑 → 设备像素换算
    const int L = 2 * R + 2 * b + 1;     // 贴图逻辑边长（渲染时按 dpr 取整）

    const int cl = content.left(), ct = content.top();
    const int cr = content.right(), cb = content.bottom();
    const int cw = content.width(), ch = content.height();

    auto put = [&](const QRect &target, int sx, int sy, int sw, int sh) {
        painter->drawImage(target, tile,
                           QRect(qRound(sx * d), qRound(sy * d),
                                 qRound(sw * d), qRound(sh * d)));
    };

    // 四角（弧线对齐：角贴图原点置于窗角 - 渐隐环宽 b，贴图内 (b, b) 处
    // 的窗口矩形弧线恰好落在内容区角上）
    put(QRect(cl - b, ct - b, cTL, cTL), 0, 0, cTL, cTL);
    put(QRect(cr + 1 - rTR, ct - b, cTR, cTR), L - cTR, 0, cTR, cTR);
    put(QRect(cl - b, cb + 1 - rBL, cBL, cBL), 0, L - cBL, cBL, cBL);
    put(QRect(cr + 1 - rBR, cb + 1 - rBR, cBR, cBR), L - cBR, L - cBR, cBR, cBR);

    // 边条：1px 源条拉伸填充直边段；跨度按对应角半径计算，窗口过小时
    // 角贴图重叠（跨度 ≤ 0）则跳过该边条（KWin 同款处理）
    const int hSpanTop = cw - rTL - rTR;
    const int hSpanBottom = cw - rBL - rBR;
    const int vSpanLeft = ch - rTL - rBL;
    const int vSpanRight = ch - rTR - rBR;
    if (hSpanTop > 0)
        put(QRect(cl + rTL, ct - b, hSpanTop, stripLen), b + R, 0, 1, stripLen);
    if (hSpanBottom > 0)
        put(QRect(cl + rBL, cb + 1 - R, hSpanBottom, stripLen), b + R, L - stripLen, 1, stripLen);
    if (vSpanLeft > 0)
        put(QRect(cl - b, ct + rTL, stripLen, vSpanLeft), 0, b + R, stripLen, 1);
    if (vSpanRight > 0)
        put(QRect(cr + 1 - R, ct + rTR, stripLen, vSpanRight), L - stripLen, b + R, stripLen, 1);
}

// ---- 绘制 ----

void AuWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    const AuWindowStyle *st = style();
    const int margin = effectiveMargin();
    const QRect content = rect().adjusted(margin, margin, -margin, -margin);

    // 阴影环先于内容区背景绘制（贴图与内容区重叠的部分在渲染时已按
    // 窗口形状清除，弧外渐隐由 ARGB alpha 呈现）
    if (margin > 0)
        paintBorderShadow(&painter, content);


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
    // 最大化状态下拖标题栏 = 还原：无条件优先走 WM 原生通道（即便手动通道
    // 默认的 5.12）。KWin 的交互移动在最大化窗口上 = 取消最大化 + 锚定光标
    // 一次到位；手动路径的"showNormal + setGeometry"做不到单帧落位——普通
    // ConfigureRequest 对最大化窗口被 WM 忽略，而"showNormal 先落 WM 记忆
    // 几何、再重定位"会产生 A→B 位置闪动。接管后的事件冻结由 grab-recovery
    // 轮询兜底；还原尺寸偏差（KWin 记忆过期）由 changeEvent 的 max→normal
    // 分支按 m_normalSizeHint 纠正。WM 不接管时回退手动还原。
    if (isMaximized() && platformStartMaximizeRestoreDrag())
        return;
    if (platformStartSystemMove())
        return;
    startManualMove(pressGlobal);
}

void AuWindow::startManualMove(const QPoint &pressGlobal)
{
    m_manualMoving = true;
    m_manualStartGlobal = pressGlobal;
    m_manualStartPos = pos();
    m_manualStartSize = size();
    // 拖出贴靠状态：先还原吸附前尺寸再跟随（Windows 同款行为）。Windows
    // 手动路径不可达（拖动一律原生转发），本分支仅服务 X11 / 其它平台。
    // 时序注意：showNormal() 后 Qt 侧窗口状态同步清零，但 WM 侧还原异步，
    // isMaximized() 不能用作"还原已落地"的守卫——还原尺寸必须由我们
    // 显式 setGeometry 兜住，否则 KWin 会套用它按 WM_CLASS 记忆的过期
    // 还原几何（表现为还原后尺寸不对）。
    if (m_snapped) {
        m_snapped = false;
        if (isMaximized()) {
            // 顶部吸附走的是最大化状态：尽力还原（ukui-kwin 在按住期间
            // 可能忽略 showNormal，未落地则该次保持最大化拖动无效）
            showNormal();
        }
        const QRect pre = m_preSnapGeometry;
        if (pre.isValid()) {
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
    } else if (isMaximized()) {
        // 按钮 / 双击进入的最大化（非贴靠态）：手动路径下拖标题栏还原。
        // 此前无该分支——拖动被当作普通移动发给 WM，而 WM 会忽略最大化
        // 窗口的移动请求，表现为"最大化后拖标题栏无法还原"。系统通道
        // （5.15 默认）由 WM 原生还原，不经此处。
        //
        // 还原位置取进入最大化前的原位（= WM 取消最大化时将应用的还原
        // 几何）：我们的 setGeometry 与 WM 的还原落点重合，避免"WM 先落
        // A、我们再跳到光标处 B"的单帧位置闪动；窗口从原位起按 1:1 位移
        // 跟随本次拖动。
        // normalGeometry() 必须在 showNormal() 之前取：最大化态下它返回
        // 进入最大化前保存的还原几何；showNormal 之后 Qt 侧状态同步清零，
        // 再取就落到通用分支（返回当前几何 = 最大化尺寸），还原尺寸即错。
        const QRect normalGeo = normalGeometry();
        showNormal();
        if (normalGeo.isValid()) {
            const QPoint newPos = normalGeo.topLeft();
            setGeometry(QRect(newPos, normalGeo.size()));
            m_manualStartPos = newPos;
            m_manualStartSize = normalGeo.size();
        }
    }
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
    platformMouseMoveEvent(event);
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

void AuWindow::noteMouseRelease()
{
    m_awaitingRelease = false;
    if (m_grabRecoveryTimer)
        m_grabRecoveryTimer->stop();
}
