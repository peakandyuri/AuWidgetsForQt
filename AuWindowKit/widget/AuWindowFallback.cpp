#include "AuWindow.h"
#include "AuWindow_p.h"

#include <QMouseEvent>
#include <QWindow>

// ============================================================================
// AuWindow 的其它平台实现（macOS 等 Qt 桌面平台）。窗口为纯 Qt 无边框 +
// 手动拖动 / 缩放兜底路径，无原生扩展；AuWindow.cpp 只保留跨平台逻辑
// （经 platformXxx() 钩子进入本文件）。Windows / Linux 编译本文件为空
// 翻译单元（分别由 AuWindowWin.cpp / AuWindowX11.cpp 接管）。
// ============================================================================

#if !defined(Q_OS_WIN) && !defined(Q_OS_LINUX)

// ---- 平台钩子 ----

void AuWindow::platformInit()
{
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    m_client = new QWidget(this);
    setupEdgeGrips();
}

void AuWindow::platformWinIdChanged() {}

void AuWindow::platformFirstShow() {}

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
    Q_UNUSED(visible)
    return false;
}

bool AuWindow::platformStartSystemMove()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    if (m_systemMoveEnabled && windowHandle() && windowHandle()->startSystemMove()) {
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
    Q_UNUSED(edges)
    return false;
#endif
}

void AuWindow::platformMouseMoveEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
}

bool AuWindow::platformIsComposited() const
{
    return true;
}

bool AuWindow::platformClipsWindowShape() const
{
    return false;
}

// ---- Windows 专属接口的空实现 ----

void AuWindow::applyWindowStyles() {}
void AuWindow::applyWindowRoundedCorners() {}
void AuWindow::applyTaskbarVisibility() {}

// ---- mask / 几何 ----

void AuWindow::updateMask()
{
    const bool noMaskNeeded = windowRadius() <= 0 || isMaximized() || isFullScreen();
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
    move(pos);
}

void AuWindow::startGrabRecoveryPoll() {}
void AuWindow::pollGrabRecovery() {}

bool AuWindow::nativeEvent(const QByteArray &eventType, void *message, AuNativeEventResult *result)
{
    return QWidget::nativeEvent(eventType, message, result);
}

#endif // !Q_OS_WIN && !Q_OS_LINUX
