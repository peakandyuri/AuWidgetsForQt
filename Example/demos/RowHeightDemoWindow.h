#ifndef ROWHEIGHTDEMOWINDOW_H
#define ROWHEIGHTDEMOWINDOW_H

#include "DemoWindowBase.h"

class AuGridModel;
class AuGridWidget;

/**
 * @brief 自适应行高示例：WordWrap 列按内容自动换行并撑高所在行、
 *        行高跟随列宽变化重算、换行 / 截断模式切换对比。
 */
class RowHeightDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit RowHeightDemoWindow(QWidget *parent = nullptr);

private:
    /// 按 wrapMode 重建列定义（演示换行模式切换对行高的影响）。
    void applyWrapMode(bool wordWrap);

    AuGridWidget *m_grid;
    AuGridModel *m_model;
};

#endif // ROWHEIGHTDEMOWINDOW_H
