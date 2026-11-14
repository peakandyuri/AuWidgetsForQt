#include "DataAnimationDemoWindow.h"

#include "AuColumnDef.h"
#include "AuGridModel.h"
#include "AuGridWidget.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRandomGenerator>
#include <QVBoxLayout>

DataAnimationDemoWindow::DataAnimationDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"数据切换动画"),
                     QStringLiteral(u"排序 / 过滤：行从旧布局位置移向新位置，移出视口的行向对应方向退场、"
                                     u"新进入视口的行从所属方向入场（默认 100ms）；重设数据：旧内容先上移离场、"
                                     u"视口短暂留空后新内容自上方下移涌入（总时长默认 200ms，前后各半）；"
                                     u"删除行：被删行原位淡出并轻微上漂，下方行上滑补位。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
    , m_animSwitch(nullptr)
{
    m_model = new AuGridModel(this);

    QVector<AuColumnDef> columns;
    AuColumnDef colName("name", QStringLiteral(u"名称"));
    colName.setWidthMode(AuColumnDef::ColumnWidthMode::Fixed);
    colName.setWidth(130);
    colName.setSortable(true);
    columns.append(colName);
    AuColumnDef colScore("score", QStringLiteral(u"分数"));
    colScore.setWidthMode(AuColumnDef::ColumnWidthMode::Fixed);
    colScore.setWidth(100);
    colScore.setSortable(true);
    colScore.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    columns.append(colScore);
    AuColumnDef colDesc("desc", QStringLiteral(u"描述"));
    colDesc.setWidthMode(AuColumnDef::ColumnWidthMode::Flex);
    columns.append(colDesc);
    m_model->setColumnDefs(columns);
    m_model->setRowData(makeRows(40));

    m_grid = new AuGridWidget(this);
    m_grid->setModel(m_model);
    m_grid->setSortingEnabled(true);   // 点击表头排序同样触发位移动画
    m_grid->setSelectionMode(AuGridWidget::SelectionMode::Multiple);   // 多选后可整批删除

    // 控制按钮
    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *ascBtn = new QPushButton(QStringLiteral(u"分数升序"), this);
    QPushButton *descBtn = new QPushButton(QStringLiteral(u"分数降序"), this);
    QPushButton *reverseBtn = new QPushButton(QStringLiteral(u"名称反转"), this);
    QPushButton *resetBtn = new QPushButton(QStringLiteral(u"重设数据"), this);
    QPushButton *deleteSelectedBtn = new QPushButton(QStringLiteral(u"删除选中行"), this);
    QPushButton *deleteRandomBtn = new QPushButton(QStringLiteral(u"随机删 3 行"), this);
    QPushButton *deleteEvenBtn = new QPushButton(QStringLiteral(u"隔行删除"), this);
    m_animSwitch = new QCheckBox(QStringLiteral(u"动画开关"), this);
    m_animSwitch->setChecked(true);
    buttons->addWidget(ascBtn);
    buttons->addWidget(descBtn);
    buttons->addWidget(reverseBtn);
    buttons->addWidget(resetBtn);
    buttons->addWidget(deleteSelectedBtn);
    buttons->addWidget(deleteRandomBtn);
    buttons->addWidget(deleteEvenBtn);
    buttons->addWidget(m_animSwitch);
    buttons->addStretch(1);

    connect(ascBtn, &QPushButton::clicked, this, [this]() { m_model->sort(1, Qt::AscendingOrder); });
    connect(descBtn, &QPushButton::clicked, this, [this]() { m_model->sort(1, Qt::DescendingOrder); });
    connect(reverseBtn, &QPushButton::clicked, this, [this]() {
        const bool nameAsc = m_grid->sortedColumn() == 0
                             && m_grid->sortOrder() == Qt::AscendingOrder;
        m_model->sort(0, nameAsc ? Qt::DescendingOrder : Qt::AscendingOrder);
    });
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        m_model->setRowData(makeRows(40));
    });
    connect(deleteSelectedBtn, &QPushButton::clicked, this, [this]() {
        const QSet<int> selected = m_grid->selectedOriginalRows();
        if (selected.isEmpty())
            return;
        QVector<int> victims;
        victims.reserve(selected.size());
        for (int original : selected)
            victims.append(original);
        m_model->removeRows(victims);
    });
    connect(deleteRandomBtn, &QPushButton::clicked, this, [this]() {
        const int count = m_model->originalRowCount();
        if (count == 0)
            return;
        QVector<int> victims;
        for (int i = 0; i < 3 && i < count; ++i)
            victims.append(QRandomGenerator::global()->bounded(count));
        m_model->removeRows(victims);
    });
    connect(deleteEvenBtn, &QPushButton::clicked, this, [this]() {
        QVector<int> victims;
        for (int i = 0; i < m_model->originalRowCount(); i += 2)
            victims.append(i);
        m_model->removeRows(victims);
    });
    connect(m_animSwitch, &QCheckBox::toggled, this, [this](bool checked) {
        m_grid->setDataAnimationEnabled(checked);
    });

    m_contentLayout->addLayout(buttons);
    m_contentLayout->addWidget(m_grid, 1);

    addNote("排序动画：以当前可视区域为基准，可视数据的新位置在视口外时向对应方向移出视口，"
            "视口外数据变为可见时从所属方向移入；仍在视口内的数据直接滑到新位置。");
    addNote("重设数据动画：旧内容整体上移出场，新内容自视口下方移入（setRowData / setTreeData / clear 触发）。");
    addNote("删除动画：被删行原位淡出并轻微上漂，下方行上滑补位（removeRows 触发）；"
            "全在视口外的行不播动画。删除会使后续行原始行号前移，勾选与选中随删除清空。");
    addNote("关闭动画开关后，排序、重设与删除立即生效（无过渡）。");
}

QVector<QVariantMap> DataAnimationDemoWindow::makeRows(int count) const
{
    static int batch = 0;
    ++batch;
    QVector<QVariantMap> rows;
    for (int r = 0; r < count; ++r) {
        QVariantMap row;
        row.insert("name", QString("D%1-%2").arg(batch).arg(r, 2, 10, QChar('0')));
        row.insert("score", (r * 37 + batch * 13) % 100);
        row.insert("desc", QStringLiteral(u"第 %1 批数据 · 记录 %2").arg(batch).arg(r + 1));
        rows.append(row);
    }
    return rows;
}
