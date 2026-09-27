#ifndef LOADINGDEMOWINDOW_H
#define LOADINGDEMOWINDOW_H

#include "DemoWindowBase.h"

class AuGridModel;
class AuGridWidget;

/**
 * @brief 加载遮罩示例：setLoading() 显隐、自定义文字、模拟异步请求。
 */
class LoadingDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit LoadingDemoWindow(QWidget *parent = nullptr);

private:
    AuGridWidget *m_grid;
    AuGridModel *m_model;
};

#endif // LOADINGDEMOWINDOW_H
