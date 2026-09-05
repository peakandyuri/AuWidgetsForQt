#ifndef AGGRIDSTYLE_H
#define AGGRIDSTYLE_H

#include "AgGrid_global.h"

#include <QVariant>
#include <QColor>
#include <QFont>

class AgGridModel;
class AgGridWidget;

class AGGRID_EXPORT AgGridStyle
{
public:
    enum class SizeMode {
        Small,
        Medium,
        Large
    };

    enum StyleRole {
        // 全局尺寸
        HeaderHeight,
        RowHeight,
        ScrollBarExtent,
        CornerRadius,

        // 表头
        HeaderBackgroundColor,
        HeaderForegroundColor,
        HeaderBorderColor,
        HeaderFont,
        HeaderAlignment,

        // 单元格 / 网格线
        CellBorderColor,
        GridLineColor,
        RowBackgroundColor,
        SelectedRowBackgroundColor,
        AlternateRowBackgroundColor,
        HoverRowBackgroundColor,
        FocusedCellBorderColor,
        FocusedCellBorderWidth,
        CellHorizontalPadding,
        CellVerticalPadding,

        // 滚动条
        ScrollBarSliderColor,
        ScrollBarTrackBackgroundColor,

        // 字体与对齐
        Font,
        DefaultTextAlignment,

        // 行列上下文相关
        CellBackgroundColor,
        CellForegroundColor,
        CellTextAlignment,

        // 外边框
        BorderColor,

        // 复选框列
        CheckboxSize,
        CheckboxCheckedColor,
        CheckboxUncheckedColor,
        CheckboxBorderColor,
        CheckboxCheckmarkColor,

        // 行拖拽
        DragHandleIconColor,
        DragHandleHoveredIconColor,
        DragHandleDotSize,
        DragHandleBackgroundColor,
        DragHandleHoveredBackgroundColor,
        DraggedRowOverlayColor,

        // 行号列
        RowNumberFontBold,
        RowNumberForegroundColor
    };

    AgGridStyle();
    virtual ~AgGridStyle();

    virtual QVariant value(StyleRole role,
                           const AgGridModel *model = nullptr,
                           int row = -1,
                           int column = -1) const;

    template <typename T>
    T valueAs(StyleRole role, const AgGridModel *model = nullptr, int row = -1, int column = -1) const
    {
        return value(role, model, row, column).template value<T>();
    }

    QColor color(StyleRole role, const AgGridModel *model = nullptr, int row = -1, int column = -1) const;
    int integer(StyleRole role, const AgGridModel *model = nullptr, int row = -1, int column = -1) const;
    QFont font(const AgGridModel *model = nullptr, int row = -1, int column = -1) const;
    Qt::Alignment alignment(const AgGridModel *model = nullptr, int row = -1, int column = -1) const;

    SizeMode sizeMode() const;
    void setSizeMode(SizeMode mode);

    virtual AgGridStyle *clone() const;

    // 全局样式：未显式设置样式的 AgGridWidget 会自动使用
    static AgGridStyle *globalStyle();
    static void setGlobalStyle(AgGridStyle *style);

    // 内置主题工厂
    static AgGridStyle *createLightStyle();
    static AgGridStyle *createDarkStyle();

protected:
    QVariant defaultValue(StyleRole role) const;

    SizeMode m_sizeMode;

private:
    friend class AgGridWidget;
    static void registerGlobalStyleObserver(AgGridWidget *widget);
    static void unregisterGlobalStyleObserver(AgGridWidget *widget);
    static void notifyGlobalStyleChanged();

    static AgGridStyle *s_globalStyle;
    static AgGridStyle *s_defaultLightStyle;
    static QVector<AgGridWidget *> s_observers;
};

#endif // AGGRIDSTYLE_H
