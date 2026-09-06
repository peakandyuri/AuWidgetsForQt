#ifndef BASICDEMOWINDOW_H
#define BASICDEMOWINDOW_H

#include "DemoWindowBase.h"

class AgGridWidget;
class AgGridModel;

/**
 * @brief 基础功能示例：列宽模式、多级表头、单元格编辑与校验、自定义渲染器。
 */
class BasicDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit BasicDemoWindow(QWidget *parent = nullptr);

private:
    AgGridWidget *m_grid;
    AgGridModel *m_model;
};

#endif // BASICDEMOWINDOW_H
