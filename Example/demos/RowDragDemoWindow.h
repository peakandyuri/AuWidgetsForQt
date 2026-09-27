#ifndef ROWDRAGDEMOWINDOW_H
#define ROWDRAGDEMOWINDOW_H

#include "DemoWindowBase.h"

class AuGridModel;
class AuGridWidget;

/**
 * @brief 行拖拽重排示例：拖拽手柄整行拖动，带动画与自动滚动。
 */
class RowDragDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit RowDragDemoWindow(QWidget *parent = nullptr);

private:
    AuGridWidget *m_grid;
    AuGridModel *m_model;
};

#endif // ROWDRAGDEMOWINDOW_H
