#ifndef SCROLLDEMOWINDOW_H
#define SCROLLDEMOWINDOW_H

#include "DemoWindowBase.h"

class AuGridModel;
class AuGridWidget;
class QSpinBox;

/**
 * @brief 大数据量与滚动定位示例：十万行级虚拟化渲染、scrollToRow / scrollToCell。
 */
class ScrollDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit ScrollDemoWindow(QWidget *parent = nullptr);

private:
    AuGridWidget *m_grid;
    AuGridModel *m_model;
    QSpinBox *m_rowSpin;
    QSpinBox *m_columnSpin;
};

#endif // SCROLLDEMOWINDOW_H
