#pragma once

#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QRectF>
#include <QSet>
#include <QtGui/qwindowdefs.h>   // WId

// 一次截图触发的屏幕快照：整屏位图 + 目标屏裁剪 + 触发瞬间的鼠标光标
struct ScreenGrab
{
    bool valid = false;
    qreal dpr = 1.0;            // 桌面位图的 devicePixelRatio
    QRect screenGeometry;       // 目标屏幕的全局逻辑几何
    QPointF virtualTopLeft;     // 虚拟桌面左上角（全局逻辑坐标）
    QPixmap desktop;            // 整个虚拟桌面（devicePixelRatio 已设置）
    QImage screenImage;         // 目标屏幕区域（设备像素分辨率）
    QPixmap cursor;             // 触发瞬间的光标图像（可能为空）
    QPointF cursorHotspot;      // 光标热点（设备像素）
    QPointF cursorPos;          // 触发瞬间的光标位置（全局逻辑坐标）

    QRectF screenLogicalRect() const { return QRectF(screenGeometry); }
};

// 原生窗口命中结果
struct NativeWindowHit
{
    bool valid = false;
    QRectF logicalGlobal;       // 全局逻辑坐标下的窗口矩形
    QRect deviceDesktop;        // desktop 位图内的设备像素矩形（用于裁剪）
};

class Capturer
{
public:
    // 触发一次抓屏：以光标所在屏幕为目标屏幕，同时抓取触发瞬间的光标图像
    static ScreenGrab grab();

    // 从目标屏幕内按逻辑矩形裁剪（localLogical 相对目标屏幕左上角）
    static QPixmap cropScreenRect(const ScreenGrab &g, const QRectF &localLogical);

    // 按桌面位图内的设备像素矩形裁剪（窗口截图用，保证像素级精确）
    static QPixmap cropDesktopDeviceRect(const ScreenGrab &g, const QRect &deviceRect);

    // 获取指定全局逻辑坐标点下的原生顶层窗口（排除 exclude 中的窗口）
    static NativeWindowHit windowUnderPoint(const ScreenGrab &g,
                                            const QPointF &globalLogical,
                                            const QSet<WId> &exclude);

    // 若 enabled 且光标落在 image 内，把触发瞬间的光标合成进 image。
    // imageGlobalOrigin 为 image 左上角对应的全局逻辑坐标。
    static void composeCursor(QPixmap &image, const ScreenGrab &g, bool enabled,
                              const QPointF &imageGlobalOrigin);
};
