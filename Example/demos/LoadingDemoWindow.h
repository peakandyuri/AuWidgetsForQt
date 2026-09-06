#ifndef LOADINGDEMOWINDOW_H
#define LOADINGDEMOWINDOW_H

#include "DemoWindowBase.h"

class AgGridModel;
class AgGridWidget;

/**
 * @brief 加载遮罩示例：setLoading() 显隐、自定义文字、模拟异步请求。
 */
class LoadingDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit LoadingDemoWindow(QWidget *parent = nullptr);

private:
    AgGridWidget *m_grid;
    AgGridModel *m_model;
};

#endif // LOADINGDEMOWINDOW_H
