#ifndef AUBUTTONCELLRENDERER_H
#define AUBUTTONCELLRENDERER_H

#include "AuCellRenderer.h"
#include "AuCellButton.h"

#include <functional>
#include <QFontMetrics>
#include <QVector>

class AuGridModel;

/**
 * @brief 内置按钮单元格渲染器：一格绘制多个按钮 / 文字链接，热区点击。
 *
 * 注册名 "button" / "link" 已由 AuGridWidget 内置（同名注册可覆盖），
 * 也可自行创建实例配置后注册。操作列建议 AuColumnDef::setFocusable(false)。
 *
 * 按钮来源（优先级从高到低）：
 *  1. setButtonProvider 按行回调 —— 每格生成按钮列表，可按行状态出不同按钮；
 *  2. setButtons 固定列表 —— 整列同一组按钮；
 *  3. 单元格值解析 —— 未配置上两者时：QString 视为单按钮文字，
 *     QVariantMap / QVariantList（元素为 QString 或 map，键同 AuCellButton::toMap()）
 *     直接解析为按钮定义，适合后端下发按钮配置。
 *
 * 交互：按钮热区按下高亮、移出取消、释放确认后经 AuGridWidget::cellButtonClicked
 * 信号统一发出（无需逐按钮绑定回调）。hover / pressed 样式未设置时由 normal
 * 自动派生（提亮 / 加深）；Link 类型为透明背景文字链接，hover 显示下划线。
 * 列宽不足时按钮被裁剪，热区仅限可见部分。
 */
class AUGRID_EXPORT AuButtonCellRenderer : public AuCellRenderer
{
public:
    /// 按行按钮回调：返回该格按钮列表（空列表 = 该格不显示按钮）。
    using ButtonProvider = std::function<QVector<AuCellButton>(int row, int column, const AuGridModel *model)>;

    /// 构造；defaultType 为数据驱动解析时按钮的默认形态（内置 "link" 实例传 Link）。
    explicit AuButtonCellRenderer(AuCellButton::Type defaultType = AuCellButton::Type::Button);

    /// 整列固定按钮集（优先级低于 setButtonProvider）。
    void setButtons(const QVector<AuCellButton> &buttons);
    /// 按行按钮回调（优先级最高）。
    void setButtonProvider(const ButtonProvider &provider);

    /// 按钮间距（默认 8px）。
    void setSpacing(int spacing);
    /// 按钮高度（默认 22px，垂直居中于单元格）。
    void setButtonHeight(int height);
    /// 按钮左右内边距（默认 -1 = Button 10px / Link 4px）。
    void setHorizontalPadding(int padding);
    /// Button 圆角半径（默认 4px；按钮自身 radius >= 0 时优先）。
    void setRadius(int radius);
    /// Link 与未设色按钮的默认文字色（默认 #1677FF）。
    void setDefaultColor(const QColor &color);

    void paint(QPainter *painter,
               const QRect &rect,
               const QVariant &value,
               const AuColumnDef &columnDef,
               int row,
               int column,
               const AuGridModel *model,
               const AuGridStyle *style,
               const QColor &textColor) const override;

    void paint(QPainter *painter,
               const QRect &rect,
               const QVariant &value,
               const AuColumnDef &columnDef,
               int row,
               int column,
               const AuGridModel *model,
               const AuGridStyle *style,
               const QColor &textColor,
               const AuCellRendererContext &context) const override;

    /// 悬停在按钮热区上时显示手型光标（依赖 cellMouseMove 记录的热区索引）。
    bool wantsHoverCursor(int row, int column, const QPoint &cellPos) const override;

    /// 需要格内移动重绘以驱动按钮 hover 变色。
    bool wantsMouseMoveUpdates() const override;

    void cellMouseLeave(int row, int column) override;
    using AuCellRenderer::cellMouseMove;
    void cellMouseMove(int row, int column, const AuCellInteractionContext &context) override;
    bool cellMousePress(int row, int column, const AuCellInteractionContext &context) override;
    QVariant cellMouseRelease(int row, int column, const AuCellInteractionContext &context) override;

    /// 返回各按钮文字（空格连接）。
    QVariant copyContent(const QVariant &value,
                         const AuColumnDef &columnDef,
                         int row,
                         int column,
                         const AuGridModel *model) const override;

private:
    /// 解析该格按钮列表（按行回调 > 固定列表 > 单元格值解析）。
    QVector<AuCellButton> buttonsFor(const QVariant &value, int row, int column, const AuGridModel *model) const;
    /// 按对齐排布按钮矩形（垂直居中；总宽超出列宽时从左侧顺序排布、由裁剪兜底）。
    QVector<QRect> buttonRects(const QVector<AuCellButton> &buttons,
                               const QRect &rect,
                               const QFontMetrics &fontMetrics,
                               Qt::Alignment alignment) const;
    /// 单个按钮宽度（文字宽 + 左右内边距）。
    int buttonWidth(const AuCellButton &button, const QFontMetrics &fontMetrics) const;
    /// 交互命中的字体度量（context.font，缺省应用字体）。
    QFontMetrics interactionFontMetrics(const AuCellInteractionContext &context) const;
    /// 交互命中用：以单元格本地坐标排布按钮矩形（与 paint 布局一致）。
    QVector<QRect> hitTestRects(const AuCellInteractionContext &context,
                                const QVector<AuCellButton> &buttons) const;
    /// 绘制单个按钮（按 hover / pressed / disabled 解析最终三态样式）。
    void drawButton(QPainter *painter,
                    const AuCellButton &button,
                    const QRect &rect,
                    const QFontMetrics &fontMetrics,
                    const QColor &defaultTextColor,
                    bool hovered,
                    bool pressed) const;
    /// pos 命中的按钮索引（未命中返回 -1；仅可见部分参与命中）。
    int buttonIndexAt(const QVector<AuCellButton> &buttons,
                      const QVector<QRect> &rects,
                      const QPoint &cellPos) const;

    AuCellButton::Type m_defaultType;
    QVector<AuCellButton> m_buttons;
    ButtonProvider m_provider;
    int m_spacing = 8;          ///< 按钮间距
    int m_buttonHeight = 22;    ///< 按钮高度
    int m_hPadding = -1;        ///< 按钮左右内边距（-1 = 按类型默认）
    int m_radius = 4;           ///< Button 圆角半径
    QColor m_defaultColor;      ///< 默认文字色（无效 = 内置 #1677FF）

    // 交互状态（按格校验后才生效，渲染器实例可跨控件共享）
    int m_hoverRow = -1;        ///< 悬停格行
    int m_hoverColumn = -1;     ///< 悬停格列
    int m_hoverIndex = -1;      ///< 悬停按钮索引
    int m_pressRow = -1;        ///< 按下格行
    int m_pressColumn = -1;     ///< 按下格列
    int m_pressIndex = -1;      ///< 按下按钮索引
};

#endif // AUBUTTONCELLRENDERER_H
