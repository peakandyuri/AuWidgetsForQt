#include "AnnotationCanvas.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>

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

void AnnotationItem::paint(QPainter *p) const
{
    p->save();
    QPen pen(color, penWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    switch (type) {
    case Pen:
        if (points.size() > 1)
            p->drawPolyline(points);
        break;
    case Line:
        if (points.size() >= 2)
            p->drawLine(points.at(0), points.at(1));
        break;
    case Rect:
        if (points.size() >= 2)
            p->drawRect(QRectF(points.at(0), points.at(1)).normalized());
        break;
    case Ellipse:
        if (points.size() >= 2)
            p->drawEllipse(QRectF(points.at(0), points.at(1)).normalized());
        break;
    case Text: {
        QFont f = p->font();
        f.setPixelSize(fontSize);
        f.setBold(true);
        p->setFont(f);
        p->setPen(QPen(color));
        p->drawText(QPointF(points.at(0).x(), points.at(0).y() + fontSize), text);
        break;
    }
    }
    p->restore();
}

AnnotationCanvas::AnnotationCanvas(const QPixmap &base, QWidget *parent)
    : QWidget(parent)
    , m_base(base)
{
    m_dpr = m_base.devicePixelRatio() > 0 ? m_base.devicePixelRatio() : 1.0;
    setFixedSize(m_base.size() / m_dpr);
    setMouseTracking(true);
}

void AnnotationCanvas::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.drawPixmap(QPointF(0, 0), m_base);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    for (const AnnotationItem &item : m_items)
        item.paint(&p);
    if (m_drawing)
        m_preview.paint(&p);
}

void AnnotationCanvas::mousePressEvent(QMouseEvent *event)
{
    if (m_textEditor)
        finishTextEditor(true);     // 先提交之前输入的文字

    if (event->button() != Qt::LeftButton)
        return;

    const QPointF pos = clampPos(eventPos(event));
    if (m_tool == TextTool) {
        startTextEditor(pos);
        return;
    }

    m_drawing = true;
    m_preview = AnnotationItem();
    switch (m_tool) {
    case PenTool:     m_preview.type = AnnotationItem::Pen; break;
    case LineTool:    m_preview.type = AnnotationItem::Line; break;
    case RectTool:    m_preview.type = AnnotationItem::Rect; break;
    case EllipseTool: m_preview.type = AnnotationItem::Ellipse; break;
    case TextTool:    break;
    }
    m_preview.color = m_color;
    m_preview.penWidth = m_penWidth;
    m_preview.fontSize = m_fontSize;
    m_preview.points.clear();
    m_preview.points << pos << pos;
    update();
}

void AnnotationCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_drawing)
        return;

    const QPointF pos = clampPos(eventPos(event));
    const QRectF old = previewBounds();

    if (m_tool == PenTool) {
        if (!m_preview.points.isEmpty()
                && QLineF(m_preview.points.last(), pos).length() < 1.5)
            return;
        m_preview.points.append(pos);
    } else {
        m_preview.points[1] = pos;
    }

    const QRectF dirty = old.united(previewBounds())
            .adjusted(-m_penWidth - 2, -m_penWidth - 2, m_penWidth + 2, m_penWidth + 2);
    update(dirty.toAlignedRect());
}

void AnnotationCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_drawing || event->button() != Qt::LeftButton)
        return;

    m_drawing = false;
    if (m_tool == PenTool) {
        m_preview.points.append(clampPos(eventPos(event)));
    } else {
        m_preview.points[1] = clampPos(eventPos(event));
    }

    bool ok = false;
    if (m_tool == PenTool)
        ok = m_preview.points.size() >= 2;
    else if (m_preview.points.size() >= 2)
        ok = QLineF(m_preview.points.at(0), m_preview.points.at(1)).length() >= 2.0;

    if (ok)
        commitItem(m_preview);
    else
        update();
}

bool AnnotationCanvas::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_textEditor.data() && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            finishTextEditor(false);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void AnnotationCanvas::startTextEditor(const QPointF &pos)
{
    finishTextEditor(true);

    auto *edit = new QLineEdit(this);
    QFont f = edit->font();
    f.setPixelSize(m_fontSize);
    f.setBold(true);
    edit->setFont(f);
    edit->setStyleSheet(QStringLiteral(
        "QLineEdit { color: %1; background: rgba(255, 255, 255, 220);"
        " border: 1px dashed %1; padding: 0 2px; }").arg(m_color.name()));
    edit->setFixedSize(qMax(120, m_fontSize * 10), m_fontSize + 14);

    // 保证编辑框完整落在画板内
    QPoint topLeft(int(pos.x()), int(pos.y()));
    topLeft.setX(qBound(0, topLeft.x(), width() - edit->width()));
    topLeft.setY(qBound(0, topLeft.y(), height() - edit->height()));
    edit->move(topLeft);

    edit->installEventFilter(this);
    connect(edit, &QLineEdit::editingFinished, this, [this] { finishTextEditor(true); });

    m_textEditor = edit;
    m_textPos = pos;
    edit->show();
    edit->raise();
    edit->setFocus();
}

void AnnotationCanvas::finishTextEditor(bool commit)
{
    if (m_textEditor.isNull())
        return;
    QLineEdit *edit = m_textEditor.data();
    const QString text = edit->text().trimmed();
    m_textEditor.clear();           // 先清指针，避免 focusOut 触发 editingFinished 重入
    edit->deleteLater();

    if (!commit || text.isEmpty())
        return;

    AnnotationItem item;
    item.type = AnnotationItem::Text;
    item.color = m_color;
    item.fontSize = m_fontSize;
    item.text = text;
    item.points << m_textPos;
    commitItem(item);
}

QPointF AnnotationCanvas::clampPos(const QPointF &pos) const
{
    return QPointF(qBound<qreal>(0.0, pos.x(), qreal(width() - 1)),
                   qBound<qreal>(0.0, pos.y(), qreal(height() - 1)));
}

QRectF AnnotationCanvas::previewBounds() const
{
    if (!m_drawing || m_preview.points.isEmpty())
        return QRectF();
    if (m_preview.type == AnnotationItem::Pen)
        return m_preview.points.boundingRect();
    if (m_preview.points.size() >= 2)
        return QRectF(m_preview.points.at(0), m_preview.points.at(1)).normalized();
    return QRectF();
}

void AnnotationCanvas::commitItem(const AnnotationItem &item)
{
    m_items.append(item);
    m_redoStack.clear();
    update();
    emit stateChanged();
    emit contentsChanged();
}

void AnnotationCanvas::undo()
{
    if (m_items.isEmpty() || m_undoUsed >= kMaxUndoSteps)
        return;
    m_redoStack.append(m_items.takeLast());
    ++m_undoUsed;
    update();
    emit stateChanged();
    emit contentsChanged();
}

void AnnotationCanvas::redo()
{
    if (m_redoStack.isEmpty() || m_undoUsed <= 0)
        return;
    m_items.append(m_redoStack.takeLast());
    --m_undoUsed;
    update();
    emit stateChanged();
    emit contentsChanged();
}

bool AnnotationCanvas::canUndo() const
{
    return !m_items.isEmpty() && m_undoUsed < kMaxUndoSteps;
}

bool AnnotationCanvas::canRedo() const
{
    return !m_redoStack.isEmpty();
}

QPixmap AnnotationCanvas::renderedImage() const
{
    QImage out = m_base.toImage();
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.scale(m_dpr, m_dpr);
    for (const AnnotationItem &item : m_items)
        item.paint(&p);
    p.end();

    QPixmap pm = QPixmap::fromImage(out);
    pm.setDevicePixelRatio(m_dpr);
    return pm;
}
