#ifndef STYLEDEMOWINDOW_H
#define STYLEDEMOWINDOW_H

#include "DemoWindowBase.h"

class AuGridModel;
class AuGridWidget;

/**
 * @brief 样式与外观示例：全局主题、尺寸模式、独立样式与上下文样式。
 */
class StyleDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit StyleDemoWindow(QWidget *parent = nullptr);

private:
    AuGridWidget *m_grid;
    AuGridModel *m_model;
};

#endif // STYLEDEMOWINDOW_H
