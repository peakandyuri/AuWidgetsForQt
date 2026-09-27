#ifndef HEADERBUTTONRENDERER_H
#define HEADERBUTTONRENDERER_H

#include "AuHeaderRenderer.h"

class AuColumnDef;
class AuGridModel;
class AuGridStyle;

class HeaderButtonRenderer : public AuHeaderRenderer
{
public:
    void paint(QPainter *painter,
               const QRect &rect,
               const AuColumnDef &columnDef,
               int column,
               int level,
               const AuGridModel *model,
               const AuGridStyle *style,
               const QColor &textColor,
               const AuHeaderRendererContext &context) const override;

    bool wantsHoverCursor(int column, int level, const QRect &rect, const QPoint &headerPos) const override;
    bool hitTest(int column, int level, const QRect &rect, const QPoint &headerPos) const override;
    void headerMouseClick(int column, int level, const QRect &rect, const QPoint &headerPos) override;
};

#endif // HEADERBUTTONRENDERER_H
