#ifndef TREEDEMOWINDOW_H
#define TREEDEMOWINDOW_H

#include "DemoWindowBase.h"
#include "AgTreeRow.h"

class AgGridModel;
class AgGridWidget;
class QPlainTextEdit;

/**
 * @brief 树形表格示例：层级数据、展开折叠、内置复选框列父子级联三态。
 */
class TreeDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit TreeDemoWindow(QWidget *parent = nullptr);

private:
    /// 构建"部门 → 项目 → 任务"三级示例树。
    static QVector<AgTreeRow> buildTreeRows();

    AgGridWidget *m_grid;
    AgGridModel *m_model;
    QPlainTextEdit *m_log;
};

#endif // TREEDEMOWINDOW_H
