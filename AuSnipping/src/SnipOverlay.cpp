#include "SnipOverlay.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QtMath>

namespace {

QPointF eventPos(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->localPos();
#endif
}

} // namespace

SnipOverlay::SnipOverlay(const ScreenGrab &grab, Mode mode, bool captureCursor, QWidget *parent)
    : QWidget(parent)
    , m_grab(grab)
    , m_mode(mode)
    , m_captureCursor(captureCursor)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose);
    // 关键：会话期间主窗口处于隐藏状态，遮罩/编辑窗是仅有的可见顶层窗口，
    // 若保留 Qt 默认的 WA_QuitOnClose，关闭遮罩会把整个程序退出
    setAttribute(Qt::WA_QuitOnClose, false);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setGeometry(m_grab.screenGeometry);

    m_dpr = m_grab.dpr;

    // 窗口模式下排除本进程所有可见顶层窗口（遮罩层、启动器、倒计时标签）
    const auto tops = QApplication::topLevelWidgets();
    for (QWidget *w : tops) {
        if (w && w->isVisible())
            m_exclude.insert(w->winId());
    }
    m_exclude.insert(winId());

    m_queryTimer.start();
}

void SnipOverlay::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);

    // 冻结快照 1:1 铺满（screenImage 为设备像素分辨率）
    p.drawImage(QRectF(rect()), m_grab.screenImage,
                QRectF(0, 0, m_grab.screenImage.width(), m_grab.screenImage.height()));

    // 半透明遮罩
    p.fillRect(rect(), QColor(0, 0, 0, 110));

    if (m_sel.isNull() || m_sel.isEmpty()) {
        drawHint(p);
        return;
    }

    // 框内完全透明：把快照对应区域按原色重绘
    const QRectF devSrc(m_sel.left() * m_dpr, m_sel.top() * m_dpr,
                        m_sel.width() * m_dpr, m_sel.height() * m_dpr);
    p.drawImage(m_sel, m_grab.screenImage, devSrc);

    // 边框
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(m_mode == WindowMode ? QColor(0, 188, 212) : QColor(33, 150, 243),
                  m_mode == WindowMode ? 2 : 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(m_sel);

    drawSizeLabel(p, m_sel);
    if (m_mode == RegionMode) {
        drawHandles(p);
        if (m_toolbarVisible)
            drawToolbar(p);
    }
}

void SnipOverlay::mousePressEvent(QMouseEvent *event)
{
    const QPointF pos = eventPos(event);

    if (event->button() == Qt::RightButton) {
        cancel();
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;

    if (m_mode == WindowMode) {
        if (m_state == Fixed && !m_sel.isEmpty())
            finish();
        return;
    }

    if (m_state == Fixed) {
        if (m_toolbarVisible && m_confirmRect.contains(pos)) {
            finish();
            return;
        }
        if (m_toolbarVisible && m_cancelRect.contains(pos)) {
            cancel();
            return;
        }

        const Handle h = handleAt(pos);
        if (h != HandleNone) {
            m_handle = h;
            m_state = Adjusting;
            m_origSel = m_sel;
            m_toolbarVisible = false;
            update();
        } else if (m_sel.contains(pos)) {
            m_handle = HandleNone;          // Adjusting 且无手柄 = 移动整框
            m_pressPos = pos;
            m_origSel = m_sel;
            m_state = Adjusting;
            m_toolbarVisible = false;
            update();
        } else {
            m_anchorPos = pos;              // 框外重新起一个选区
            m_sel = QRectF(pos, pos);
            m_state = Creating;
            m_toolbarVisible = false;
            update();
        }
    } else if (m_state == NoSelection) {
        m_anchorPos = pos;
        m_sel = QRectF(pos, pos);
        m_state = Creating;
        update();
    }
}

void SnipOverlay::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF pos = eventPos(event);

    if (m_mode == WindowMode) {
        updateWindowHighlight(pos);
        return;
    }

    switch (m_state) {
    case Creating: {
        const QRectF old = m_sel;
        m_sel = clampToWidget(QRectF(m_anchorPos, clampPos(pos)).normalized());
        scheduleUpdate(old, m_sel);
        break;
    }
    case Adjusting: {
        const QRectF old = m_sel;
        if (m_handle == HandleNone)
            m_sel = movedSelection(pos);
        else
            m_sel = adjustedSelection(m_handle, pos);
        scheduleUpdate(old, m_sel);
        break;
    }
    default:
        updateHover(pos);
        break;
    }
}

void SnipOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_mode == WindowMode || event->button() != Qt::LeftButton)
        return;

    if (m_state == Creating) {
        if (m_sel.width() * m_dpr >= 4 && m_sel.height() * m_dpr >= 4) {
            m_state = Fixed;
            computeToolbar();
            m_toolbarVisible = true;
            update();
        } else {
            m_sel = QRectF();
            m_state = NoSelection;
            update();
        }
    } else if (m_state == Adjusting) {
        m_state = Fixed;
        computeToolbar();
        m_toolbarVisible = true;
        update();
    }
}

void SnipOverlay::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_mode == RegionMode && event->button() == Qt::LeftButton && m_state == Fixed
            && !m_sel.isEmpty())
        finish();
}

void SnipOverlay::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();
    if (key == Qt::Key_Escape) {
        cancel();
        return;
    }
    if (key == Qt::Key_Enter || key == Qt::Key_Return) {
        if (m_mode == WindowMode) {
            if (m_state == Fixed && !m_sel.isEmpty())
                finish();
        } else if (!m_sel.isNull() && m_sel.width() * m_dpr >= 4
                   && m_sel.height() * m_dpr >= 4) {
            finish();
        }
    }
}

void SnipOverlay::showEvent(QShowEvent * /*event*/)
{
    activateWindow();
    raise();
    grabKeyboard();

    // 鼠标可能静止在某个窗口上，未触发 move 事件时主动查询一次
    if (m_mode == WindowMode)
        QTimer::singleShot(30, this, [this] {
            updateWindowHighlight(mapFromGlobal(QCursor::pos()));
        });
}

void SnipOverlay::hideEvent(QHideEvent * /*event*/)
{
    releaseKeyboard();
}

void SnipOverlay::finish()
{
    QPixmap image;
    if (m_mode == WindowMode && !m_nativeDevice.isEmpty())
        image = Capturer::cropDesktopDeviceRect(m_grab, m_nativeDevice);
    else
        image = Capturer::cropScreenRect(m_grab, m_sel);

    Capturer::composeCursor(image, m_grab, m_captureCursor, m_grab.screenGeometry.topLeft());
    emit captured(image);
    // 只隐藏不销毁：保留冻结画面与选区，供编辑窗口关闭后返回继续“连续截图”；
    // 仅当用户取消（Esc / 右键 / “取消”）时才走 cancel() → close() 销毁
    hide();
}

void SnipOverlay::cancel()
{
    emit cancelled();
    close();
}

void SnipOverlay::updateHover(const QPointF &pos)
{
    int newHover = 0;
    if (m_toolbarVisible && m_state == Fixed) {
        if (m_confirmRect.contains(pos))
            newHover = 1;
        else if (m_cancelRect.contains(pos))
            newHover = 2;
    }

    Handle newHandle = HandleNone;
    bool inside = false;
    if (m_state == Fixed && newHover == 0) {
        newHandle = handleAt(pos);
        if (newHandle == HandleNone && m_sel.contains(pos))
            inside = true;
    }

    const bool changed = (newHover != m_hoverToolbar) || (newHandle != m_hoverHandle);
    m_hoverToolbar = newHover;
    m_hoverHandle = newHandle;

    if (newHover != 0)
        setCursor(Qt::PointingHandCursor);
    else if (newHandle != HandleNone)
        switch (newHandle) {
        case HandleTopLeft:
        case HandleBottomRight: setCursor(Qt::SizeFDiagCursor); break;
        case HandleTopRight:
        case HandleBottomLeft:  setCursor(Qt::SizeBDiagCursor); break;
        case HandleTop:
        case HandleBottom:      setCursor(Qt::SizeVerCursor); break;
        case HandleLeft:
        case HandleRight:       setCursor(Qt::SizeHorCursor); break;
        default: break;
        }
    else if (inside)
        setCursor(Qt::SizeAllCursor);
    else
        setCursor(Qt::CrossCursor);

    if (changed)
        update();
}

void SnipOverlay::updateWindowHighlight(const QPointF &localPos)
{
    if (m_queryTimer.isValid() && m_queryTimer.elapsed() < 12)
        return;
    m_queryTimer.restart();

    const QPointF global = QPointF(mapToGlobal(localPos.toPoint()));
    const NativeWindowHit hit = Capturer::windowUnderPoint(m_grab, global, m_exclude);

    QRectF newSel;
    QRect newDevice;
    if (hit.valid) {
        newDevice = hit.deviceDesktop;
        newSel = hit.logicalGlobal.translated(-m_grab.screenGeometry.topLeft());
        newSel = clampToWidget(newSel);
        if (newSel.isEmpty()) {
            newSel = QRectF();
            newDevice = QRect();
        }
    }

    if (newSel != m_sel || newDevice != m_nativeDevice) {
        const QRectF old = m_sel;
        m_sel = newSel;
        m_nativeDevice = newDevice;
        m_state = newSel.isEmpty() ? NoSelection : Fixed;
        scheduleUpdate(old, m_sel);
    }
}

SnipOverlay::Handle SnipOverlay::handleAt(const QPointF &pos) const
{
    if (m_sel.isNull() || m_sel.isEmpty())
        return HandleNone;

    const qreal tol = 6.0;
    const bool atL = qAbs(pos.x() - m_sel.left()) <= tol;
    const bool atR = qAbs(pos.x() - m_sel.right()) <= tol;
    const bool atT = qAbs(pos.y() - m_sel.top()) <= tol;
    const bool atB = qAbs(pos.y() - m_sel.bottom()) <= tol;
    const bool withinX = pos.x() >= m_sel.left() - tol && pos.x() <= m_sel.right() + tol;
    const bool withinY = pos.y() >= m_sel.top() - tol && pos.y() <= m_sel.bottom() + tol;

    if (atT && atL && withinX && withinY) return HandleTopLeft;
    if (atT && atR && withinX && withinY) return HandleTopRight;
    if (atB && atL && withinX && withinY) return HandleBottomLeft;
    if (atB && atR && withinX && withinY) return HandleBottomRight;
    if (atT && withinX) return HandleTop;
    if (atB && withinX) return HandleBottom;
    if (atL && withinY) return HandleLeft;
    if (atR && withinY) return HandleRight;
    return HandleNone;
}

QRectF SnipOverlay::adjustedSelection(Handle h, const QPointF &rawPos) const
{
    const QPointF pos = clampPos(rawPos);
    QRectF r = m_origSel;
    switch (h) {
    case HandleTopLeft:     r.setTopLeft(pos);                 break;
    case HandleTop:         r.setTop(pos.y());                 break;
    case HandleTopRight:    r.setTopRight(pos);                break;
    case HandleRight:       r.setRight(pos.x());               break;
    case HandleBottomRight: r.setBottomRight(pos);             break;
    case HandleBottom:      r.setBottom(pos.y());              break;
    case HandleBottomLeft:  r.setBottomLeft(pos);              break;
    case HandleLeft:        r.setLeft(pos.x());                break;
    default: break;
    }
    return clampToWidget(r.normalized());
}

QRectF SnipOverlay::movedSelection(const QPointF &pos) const
{
    QRectF r = m_origSel.translated(pos - m_pressPos);
    if (r.left() < 0)
        r.translate(-r.left(), 0);
    if (r.top() < 0)
        r.translate(0, -r.top());
    if (r.right() > width())
        r.translate(width() - r.right(), 0);
    if (r.bottom() > height())
        r.translate(0, height() - r.bottom());
    return r;
}

QRectF SnipOverlay::clampToWidget(const QRectF &rect) const
{
    return rect.intersected(QRectF(this->rect()));
}

QPointF SnipOverlay::clampPos(const QPointF &pos) const
{
    return QPointF(qBound<qreal>(0.0, pos.x(), width()),
                   qBound<qreal>(0.0, pos.y(), height()));
}

void SnipOverlay::computeToolbar()
{
    const QSizeF btn(64, 26);
    const qreal gap = 8.0;
    const qreal totalW = btn.width() * 2 + gap;

    qreal x = m_sel.right() - totalW;
    x = qBound<qreal>(8.0, x, width() - totalW - 8.0);

    qreal y = m_sel.bottom() + 10.0;
    if (y + btn.height() + 8.0 > height())
        y = m_sel.bottom() - btn.height() - 10.0;   // 下方放不下：移到框内

    m_confirmRect = QRectF(QPointF(x, y), btn);
    m_cancelRect = QRectF(QPointF(x + btn.width() + gap, y), btn);
}

QRectF SnipOverlay::labelBoxFor(const QRectF &sel, const QString &text, const QFont &font) const
{
    const QFontMetricsF fm(font);
    QRectF box = fm.boundingRect(text).adjusted(-8, -4, 8, 4);
    const qreal margin = 6.0;

    // 横向：与选区左缘对齐，越出屏幕时自动收进屏幕（等效“调整到框外右侧”）
    qreal x = qBound<qreal>(margin, sel.left(), width() - box.width() - margin);

    // 纵向：框外上方 → 框内 → 框外下方 → 框内兜底
    qreal y = sel.top() - box.height() - margin;
    if (y < margin) {
        if (sel.height() >= box.height() + 2 * margin)
            y = sel.top() + margin;
        else {
            y = sel.bottom() + margin;
            if (y + box.height() > height() - margin)
                y = sel.top() + margin;
        }
    }
    box.moveTo(x, y);
    return box;
}

void SnipOverlay::drawSizeLabel(QPainter &p, const QRectF &sel) const
{
    const int wDev = qRound(sel.width() * m_dpr);
    const int hDev = qRound(sel.height() * m_dpr);
    const QString text = QStringLiteral("%1 × %2").arg(wDev).arg(hDev);

    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(10);

    p.save();
    p.setFont(f);
    const QRectF box = labelBoxFor(sel, text, f);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 180));
    p.drawRoundedRect(box, 4, 4);
    p.setPen(Qt::white);
    p.drawText(box, Qt::AlignCenter, text);
    p.restore();
}

void SnipOverlay::drawHandles(QPainter &p) const
{
    if (m_sel.isNull() || m_sel.isEmpty())
        return;

    const qreal half = 4.0;
    const QPointF centers[8] = {
        QPointF(m_sel.left(), m_sel.top()),
        QPointF(m_sel.center().x(), m_sel.top()),
        QPointF(m_sel.right(), m_sel.top()),
        QPointF(m_sel.right(), m_sel.center().y()),
        QPointF(m_sel.right(), m_sel.bottom()),
        QPointF(m_sel.center().x(), m_sel.bottom()),
        QPointF(m_sel.left(), m_sel.bottom()),
        QPointF(m_sel.left(), m_sel.center().y())
    };

    p.save();
    p.setPen(QPen(Qt::black, 1));
    p.setBrush(Qt::white);
    for (const QPointF &c : centers)
        p.drawRect(QRectF(c.x() - half, c.y() - half, half * 2, half * 2));
    p.restore();
}

void SnipOverlay::drawToolbar(QPainter &p) const
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);

    auto drawBtn = [&](const QRectF &r, const QString &text, const QColor &base, bool hovered) {
        QColor bg = base;
        if (hovered)
            bg = bg.lighter(120);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(r, 4, 4);
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setBold(true);
        f.setPointSize(10);
        p.setFont(f);
        p.drawText(r, Qt::AlignCenter, text);
    };

    drawBtn(m_confirmRect, QStringLiteral("确定"), QColor(33, 150, 243), m_hoverToolbar == 1);
    drawBtn(m_cancelRect, QStringLiteral("取消"), QColor(60, 60, 60, 230), m_hoverToolbar == 2);
    p.restore();
}

void SnipOverlay::drawHint(QPainter &p) const
{
    const QString text = m_mode == RegionMode
        ? QStringLiteral("按住鼠标拖动框选截图区域\nEnter / 双击 确认    Esc 取消")
        : QStringLiteral("移动鼠标选择窗口，点击确认    Esc 取消");

    QFont f = p.font();
    f.setPointSize(12);
    const QFontMetricsF fm(f);
    QRectF box = fm.boundingRect(QRectF(), Qt::AlignCenter, text).adjusted(-16, -10, 16, 10);
    box.moveCenter(QPointF(width() / 2.0, 80.0));

    p.save();
    p.setFont(f);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 150));
    p.drawRoundedRect(box, 6, 6);
    p.setPen(Qt::white);
    p.drawText(box, Qt::AlignCenter, text);
    p.restore();
}

void SnipOverlay::scheduleUpdate(const QRectF &oldArea, const QRectF &newArea)
{
    // 余量覆盖宽高标注、手柄与工具条
    QRectF area = oldArea.united(newArea).adjusted(-80, -80, 80, 80);
    const QRect dirty = area.toAlignedRect().intersected(rect());
    if (!dirty.isEmpty())
        update(dirty);
    else
        update();
}
