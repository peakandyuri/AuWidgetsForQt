#ifndef HEADERBUTTONRENDERER_H
#define HEADERBUTTONRENDERER_H

#include "AgHeaderRenderer.h"

class AgColumnDef;
class AgGridModel;
class AgGridStyle;

class HeaderButtonRenderer : public AgHeaderRenderer
{
public:
    void paint(QPainter *painter,
               const QRect &rect,
               const AgColumnDef &columnDef,
               int column,
               int level,
               const AgGridModel *model,
               const AgGridStyle *style,
               const QColor &textColor,
               const AgHeaderRendererContext &context) const override;

    bool wantsHoverCursor(int column, int level, const QRect &rect, const QPoint &headerPos) const override;
    bool hitTest(int column, int level, const QRect &rect, const QPoint &headerPos) const override;
    void headerMouseClick(int column, int level, const QRect &rect, const QPoint &headerPos) override;
};

#endif // HEADERBUTTONRENDERER_H
