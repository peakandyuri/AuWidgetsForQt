#ifndef AUWINDOW_P_H
#define AUWINDOW_P_H

//
// AuWindow 内部共享实现细节：仅供模块内各 AuWindow*.cpp 编译单元使用，
// 不属于公共 API，不随库对外安装。
//

#include <QPainterPath>
#include <QRectF>

namespace AuWindowDetail {

// 四角独立半径的圆角矩形路径（tl/tr/bl/br 分别为左上/右上/左下/右下）。
inline QPainterPath auRoundedRectPath(const QRectF &r, qreal tl, qreal tr, qreal bl, qreal br)
{
    QPainterPath path;
    const qreal maxR = qMin(r.width(), r.height()) / 2;
    tl = qBound<qreal>(0, tl, maxR);
    tr = qBound<qreal>(0, tr, maxR);
    bl = qBound<qreal>(0, bl, maxR);
    br = qBound<qreal>(0, br, maxR);
    if (tl <= 0 && tr <= 0 && bl <= 0 && br <= 0) {
        path.addRect(r);
        return path;
    }
    path.moveTo(r.left() + tl, r.top());
    path.lineTo(r.right() - tr, r.top());
    if (tr > 0)
        path.arcTo(r.right() - 2 * tr, r.top(), 2 * tr, 2 * tr, 90, -90);
    else
        path.lineTo(r.right(), r.top());
    path.lineTo(r.right(), r.bottom() - br);
    if (br > 0)
        path.arcTo(r.right() - 2 * br, r.bottom() - 2 * br, 2 * br, 2 * br, 0, -90);
    else
        path.lineTo(r.right(), r.bottom());
    path.lineTo(r.left() + bl, r.bottom());
    if (bl > 0)
        path.arcTo(r.left(), r.bottom() - 2 * bl, 2 * bl, 2 * bl, 270, -90);
    else
        path.lineTo(r.left(), r.bottom());
    path.lineTo(r.left(), r.top() + tl);
    if (tl > 0)
        path.arcTo(r.left(), r.top(), 2 * tl, 2 * tl, 180, -90);
    else
        path.lineTo(r.left(), r.top());
    path.closeSubpath();
    return path;
}

// 窗体 1px 描边路径（与 AuWindow::paintEvent 同一形状约定：圆角态用四角
// 独立半径的圆角矩形；直角态用内缩 0.5px 的像素对齐矩形）。AuTitleBar
// 重描标题栏区段的边框线时复用，保证衔接处像素级一致。
inline QPainterPath auWindowBorderPath(const QRectF &r, bool rounded,
                                       qreal tl, qreal tr, qreal bl, qreal br)
{
    if (!rounded)
        return auRoundedRectPath(r.adjusted(0.5, 0.5, -0.5, -0.5), 0, 0, 0, 0);
    return auRoundedRectPath(r, tl, tr, bl, br);
}

} // namespace AuWindowDetail

#endif // AUWINDOW_P_H
