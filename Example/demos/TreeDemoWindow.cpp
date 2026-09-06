#include "TreeDemoWindow.h"

#include "AgColumnDef.h"
#include "AgGridModel.h"
#include "AgGridWidget.h"
#include "AgTreeRow.h"

#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

QVector<AgTreeRow> TreeDemoWindow::buildTreeRows()
{
    const QStringList departments = {
        QStringLiteral(u"研发部"), QStringLiteral(u"产品部"), QStringLiteral(u"市场部")
    };
    QVector<AgTreeRow> roots;
    for (int d = 0; d < departments.size(); ++d) {
        AgTreeRow dept;
        dept.data.insert("col0", QString("D%1").arg(d));
        dept.data.insert("col1", departments.at(d));
        dept.data.insert("col4", QStringLiteral(u"3 个项目"));
        for (int p = 0; p < 3; ++p) {
            AgTreeRow project;
            project.data.insert("col0", QString("D%1P%2").arg(d).arg(p));
            project.data.insert("col1", QStringLiteral(u"项目 %1-%2").arg(d).arg(p));
            project.data.insert("col4", QStringLiteral(u"4 个任务"));
            for (int t = 0; t < 4; ++t) {
                AgTreeRow task;
                task.data.insert("col0", QString("D%1P%2T%3").arg(d).arg(p).arg(t));
                task.data.insert("col1", QStringLiteral(u"任务 %1-%2-%3").arg(d).arg(p).arg(t));
                task.data.insert("col4", QString("%1%").arg((d + p + t) * 10 % 100));
                task.data.insert("col6", (d + p + t) % 3 == 0);
                project.children.append(task);
            }
            dept.children.append(project);
        }
        roots.append(dept);
    }
    return roots;
}

TreeDemoWindow::TreeDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"树形表格"),
                     QStringLiteral(u"setTreeData() 载入层级数据（参考 QTreeView 多列即表格树）："
                                     u"首列显示层级缩进与展开箭头，默认全部折叠，仅显示根行。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
    , m_log(nullptr)
{
    // 树形列：首列为树列（缩进 + 箭头），Enabled 为数据列复选框（本示例不改其值）
    QVector<AgColumnDef> columns;
    AgColumnDef colA("col0", "A");
    colA.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    colA.setWidth(110);
    colA.setSortable(true);
    columns.append(colA);
    AgColumnDef colB("col1", QStringLiteral(u"名称"));
    colB.setWidthMode(AgColumnDef::ColumnWidthMode::Flex);
    colB.setSortable(true);
    columns.append(colB);
    AgColumnDef colE("col4", QStringLiteral(u"摘要"));
    colE.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    colE.setWidth(120);
    columns.append(colE);
    AgColumnDef colG("col6", "Enabled");
    colG.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    colG.setWidth(90);
    colG.setCheckbox(true);
    colG.setEditable(true);
    columns.append(colG);

    m_model = new AgGridModel(this);
    m_model->setColumnDefs(columns);
    m_model->setTreeData(buildTreeRows());

    m_grid = new AgGridWidget(this);
    m_grid->setModel(m_model);
    m_grid->setRowNumberColumnVisible(true);
    m_grid->setCheckboxColumnVisible(true);
    m_grid->expandToDepth(0);   // 初始展开到第一层，便于观察

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(120);
    m_log->setPlaceholderText(QStringLiteral(u"信号日志…"));

    connect(m_grid, &AgGridWidget::rowExpandedChanged, this, [this](int row, bool expanded) {
        m_log->appendPlainText(QString("rowExpandedChanged: row=%1 expanded=%2").arg(row).arg(expanded));
    });
    connect(m_grid, &AgGridWidget::rowCheckedChanged, this, [this](int row, bool checked) {
        m_log->appendPlainText(QString("rowCheckedChanged: row=%1 checked=%2").arg(row).arg(checked));
    });

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *expandAllBtn = new QPushButton(QStringLiteral(u"全部展开"), this);
    QPushButton *collapseAllBtn = new QPushButton(QStringLiteral(u"全部折叠"), this);
    QPushButton *expandDepthBtn = new QPushButton(QStringLiteral(u"展开到 2 级"), this);
    QPushButton *expandRootBtn = new QPushButton(QStringLiteral(u"仅展开根行"), this);
    buttons->addWidget(expandAllBtn);
    buttons->addWidget(collapseAllBtn);
    buttons->addWidget(expandDepthBtn);
    buttons->addWidget(expandRootBtn);
    buttons->addStretch(1);

    connect(expandAllBtn, &QPushButton::clicked, this, [this]() { m_grid->expandAll(); });
    connect(collapseAllBtn, &QPushButton::clicked, this, [this]() { m_grid->collapseAll(); });
    connect(expandDepthBtn, &QPushButton::clicked, this, [this]() { m_grid->expandToDepth(1); });
    connect(expandRootBtn, &QPushButton::clicked, this, [this]() { m_grid->expandToDepth(0); });

    m_contentLayout->addLayout(buttons);
    m_contentLayout->addWidget(m_grid, 1);
    m_contentLayout->addWidget(m_log);

    addNote("交互：点击首列箭头 / 双击首列 / 键盘 Left-Right（Right 展开，Left 折叠或选中父行）。");
    addNote("复选框父子级联：勾选父行全选子行；子行部分勾选时父行显示半选；表头全选按原始行全集统计（折叠行也计入）。");
    addNote("展开 / 折叠触发 layoutChanged：合并区域清空、选中按原始行恢复；排序在同级兄弟间进行；行拖拽不可用。");
}
