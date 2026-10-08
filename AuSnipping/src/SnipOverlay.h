#pragma once

#include <QElapsedTimer>
#include <QWidget>

#include "Capturer.h"

// 截图遮罩层：覆盖目标屏幕，显示冻结的屏幕快照。
//
// 区域模式（RegionMode）：
//   - 按住拖动框选，选区外部半透明、内部完全透明（显示快照原色）
//   - 选区四周/四角可调整大小，框内可拖动整框
//   - 框左上角附近显示宽高，位置自适应（框外上方 → 框内 → 框下方 → 框内，
//     横向越界时自动收进屏幕）
//   - 松开后出现“确定 / 取消”工具条；Enter、双击确认，Esc、右键取消
//
// 窗口模式（WindowMode）：
//   - 移动鼠标自动捕获光标下的原生顶层窗口并高亮，点击确认，不可调整大小
//
// 延迟截图（delaySeconds > 0，仅区域/窗口模式）：
//   - 确认选区后立即关闭遮罩回到实时画面，由启动器倒计时，
//     到点重抓实时画面并按锁定的选区裁剪（PendingCrop 描述选区）
class SnipOverlay : public QWidget
{
    Q_OBJECT

public:
    enum Mode { RegionMode, WindowMode };

    // 延迟截图时传给启动器的选区信息：倒计时结束后按它对实时画面重抓裁剪
    struct PendingCrop {
        QScreen *screen = nullptr;  // 目标屏幕
        bool windowMode = false;    // true 按 windowDevice 裁剪，否则按 localRect
        QRect windowDevice;         // 桌面位图内的设备像素矩形（窗口截图）
        QRectF localRect;           // 相对目标屏幕左上角的逻辑矩形（区域截图）
    };

    SnipOverlay(const ScreenGrab &grab, Mode mode, bool captureCursor,
                int delaySeconds = 0, QWidget *parent = nullptr);

    PendingCrop pendingCrop() const { return m_pending; }

signals:
    void captured(const QPixmap &image);
    void cancelled();
    // 延迟模式下确认选区：发出后遮罩层自行关闭，选区经 pendingCrop() 取走
    void delayedConfirm();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    enum State { NoSelection, Creating, Adjusting, Fixed };
    enum Handle {
        HandleNone = 0,
        HandleTopLeft, HandleTop, HandleTopRight, HandleRight,
        HandleBottomRight, HandleBottom, HandleBottomLeft, HandleLeft
    };

    void finish();
    void cancel();
    void updateHover(const QPointF &pos);
    void updateWindowHighlight(const QPointF &localPos);
    Handle handleAt(const QPointF &pos) const;
    QRectF adjustedSelection(Handle h, const QPointF &rawPos) const;
    QRectF movedSelection(const QPointF &pos) const;
    QRectF clampToWidget(const QRectF &rect) const;
    QPointF clampPos(const QPointF &pos) const;
    void computeToolbar();
    QRectF labelBoxFor(const QRectF &sel, const QString &text, const QFont &font) const;
    void drawSizeLabel(QPainter &p, const QRectF &sel) const;
    void drawHandles(QPainter &p) const;
    void drawToolbar(QPainter &p) const;
    void drawHint(QPainter &p) const;
    void scheduleUpdate(const QRectF &oldArea, const QRectF &newArea);

    const ScreenGrab m_grab;
    const Mode m_mode;
    const bool m_captureCursor;
    const int m_delaySeconds;
    qreal m_dpr = 1.0;

    PendingCrop m_pending;      // 延迟模式下确认选区时填写

    State m_state = NoSelection;
    QRectF m_sel;               // 选区（widget 本地逻辑坐标，widget 与目标屏幕重合）
    QRect m_nativeDevice;       // 窗口模式命中的桌面设备像素矩形
    Handle m_handle = HandleNone;
    QPointF m_pressPos;
    QPointF m_anchorPos;
    QRectF m_origSel;

    QRectF m_confirmRect;
    QRectF m_cancelRect;
    bool m_toolbarVisible = false;
    int m_hoverToolbar = 0;     // 0 无 / 1 确定 / 2 取消
    Handle m_hoverHandle = HandleNone;

    QSet<WId> m_exclude;        // 窗口模式下排除的本进程窗口
    QElapsedTimer m_queryTimer; // 窗口查询限频
};
