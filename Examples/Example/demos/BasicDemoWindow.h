#ifndef BASICDEMOWINDOW_H
#define BASICDEMOWINDOW_H

#include "DemoWindowBase.h"

class AuGridWidget;
class AuGridModel;

/**
 * @brief 基础功能示例：列宽模式、多级表头、单元格编辑与校验、自定义渲染器。
 */
class BasicDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit BasicDemoWindow(QWidget *parent = nullptr);

private:
    AuGridWidget *m_grid;
    AuGridModel *m_model;
};

#endif // BASICDEMOWINDOW_H
