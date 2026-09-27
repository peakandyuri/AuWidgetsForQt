#include "AuPopupPanel.h"

#include <QApplication>
#include <QGridLayout>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QScreen>
#include <QWheelEvent>
#include <QWindow>
#include <QtMath>

#include <vector>

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#endif

namespace {

/// 单方向盒式模糊（半径 radius，边缘按邻近值延续）。
void boxBlurPass(const std::vector<quint8> &src, std::vector<quint8> &dst,
                 int width, int height, int radius, bool horizontal)
{
    const int windowSize = 2 * radius + 1;
    if (horizontal) {
        for (int y = 0; y < height; ++y) {
            const int row = y * width;
            int sum = 0;
            for (int x = -radius; x <= radius; ++x)
                sum += src[row + qBound(0, x, width - 1)];
            for (int x = 0; x < width; ++x) {
                dst[row + x] = static_cast<quint8>(sum / windowSize);
                sum += src[row + qBound(0, x + radius + 1, width - 1)]
                       - src[row + qBound(0, x - radius, width - 1)];
            }
        }
    } else {
        for (int x = 0; x < width; ++x) {
            int sum = 0;
            for (int y = -radius; y <= radius; ++y)
                sum += src[qBound(0, y, height - 1) * width + x];
            for (int y = 0; y < height; ++y) {
                dst[y * width + x] = static_cast<quint8>(sum / windowSize);
                sum += src[qBound(0, y + radius + 1, height - 1) * width + x]
                       - src[qBound(0, y - radius, height - 1) * width + x];
            }
        }
    }
}

} // namespace

AuPopupPanel::AuPopupPanel(QWidget *parent)
    : QWidget(parent)
    , m_content(nullptr)
    , m_layout(nullptr)
    , m_backgroundColor(Qt::white)
    , m_borderColor(QColor(0, 0, 0, 20))
    , m_shadowColor(0, 0, 0, 45)
    , m_cornerRadius(8)
    , m_shadowRadius(10)
    , m_gap(4)
    , m_placedAbove(false)
    , m_matchOwnerWidth(true)
    , m_escapeCloses(true)
    , m_tileBoxRadius(0)
    , m_tileCornerRadius(0)
    , m_tileColorKey(0)
    , m_animMode(AnimNone)
    , m_eventFilterInstalled(false)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_X11DoNotAcceptFocus); // X11：点击面板不转移输入焦点
    setFocusPolicy(Qt::NoFocus);

    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    updateLayout();

    m_geometryAnimation = new QVariantAnimation(this);
    m_geometryAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_geometryAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setGeometry(value.toRect());
    });
    connect(m_geometryAnimation, &QVariantAnimation::finished, this, [this]() {
        if (m_animMode == AnimOpening) {
            m_animMode = AnimNone;
            emit shown();
        } else if (m_animMode == AnimClosing) {
            m_animMode = AnimNone;
            if (m_eventFilterInstalled) {
                qApp->removeEventFilter(this);
                m_eventFilterInstalled = false;
            }
            hide();
            emit closed();
        }
    });

    // 焦点移出宿主子树（但不是移入面板自身）时收起；焦点丢失到其他应用由
    // 宿主窗口的 WindowDeactivate 事件处理。
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget * /*old*/, QWidget *now) {
        if (!isVisible() || m_animMode == AnimClosing || !now)
            return;
        if (m_owner && (now == m_owner || m_owner->isAncestorOf(now)))
            return;
        if (now == this || isAncestorOf(now))
            return;
        closePopup();
    });
}

AuPopupPanel::~AuPopupPanel()
{
    dismiss();
}

void AuPopupPanel::setContentWidget(QWidget *content)
{
    if (m_content == content)
        return;
    QWidget *oldContent = m_content;
    m_content = content;
    if (m_content) {
        m_layout->addWidget(m_content);
    }
    if (oldContent)
        oldContent->deleteLater();
}

QWidget *AuPopupPanel::contentWidget() const
{
    return m_content;
}

int AuPopupPanel::cornerRadius() const
{
    return m_cornerRadius;
}

void AuPopupPanel::setCornerRadius(int radius)
{
    m_cornerRadius = qMax(0, radius);
    update();
}

bool AuPopupPanel::isMatchOwnerWidth() const
{
    return m_matchOwnerWidth;
}

void AuPopupPanel::setMatchOwnerWidth(bool match)
{
    m_matchOwnerWidth = match;
}

bool AuPopupPanel::isEscapeCloses() const
{
    return m_escapeCloses;
}

void AuPopupPanel::setEscapeCloses(bool closes)
{
    m_escapeCloses = closes;
}

QColor AuPopupPanel::backgroundColor() const
{
    return m_backgroundColor;
}

void AuPopupPanel::setBackgroundColor(const QColor &color)
{
    m_backgroundColor = color;
    update();
}

QColor AuPopupPanel::borderColor() const
{
    return m_borderColor;
}

void AuPopupPanel::setBorderColor(const QColor &color)
{
    m_borderColor = color;
    update();
}

QColor AuPopupPanel::shadowColor() const
{
    return m_shadowColor;
}

void AuPopupPanel::setShadowColor(const QColor &color)
{
    m_shadowColor = color;
    update();
}

int AuPopupPanel::shadowRadius() const
{
    return m_shadowRadius;
}

void AuPopupPanel::setShadowRadius(int radius)
{
    m_shadowRadius = qMax(0, radius);
    updateLayout();
    update();
}

int AuPopupPanel::gap() const
{
    return m_gap;
}

void AuPopupPanel::setGap(int gap)
{
    m_gap = qMax(0, gap);
}

int AuPopupPanel::shadowMargin() const
{
    return marginForRadius(m_shadowRadius);
}

QSize AuPopupPanel::outerSizeForContent(const QSize &contentSize) const
{
    const int outer = shadowMargin() + 1;
    return contentSize + QSize(2 * outer, 2 * outer);
}

bool AuPopupPanel::isPopupVisible() const
{
    return isVisible();
}

bool AuPopupPanel::isPlacedAbove() const
{
    return m_placedAbove;
}

void AuPopupPanel::popupOver(QWidget *owner, const QSize &preferredSize)
{
    if (!owner)
        return;
    m_owner = owner;
    m_ownerWindow = owner->window();

    QRect avail = QRect(QPoint(0, 0), QSize(1024, 768));
    if (QScreen *screen = screenFor(owner))
        avail = screen->availableGeometry();

    const QRect ownerRect(owner->mapToGlobal(QPoint(0, 0)), owner->size());
    const int outer = shadowMargin() + 1; // 投影边距 + 边框线宽

    // 内容区宽度：默认拉伸到不小于宿主宽度（matchOwnerWidth，与编辑框同宽语义）；
    // 固定宽度面板（matchOwnerWidth=false）保持期望宽度；均不超过屏幕可用区（减去两侧投影）
    const int minHeight = 8;
    int width;
    if (m_matchOwnerWidth)
        width = qBound(ownerRect.width(), preferredSize.width(), qMax(ownerRect.width(), avail.width() - 2 * outer));
    else
        width = qBound(minHeight, preferredSize.width(), qMax(minHeight, avail.width() - 2 * outer));
    int height = qMax(preferredSize.height(), minHeight);

    // 垂直：优先下方；下方放不下且上方放得下则翻转到上方；都放不下取空间更大的一侧收敛
    const int spaceBelow = avail.bottom() - ownerRect.bottom() - m_gap - 2 * outer;
    const int spaceAbove = ownerRect.top() - avail.top() - m_gap - 2 * outer;
    bool above = false;
    if (height > spaceBelow) {
        if (height <= spaceAbove)
            above = true;
        else
            above = spaceAbove > spaceBelow;
    }
    if (above)
        height = qBound(minHeight, height, qMax(spaceAbove, minHeight));
    else
        height = qBound(minHeight, height, qMax(spaceBelow, minHeight));

    // 水平：内容区与宿主左对齐；右侧放不下时切换为右对齐；整体收敛到屏幕内
    int contentLeft = ownerRect.left();
    if (contentLeft + width > avail.right() + 1)
        contentLeft = ownerRect.right() + 1 - width;
    contentLeft = qBound(avail.left() + outer, contentLeft,
                         qMax(avail.left() + outer, avail.right() + 1 - outer - width));

    // 内容区与宿主边缘对齐（gap 为内容区与宿主之间的视觉间距）
    const int contentTop = above ? ownerRect.top() - m_gap - height
                                 : ownerRect.bottom() + 1 + m_gap;
    QRect finalRect(contentLeft - outer, contentTop - outer,
                    width + 2 * outer, height + 2 * outer);
    m_placedAbove = above;

    // 面板是带父控件的顶级窗口（Qt::Tool），setGeometry 使用全局屏幕坐标
    // （与 QMenu::popup / QComboBox 弹层一致），这里不做父坐标换算。

    if (!m_eventFilterInstalled) {
        qApp->installEventFilter(this);
        m_eventFilterInstalled = true;
    }

    emit aboutToShow();
    stopAnimation();

    if (isVisible()) {
        setGeometry(finalRect);
        raise();
        return;
    }

    setGeometry(collapsedRect(finalRect, above));
    show();
    raise();
    animateGeometry(finalRect, true);
}

void AuPopupPanel::closePopup()
{
    if (m_animMode == AnimClosing)
        return;
    if (!isVisible()) {
        if (m_eventFilterInstalled) {
            qApp->removeEventFilter(this);
            m_eventFilterInstalled = false;
        }
        return;
    }

    emit aboutToClose();
#ifdef Q_OS_WIN
    const QRect current = geometry();
    animateGeometry(collapsedRect(current, m_placedAbove), false);
#else
    // Linux / Wayland：顶级窗口逐帧改尺寸依赖窗口系统回配（Wayland 需要合成器
    // 逐帧 configure，X11 受合成器与异步 ConfigureNotify 影响），中间帧被合并
    // 丢弃，收起动画的实际观感是面板先跳到收起尺寸停顿一下再消失。因此非
    // Windows 平台不做几何收起动画，直接隐藏（弹出动画保留）。
    stopAnimation();
    if (m_eventFilterInstalled) {
        qApp->removeEventFilter(this);
        m_eventFilterInstalled = false;
    }
    hide();
    emit closed();
#endif
}

void AuPopupPanel::dismiss()
{
    stopAnimation();
    if (m_eventFilterInstalled) {
        qApp->removeEventFilter(this);
        m_eventFilterInstalled = false;
    }
    if (isVisible()) {
        hide();
        emit closed();
    }
}

void AuPopupPanel::paintEvent(QPaintEvent * /*event*/)
{
    if (m_tileBoxRadius != boxBlurRadius(m_shadowRadius)
        || m_tileCornerRadius != m_cornerRadius
        || m_tileColorKey != m_shadowColor.rgba()) {
        rebuildShadowTiles();
    }

    QPainter painter(this);
    const int margin = shadowMargin();
    const int side = m_shadowTiles[0].width();
    if (side <= 0)
        return;

    const int w = width();
    const int h = height();
    const int corner = qMin(side, qMin(w, h) / 2);

    // 9-slice：四个角块原样绘制，边缘拉伸 1px 剖面，中心拉伸纯色。
    painter.drawImage(QRect(0, 0, corner, corner), m_shadowTiles[0], QRect(0, 0, corner, corner));
    painter.drawImage(QRect(w - corner, 0, corner, corner), m_shadowTiles[1], QRect(side - corner, 0, corner, corner));
    painter.drawImage(QRect(0, h - corner, corner, corner), m_shadowTiles[2], QRect(0, side - corner, corner, corner));
    painter.drawImage(QRect(w - corner, h - corner, corner, corner), m_shadowTiles[3],
                      QRect(side - corner, side - corner, corner, corner));

    painter.drawImage(QRect(corner, 0, w - 2 * corner, corner), m_shadowTiles[0], QRect(side - 1, 0, 1, corner));
    painter.drawImage(QRect(corner, h - corner, w - 2 * corner, corner), m_shadowTiles[2], QRect(side - 1, 0, 1, corner));
    painter.drawImage(QRect(0, corner, corner, h - 2 * corner), m_shadowTiles[0], QRect(0, side - 1, corner, 1));
    painter.drawImage(QRect(w - corner, corner, corner, h - 2 * corner), m_shadowTiles[1], QRect(0, side - 1, corner, 1));
    painter.drawImage(QRect(corner, corner, w - 2 * corner, h - 2 * corner), m_shadowTiles[0], QRect(side - 1, side - 1, 1, 1));

    // 面板背景与边框
    const QRectF contentRect(margin, margin, w - 2.0 * margin, h - 2.0 * margin);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_backgroundColor);
    painter.drawRoundedRect(contentRect, m_cornerRadius, m_cornerRadius);

    if (m_borderColor.isValid()) {
        QPen pen(m_borderColor, 1.0);
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(contentRect.adjusted(0.5, 0.5, -0.5, -0.5), m_cornerRadius, m_cornerRadius);
    }
}

bool AuPopupPanel::eventFilter(QObject *watched, QEvent *event)
{
    const auto ownerRect = [this]() -> QRect {
        if (!m_owner || !m_owner->isVisible())
            return QRect();
        return QRect(m_owner->mapToGlobal(QPoint(0, 0)), m_owner->size());
    };

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick: {
        const QPoint globalPos = static_cast<QMouseEvent *>(event)->globalPos();
        if (!rect().contains(mapFromGlobal(globalPos)) && !ownerRect().contains(globalPos))
            closePopup();
        break;
    }
    case QEvent::Wheel: {
        const QPoint globalPos = static_cast<QWheelEvent *>(event)->globalPos();
        if (!rect().contains(mapFromGlobal(globalPos)) && !ownerRect().contains(globalPos))
            closePopup();
        break;
    }
    case QEvent::KeyPress: {
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            emit escapePressed();
            if (m_escapeCloses)
                closePopup(); // escapeCloses=false 时由宿主在 escapePressed 里决定收起或返回上一级
            return true;
        }
        break;
    }
    case QEvent::WindowDeactivate:
    case QEvent::Hide: {
        if (watched == m_ownerWindow.data()) {
            // 面板显示本身可能引发宿主窗口失活（offscreen 等不遵守
            // WA_ShowWithoutActivating 的平台会激活面板），此时不收起；
            // 用户切换到其他应用时活动窗口为空，正常收起。
            if (QApplication::activeWindow() != this)
                closePopup();
        }
        break;
    }
    case QEvent::Move: {
        // 拖动宿主窗口（或宿主控件被布局移动）时收起，避免面板与宿主错位悬空
        if (watched == m_ownerWindow.data() || watched == m_owner.data())
            closePopup();
        break;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

bool AuPopupPanel::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        // 点击面板时不在窗口系统层激活面板，避免宿主窗口（编辑框焦点）被抢走
        if (msg->message == WM_MOUSEACTIVATE) {
            *result = MA_NOACTIVATE;
            return true;
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

void AuPopupPanel::updateLayout()
{
    const int outer = shadowMargin() + 1;
    if (m_layout)
        m_layout->setContentsMargins(outer, outer, outer, outer);
}

void AuPopupPanel::rebuildShadowTiles()
{
    const int boxRadius = boxBlurRadius(m_shadowRadius);
    const int margin = marginForRadius(m_shadowRadius);
    // 角块边长覆盖：投影边距 + 圆角 + 完整模糊剖面（2 次盒式模糊，扩展约 4 * boxRadius）
    const int side = margin + m_cornerRadius + 4 * boxRadius + 2;

    QImage tile(side, side, QImage::Format_ARGB32);
    tile.fill(Qt::transparent);
    {
        QPainter painter(&tile);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        // 先以不透明色绘制圆角矩形，再对 alpha 通道做盒式模糊，最后按投影色 alpha 缩放
        QColor solid = m_shadowColor;
        solid.setAlpha(255);
        painter.setBrush(solid);
        painter.drawRoundedRect(QRect(margin, margin, side - margin, side - margin),
                                m_cornerRadius, m_cornerRadius);
    }

    const int pixelCount = side * side;
    std::vector<quint8> buffer(pixelCount);
    std::vector<quint8> temp(pixelCount);
    const uchar *tileBits = tile.bits();
    for (int i = 0; i < pixelCount; ++i)
        buffer[i] = tileBits[i * 4 + 3]; // ARGB32 小端布局：第 4 字节为 alpha

    for (int pass = 0; pass < 2; ++pass) {
        boxBlurPass(buffer, temp, side, side, boxRadius, true);
        boxBlurPass(temp, buffer, side, side, boxRadius, false);
    }

    const int baseAlpha = m_shadowColor.alpha();
    const QRgb rgb = m_shadowColor.rgb();
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            const int alpha = buffer[y * side + x] * baseAlpha / 255;
            tile.setPixel(x, y, qRgba(qRed(rgb), qGreen(rgb), qBlue(rgb), alpha));
        }
    }

    m_shadowTiles[0] = tile;
    m_shadowTiles[1] = tile.mirrored(true, false);
    m_shadowTiles[2] = tile.mirrored(false, true);
    m_shadowTiles[3] = tile.mirrored(true, true);
    m_tileBoxRadius = boxRadius;
    m_tileCornerRadius = m_cornerRadius;
    m_tileColorKey = m_shadowColor.rgba();
}

void AuPopupPanel::animateGeometry(const QRect &to, bool opening)
{
    // 先以 AnimNone 停掉旧动画（finished 回调对 None 状态不做任何事），
    // 再切换到新动画模式，避免 stop() 触发的 finished 误发 shown()/closed()。
    m_animMode = AnimNone;
    m_geometryAnimation->stop();
    m_animMode = opening ? AnimOpening : AnimClosing;
    m_geometryAnimation->setDuration(kAnimDuration);
    m_geometryAnimation->setStartValue(geometry());
    m_geometryAnimation->setEndValue(to);
    m_geometryAnimation->start();
}

void AuPopupPanel::stopAnimation()
{
    m_animMode = AnimNone;
    m_geometryAnimation->stop();
}

QRect AuPopupPanel::collapsedRect(const QRect &finalRect, bool above) const
{
    const int collapsedHeight = qMin(finalRect.height(), 2 * shadowMargin() + 8);
    if (above)
        return QRect(finalRect.left(), finalRect.bottom() - collapsedHeight + 1,
                     finalRect.width(), collapsedHeight);
    return QRect(finalRect.topLeft(), QSize(finalRect.width(), collapsedHeight));
}

QScreen *AuPopupPanel::screenFor(const QWidget *widget)
{
    const QWidget *window = widget ? widget->window() : nullptr;
    if (window && window->windowHandle() && window->windowHandle()->screen())
        return window->windowHandle()->screen();
    if (widget) {
        if (QScreen *screen = QGuiApplication::screenAt(widget->mapToGlobal(widget->rect().center())))
            return screen;
    }
    return QGuiApplication::primaryScreen();
}

int AuPopupPanel::boxBlurRadius(int shadowRadius)
{
    return qMax(1, qRound(shadowRadius * 2.0 / 3.0));
}

int AuPopupPanel::marginForRadius(int shadowRadius)
{
    return 4 * boxBlurRadius(shadowRadius) + 2;
}
