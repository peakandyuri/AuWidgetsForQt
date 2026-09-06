#include "RowDragDemoWindow.h"

#include "AgColumnDef.h"
#include "AgGridModel.h"
#include "AgGridWidget.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

RowDragDemoWindow::RowDragDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"行拖拽重排"),
                     QStringLiteral(u"最左侧拖拽手柄列按住拖动整行重排；勾选多行后拖动可整组移动，"
                                     u"拖到上下边缘自动滚动；带插入位置动画。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
{
    QVector<AgColumnDef> columns;
    AgColumnDef colName("name", QStringLiteral(u"任务"));
    colName.setWidthMode(AgColumnDef::ColumnWidthMode::Flex);
    columns.append(colName);
    AgColumnDef colOwner("owner", QStringLiteral(u"负责人"));
    colOwner.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    colOwner.setWidth(120);
    columns.append(colOwner);
    AgColumnDef colPri("priority", QStringLiteral(u"优先级"));
    colPri.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    colPri.setWidth(90);
    colPri.setSortable(true);
    columns.append(colPri);

    m_model = new AgGridModel(this);
    m_model->setColumnDefs(columns);

    const QStringList owners = { QStringLiteral(u"张三"), QStringLiteral(u"李四"), QStringLiteral(u"王五"), QStringLiteral(u"赵六") };
    QVector<QVariantMap> rows;
    for (int r = 0; r < 30; ++r) {
        QVariantMap row;
        row.insert("name", QStringLiteral(u"任务 %1").arg(r + 1));
        row.insert("owner", owners.at(r % owners.size()));
        row.insert("priority", r % 3);
        rows.append(row);
    }
    m_model->setRowData(rows);

    m_grid = new AgGridWidget(this);
    m_grid->setModel(m_model);
    m_grid->setCheckboxColumnVisible(true);
    m_grid->setRowDragEnabled(true);

    connect(m_grid, &AgGridWidget::rowsDragged, this, [](const QVector<int> &originalRows, int targetOriginalIndex) {
        qDebug() << "rowsDragged: count=" << originalRows.size() << " targetOriginalIndex=" << targetOriginalIndex;
    });

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *toggleBtn = new QPushButton(QStringLiteral(u"关闭行拖拽"), this);
    toggleBtn->setCheckable(true);
    toggleBtn->setChecked(true);
    buttons->addWidget(toggleBtn);
    buttons->addStretch(1);
    connect(toggleBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_grid->setRowDragEnabled(checked);
    });

    m_contentLayout->addLayout(buttons);
    m_contentLayout->addWidget(m_grid, 1);

    addNote("拖动的是原始行位置（rowsDragged 信号返回原始行号与插入点，跨排序过滤稳定）。");
    addNote("先勾选若干行再拖动其中一行，整组一起移动；树形模式下行拖拽不可用。");
}
