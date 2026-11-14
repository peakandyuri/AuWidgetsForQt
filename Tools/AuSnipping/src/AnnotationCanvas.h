#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QPixmap>
#include <QPointer>
#include <QPolygonF>
#include <QWidget>

class QLineEdit;

// 标注项（撤销 / 恢复的基本单位）
struct AnnotationItem
{
    enum Type { Pen, Line, Rect, Ellipse, Text };

    Type type = Pen;
    QColor color = Qt::red;
    int penWidth = 3;
    int fontSize = 16;
    QPolygonF points;   // Pen: 轨迹；Line: 两端点；Rect/Ellipse: 包围盒两角；Text: 位置
    QString text;

    void paint(QPainter *painter) const;
};

// 简易画板：在截图上添加标注，支持画笔 / 直线 / 矩形 / 椭圆 / 文字，
// 有限步数（50 步）的撤销与恢复；内容变化时通知外部同步剪贴板
class AnnotationCanvas : public QWidget
{
    Q_OBJECT

public:
    enum Tool { PenTool, LineTool, RectTool, EllipseTool, TextTool };

    explicit AnnotationCanvas(const QPixmap &base, QWidget *parent = nullptr);

    void setTool(Tool tool) { m_tool = tool; }
    void setColor(const QColor &color) { m_color = color; }
    QColor color() const { return m_color; }
    void setPenWidth(int width) { m_penWidth = width; }
    void setFontSize(int size) { m_fontSize = size; }

    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    QPixmap renderedImage() const;
    bool isModified() const { return !m_items.isEmpty(); }

    // 提交一个标注项（内部交互与自检共用）
    void commitItem(const AnnotationItem &item);

signals:
    void stateChanged();        // 撤销 / 恢复可用性变化
    void contentsChanged();     // 画面内容变化（应同步到剪贴板）

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void startTextEditor(const QPointF &pos);
    void finishTextEditor(bool commit);
    QPointF clampPos(const QPointF &pos) const;
    QRectF previewBounds() const;

    QPixmap m_base;
    qreal m_dpr = 1.0;
    Tool m_tool = PenTool;
    QColor m_color = QColor(229, 57, 53);
    int m_penWidth = 3;
    int m_fontSize = 16;

    QList<AnnotationItem> m_items;
    QList<AnnotationItem> m_redoStack;
    int m_undoUsed = 0;
    static const int kMaxUndoSteps = 50;

    bool m_drawing = false;
    AnnotationItem m_preview;
    QPointer<QLineEdit> m_textEditor;
    QPointF m_textPos;
};
