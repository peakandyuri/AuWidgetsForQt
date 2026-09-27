#ifndef AUPOPUPPANEL_H
#define AUPOPUPPANEL_H

#include "AuCommon_global.h"

#include <QWidget>
#include <QPointer>
#include <QImage>
#include <QVariantAnimation>

class QLayout;

/**
 * @brief 带投影效果的通用弹层容器（下拉面板、日期面板等共用的基础部件）。
 *
 * 特性：
 *  - 无边框半透明顶级窗口（Qt::Tool），弹出时不抢焦点：面板与子控件均不获得焦点，
 *    宿主编辑框保持输入状态（Windows 通过 WM_MOUSEACTIVATE 返回 MA_NOACTIVATE，
 *    X11 通过 WA_X11DoNotAcceptFocus，均为系统级通用做法）；
 *  - 面板四周绘制 9-slice 预模糊投影（生成一次缓存，绘制开销低，各平台表现一致）；
 *  - popupOver() 负责定位：优先显示在宿主下方，空间不足自动翻转到上方；
 *    水平方向优先左对齐，右侧越界自动切换右对齐，并整体收敛在屏幕可用区内；
 *  - 显示 / 隐藏带 100ms 展开动画：下方弹出时自上而下展开，上方弹出时自下而上展开，
 *    内容随面板高度增加逐渐显露；
 *  - 自动收起：点击面板与宿主之外的区域、焦点移出宿主子树、宿主窗口失去激活或隐藏、
 *    按下 Esc 时自动收起；点击宿主本身（编辑框、标签等）不会收起，由宿主自行决定。
 *
 * 使用方式：
 * @code
 *   AuPopupPanel *panel = new AuPopupPanel(ownerWidget);
 *   panel->setContentWidget(listWidget);
 *   panel->popupOver(ownerWidget, QSize(200, 200));   // 弹出（动画）
 *   panel->closePopup();                              // 收起（动画）
 * @endcode
 */
class AUCOMMON_EXPORT AuPopupPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AuPopupPanel(QWidget *parent = nullptr);
    ~AuPopupPanel() override;

    /// 设置面板内容部件（面板负责其布局，内容区 = 面板 - 投影边距 - 边框线宽）。
    void setContentWidget(QWidget *content);
    QWidget *contentWidget() const;

    // ---- 外观 ----
    int cornerRadius() const;
    void setCornerRadius(int radius);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &color);

    /// 弹出时是否把内容区宽度拉伸到不小于宿主宽度（默认 true，与编辑框同宽语义）；
    /// false 时内容区保持期望宽度（仍收敛到屏幕可用区内），适合日历等固定宽度面板。
    bool isMatchOwnerWidth() const;
    void setMatchOwnerWidth(bool match);

    /// 按下 Esc 时是否自动收起面板（默认 true）；false 时仅发出 escapePressed()
    /// 并消费按键，由宿主决定收起或返回上一级视图（如日历的年 / 月选择页）。
    bool isEscapeCloses() const;
    void setEscapeCloses(bool closes);

    QColor borderColor() const;                 ///< 无效 QColor = 不绘制边框
    void setBorderColor(const QColor &color);

    QColor shadowColor() const;
    void setShadowColor(const QColor &color);   ///< 使用其 alpha 通道作为投影强度

    int shadowRadius() const;                   ///< 投影模糊强度（像素）
    void setShadowRadius(int radius);

    int gap() const;                            ///< 面板与宿主之间的间距
    void setGap(int gap);

    /// 投影边距：内容区边缘到面板窗口边缘的距离（调用方计算期望尺寸时需加上 2*margin）。
    int shadowMargin() const;

    /// 面板窗口尺寸 = 内容尺寸 + 2 * (shadowMargin + 1)。
    QSize outerSizeForContent(const QSize &contentSize) const;

    // ---- 弹出 / 收起 ----
    bool isPopupVisible() const;
    bool isPlacedAbove() const;                 ///< 当前是否显示在宿主上方

public Q_SLOTS:
    /// 以 owner 为锚点弹出面板。preferredSize 为【内容区】期望尺寸（不含投影边距），
    /// 内容区与 owner 的边缘对齐：matchOwnerWidth 为 true（默认）时宽度不小于 owner
    /// 宽度，为 false 时保持期望宽度（如日历面板）；均不超过所在屏幕可用区；
    /// 高度收敛到所在一侧的可用空间。位置优先下方，空间不足自动翻转到上方。
    void popupOver(QWidget *owner, const QSize &preferredSize);
    /// 收起面板（带收起动画）。
    void closePopup();
    /// 立即隐藏（无动画，用于析构等场景）。
    void dismiss();

Q_SIGNALS:
    void aboutToShow();    ///< 弹出动画开始前
    void shown();          ///< 弹出动画结束后
    void aboutToClose();   ///< 收起动画开始前
    void closed();         ///< 面板已隐藏（收起动画结束后）
    void escapePressed();  ///< 面板打开期间用户按下 Esc（面板会随之收起）

protected:
    void paintEvent(QPaintEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

private:
    bool eventFilter(QObject *watched, QEvent *event) override; ///< 应用级过滤器：自动收起

    void updateLayout();
    void rebuildShadowTiles();
    void animateGeometry(const QRect &to, bool opening);
    void stopAnimation();
    QRect collapsedRect(const QRect &finalRect, bool above) const;
    static QScreen *screenFor(const QWidget *widget);
    static int boxBlurRadius(int shadowRadius);   ///< 盒式模糊半径
    static int marginForRadius(int shadowRadius); ///< 投影边距

    QPointer<QWidget> m_owner;
    QPointer<QWidget> m_ownerWindow;
    QWidget *m_content;
    QLayout *m_layout;

    QColor m_backgroundColor;
    QColor m_borderColor;
    QColor m_shadowColor;
    int m_cornerRadius;
    int m_shadowRadius;
    int m_gap;
    bool m_placedAbove;
    bool m_matchOwnerWidth;
    bool m_escapeCloses;

    QImage m_shadowTiles[4];    ///< 9-slice 投影角块缓存（左上 / 右上 / 左下 / 右下）
    int m_tileBoxRadius;        ///< 生成缓存时的参数（变化后重建）
    int m_tileCornerRadius;
    QRgb m_tileColorKey;

    QVariantAnimation *m_geometryAnimation;
    enum AnimationMode { AnimNone, AnimOpening, AnimClosing };
    AnimationMode m_animMode;
    bool m_eventFilterInstalled;

    static constexpr int kAnimDuration = 100;
};

#endif // AUPOPUPPANEL_H
