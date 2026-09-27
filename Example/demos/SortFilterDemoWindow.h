#ifndef SORTFILTERDEMOWINDOW_H
#define SORTFILTERDEMOWINDOW_H

#include "DemoWindowBase.h"

class AuGridModel;
class AuGridWidget;

/**
 * @brief 排序与过滤示例：表头点击排序、多规则逻辑排序、自定义过滤回调。
 */
class SortFilterDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit SortFilterDemoWindow(QWidget *parent = nullptr);

private:
    AuGridWidget *m_grid;
    AuGridModel *m_model;
};

#endif // SORTFILTERDEMOWINDOW_H
