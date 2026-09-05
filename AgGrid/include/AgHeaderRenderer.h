#ifndef AGHEADERRENDERER_H
#define AGHEADERRENDERER_H

#include "AgGrid_global.h"

#include <QPainter>
#include <QColor>
#include <QPoint>
#include <QRect>

class AgColumnDef;
class AgGridModel;
class AgGridStyle;

struct AgHeaderRendererContext
{
    bool hovered = false;
    int column = -1;                 // 该表头节点首个叶子列索引
    int level = 0;                   // 1 起始的层级（与 headerRightClicked 的 level 语义一致）
    bool isGroup = false;
    bool sorted = false;             // 该列是否为当前排序列
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    QPoint headerPos = QPoint(-1, -1); // 鼠标相对表头节点左上角的坐标；无效时为 (-1,-1)
};

class AGGRID_EXPORT AgHeaderRenderer
{
public:
    virtual ~AgHeaderRenderer();

    virtual void paint(QPainter *painter,
                       const QRect &rect,
                       const AgColumnDef &columnDef,
                       int column,
                       int level,
                       const AgGridModel *model,
                       const AgGridStyle *style,
                       const QColor &textColor,
                       const AgHeaderRendererContext &context) const = 0;

    // 鼠标进入/离开/在表头节点内移动。rect 为节点矩形（窗口坐标），headerPos 为相对节点左上角的坐标。
    virtual void headerMouseEnter(int column, int level, const QRect &rect, const QPoint &headerPos) { Q_UNUSED(column) Q_UNUSED(level) Q_UNUSED(rect) Q_UNUSED(headerPos) }
    virtual void headerMouseLeave(int column, int level) { Q_UNUSED(column) Q_UNUSED(level) }
    virtual void headerMouseMove(int column, int level, const QRect &rect, const QPoint &headerPos) { Q_UNUSED(column) Q_UNUSED(level) Q_UNUSED(rect) Q_UNUSED(headerPos) }

    // 返回 true 表示该位置需要显示手型光标。
    virtual bool wantsHoverCursor(int column, int level, const QRect &rect, const QPoint &headerPos) const { Q_UNUSED(column) Q_UNUSED(level) Q_UNUSED(rect) Q_UNUSED(headerPos) return false; }

    // 返回 true 表示鼠标命中了一个可点击区域（如按钮）；此时点击不会触发列头排序。
    virtual bool hitTest(int column, int level, const QRect &rect, const QPoint &headerPos) const { Q_UNUSED(column) Q_UNUSED(level) Q_UNUSED(rect) Q_UNUSED(headerPos) return false; }

    // 鼠标在可点击区域上按下并释放时回调。
    virtual void headerMouseClick(int column, int level, const QRect &rect, const QPoint &headerPos) { Q_UNUSED(column) Q_UNUSED(level) Q_UNUSED(rect) Q_UNUSED(headerPos) }
};

#endif // AGHEADERRENDERER_H
