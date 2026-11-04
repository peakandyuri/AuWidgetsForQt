#include "AuBorderShadow.h"
#include "AuWindow_p.h"

#include <QPainter>
#include <QPainterPath>

// Qt 私有导出函数（Q_WIDGETS_EXPORT，Qt5/Qt6 均由 Qt5Widgets/Qt6Widgets
// 导出）：QGraphicsBlurEffect 的模糊内核。peony 的 BorderShadowEffect 与
// qt5-ukui-platformtheme 的 ConfigShadowHelper 皆以同一 extern 声明方式
// 使用，此处保持一致（无需 widgets-private 头文件与额外的 qmake 配置）。
extern void qt_blurImage(QImage &blurImage, qreal radius, bool quality, int transposed);

namespace AuWindowDetail {

QImage auRenderBorderShadowTile(const QColor &color, int blur,
                                int radiusTopLeft, int radiusTopRight,
                                int radiusBottomLeft, int radiusBottomRight,
                                qreal dpr)
{
    if (blur <= 0 || !color.isValid() || color.alpha() <= 0 || dpr <= 0)
        return QImage();

    dpr = qBound(1.0, dpr, 3.0);
    const int maxRadius = qMax(qMax(radiusTopLeft, radiusTopRight),
                               qMax(radiusBottomLeft, radiusBottomRight));

    // 图幅（逻辑像素）：2*maxRadius + 2*blur + 1（内窗矩形 2*maxRadius+1
    // + 两侧渐隐环 blur，+1 为中心拉伸条），全部按 dpr 放大为设备像素。
    // 与 ukui ConfigShadowHelper 的图幅公式一致。
    // 注意：QPainter 绘制进 QImage 不应用 devicePixelRatio 变换，本函数
    // 全程以设备像素坐标工作；返回图像不设 DPR 标志（原始位图），由调用
    // 方按 dpr 换算（客户端绘制）或直接上传位图（X11 WM 合成通道）。
    const int size = qRound((2 * maxRadius + 2 * blur + 1) * dpr);
    QImage tile(size, size, QImage::Format_ARGB32_Premultiplied);
    if (tile.isNull())
        return QImage();
    tile.fill(Qt::transparent);

    // 内部窗口矩形（设备像素）：位于 (blur, blur)·dpr，边长
    // (2*maxRadius + 1)·dpr，四角独立半径的圆角形状与窗体一致
    const QRectF inner(blur * dpr, blur * dpr,
                       (2 * maxRadius + 1) * dpr, (2 * maxRadius + 1) * dpr);
    const QPainterPath windowPath = auRoundedRectPath(
        inner, radiusTopLeft * dpr, radiusTopRight * dpr,
        radiusBottomLeft * dpr, radiusBottomRight * dpr);

    // 1) 填阴影色（peony：p.fillPath(contentPath, m_shadow_color)）
    {
        QPainter p(&tile);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPath(windowPath);
        p.end();
    }

    // 2) 高斯模糊（ukui 同款 quality=true；图幅极小，一次成本可忽略）
    qt_blurImage(tile, blur * dpr, true, false);

    // 3) 清除窗口矩形内部（ukui 同款 CompositionMode_Clear）：窗口内容
    //    自身不透明会盖住该区域，但圆角弧外的透明像素要让阴影透出来，
    //    不能被模糊后的实心色占据
    {
        QPainter p(&tile);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::transparent);
        p.drawPath(windowPath);
        p.end();
    }

    return tile;
}

} // namespace AuWindowDetail
