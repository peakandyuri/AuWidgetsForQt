#include "SyncDemoWindow.h"

#include "AgGridModel.h"
#include "AgGridSyncGroup.h"
#include "AgGridWidget.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

SyncDemoWindow::SyncDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"多表格同步"),
                     QStringLiteral(u"AgGridSyncGroup 把多个表格的滚动偏移与列宽互相同步，"
                                     u"适合上下两份相同数据对比差异的场景。"),
                     parent)
    , m_gridA(nullptr)
    , m_gridB(nullptr)
    , m_modelA(nullptr)
    , m_modelB(nullptr)
    , m_syncGroup(nullptr)
{
    m_modelA = new AgGridModel(this);
    m_modelA->setColumnDefs(makeStandardColumns());
    m_modelA->setRowData(makeFlatRows(500));

    m_modelB = new AgGridModel(this);
    m_modelB->setColumnDefs(makeStandardColumns());
    m_modelB->setRowData(makeFlatRows(500));

    m_gridA = new AgGridWidget(this);
    m_gridA->setModel(m_modelA);
    m_gridB = new AgGridWidget(this);
    m_gridB->setModel(m_modelB);

    m_syncGroup = new AgGridSyncGroup(this);
    m_syncGroup->addGrid(m_gridA);
    m_syncGroup->addGrid(m_gridB);
    m_syncGroup->syncFrom(m_gridA);

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *toggleSyncBtn = new QPushButton(QStringLiteral(u"同步"), this);
    toggleSyncBtn->setCheckable(true);
    toggleSyncBtn->setChecked(true);
    buttons->addWidget(toggleSyncBtn);
    buttons->addStretch(1);
    connect(toggleSyncBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_syncGroup->setSyncEnabled(checked);
    });

    // 数据一致性：同步修改两份模型同一单元格（数据对比场景需调用方自行保证数据一致）
    connect(m_gridA, &AgGridWidget::cellValueChanged, this, [this](int row, int column, const QVariant &value) {
        m_modelB->setCellValue(row, m_modelB->fieldAtColumn(column), value);
    });

    QSplitter *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_gridA);
    splitter->addWidget(m_gridB);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    m_contentLayout->addLayout(buttons);
    m_contentLayout->addWidget(splitter, 1);

    addNote("在一个表格里滚动 / 拖列宽，另一个表格跟随；关闭\"同步\"后恢复独立。");
    addNote("syncFrom(gridA) 立即把 gridA 的当前偏移与列宽应用到其余表格。");
}
