#ifndef DATAANIMATIONDEMOWINDOW_H
#define DATAANIMATIONDEMOWINDOW_H

#include "DemoWindowBase.h"

class AuGridModel;
class AuGridWidget;
class QCheckBox;

/**
 * @brief 数据切换动画示例：排序 / 过滤时行按旧位置 → 新位置位移（200ms），
 *        重设数据时旧内容上移出场、新内容自视口下方移入（200ms）。
 */
class DataAnimationDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit DataAnimationDemoWindow(QWidget *parent = nullptr);

private:
    QVector<QVariantMap> makeRows(int count) const;

    AuGridWidget *m_grid;
    AuGridModel *m_model;
    QCheckBox *m_animSwitch;
};

#endif // DATAANIMATIONDEMOWINDOW_H
