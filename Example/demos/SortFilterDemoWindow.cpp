#include "SortFilterDemoWindow.h"

#include "AgColumnDef.h"
#include "AgGridModel.h"
#include "AgGridWidget.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

SortFilterDemoWindow::SortFilterDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"排序与过滤"),
                     QStringLiteral(u"表头点击排序（sortable 列）、多规则逻辑排序、自定义过滤回调；"
                                     u"排序只重排可见行，原始数据顺序保持不变。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
{
    QVector<AgColumnDef> columns;
    AgColumnDef colA("col0", "A");
    colA.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    colA.setWidth(90);
    colA.setSortable(true);
    columns.append(colA);
    AgColumnDef colB("col1", "B");
    colB.setWidthMode(AgColumnDef::ColumnWidthMode::Flex);
    colB.setSortable(true);
    columns.append(colB);
    AgColumnDef colC("col2", "C");
    colC.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    colC.setWidth(120);
    columns.append(colC);
    AgColumnDef colD("col3", "D");
    colD.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    colD.setWidth(120);
    colD.setSortable(true);
    columns.append(colD);

    m_model = new AgGridModel(this);
    m_model->setColumnDefs(columns);
    m_model->setRowData(makeFlatRows(200));

    m_grid = new AgGridWidget(this);
    m_grid->setModel(m_model);
    m_grid->setSortingEnabled(true);   // 表头点击排序总开关

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *sortABtn = new QPushButton(QStringLiteral(u"A 列升序"), this);
    QPushButton *sortMultiBtn = new QPushButton(QStringLiteral(u"逻辑排序 A↑B↓"), this);
    QPushButton *clearSortBtn = new QPushButton(QStringLiteral(u"清除排序"), this);
    QPushButton *filterBtn = new QPushButton(QStringLiteral(u"过滤 R1 开头"), this);
    QPushButton *clearFilterBtn = new QPushButton(QStringLiteral(u"清除过滤"), this);
    buttons->addWidget(sortABtn);
    buttons->addWidget(sortMultiBtn);
    buttons->addWidget(clearSortBtn);
    buttons->addWidget(filterBtn);
    buttons->addWidget(clearFilterBtn);
    buttons->addStretch(1);

    connect(sortABtn, &QPushButton::clicked, this, [this]() {
        QList<AgSortRule> rules;
        rules << AgSortRule{0, Qt::AscendingOrder};
        m_grid->setSortRules(rules);
    });
    connect(sortMultiBtn, &QPushButton::clicked, this, [this]() {
        QList<AgSortRule> rules;
        rules << AgSortRule{0, Qt::AscendingOrder};
        rules << AgSortRule{1, Qt::DescendingOrder};
        m_grid->setSortRules(rules);
    });
    connect(clearSortBtn, &QPushButton::clicked, this, [this]() {
        m_grid->clearSort();
    });
    connect(filterBtn, &QPushButton::clicked, this, [this]() {
        m_grid->setFilterCallback([](const QVariantMap &row) {
            return row.value("col0").toString().startsWith("R1");
        });
    });
    connect(clearFilterBtn, &QPushButton::clicked, this, [this]() {
        m_grid->clearFilter();
    });

    m_contentLayout->addLayout(buttons);
    m_contentLayout->addWidget(m_grid, 1);

    addNote("点击 A / B / D 列表头切换排序方向；C 列未设置 sortable 不可排序。");
    addNote("多规则排序按规则顺序依次比较：第一列相等时才比较第二列。");
    addNote("排序 / 过滤后 selectedOriginalRow 按原始行恢复选中；合并单元格会清空。");
}
