#include "HeaderCheckDemoWindow.h"

#include "AuColumnDef.h"
#include "AuGridModel.h"
#include "AuGridWidget.h"
#include "HeaderCheckBoxRenderer.h"

#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

HeaderCheckDemoWindow::HeaderCheckDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"表头全选（数据列复选框）"),
                     QStringLiteral(u"数据列复选框（columnDef.setCheckbox(true)）的列头上增加"
                                     u"全选 / 半选组件：通过自定义表头渲染器实现，点击表头复选框"
                                     u"全选或清空当前列，状态由该列数据实时聚合。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
    , m_log(nullptr)
{
    QVector<AuColumnDef> columns;
    AuColumnDef colName("name", QStringLiteral(u"任务"));
    colName.setWidthMode(AuColumnDef::ColumnWidthMode::Flex);
    colName.setSortable(true);
    columns.append(colName);
    AuColumnDef colOwner("owner", QStringLiteral(u"负责人"));
    colOwner.setWidthMode(AuColumnDef::ColumnWidthMode::Fixed);
    colOwner.setWidth(120);
    colOwner.setSortable(true);
    columns.append(colOwner);
    // 两个复选框数据列共用同一个表头渲染器实例
    AuColumnDef colPass("pass", QStringLiteral(u"通过"));
    colPass.setWidthMode(AuColumnDef::ColumnWidthMode::Fixed);
    colPass.setWidth(90);
    colPass.setCheckbox(true);
    colPass.setEditable(true);
    colPass.setHeaderRenderer("headerCheckBox");
    columns.append(colPass);
    AuColumnDef colUrgent("urgent", QStringLiteral(u"加急"));
    colUrgent.setWidthMode(AuColumnDef::ColumnWidthMode::Fixed);
    colUrgent.setWidth(90);
    colUrgent.setCheckbox(true);
    colUrgent.setEditable(true);
    colUrgent.setHeaderRenderer("headerCheckBox");
    columns.append(colUrgent);

    m_model = new AuGridModel(this);
    m_model->setColumnDefs(columns);

    const QStringList owners = {
        QStringLiteral(u"张三"), QStringLiteral(u"李四"), QStringLiteral(u"王五"), QStringLiteral(u"赵六")
    };
    QVector<QVariantMap> rows;
    for (int r = 0; r < 30; ++r) {
        QVariantMap row;
        row.insert("name", QStringLiteral(u"任务 %1").arg(r + 1));
        row.insert("owner", owners.at(r % owners.size()));
        row.insert("pass", r % 3 != 0);
        row.insert("urgent", r % 4 == 0);
        rows.append(row);
    }
    m_model->setRowData(rows);

    m_grid = new AuGridWidget(this);
    m_grid->setModel(m_model);
    m_grid->setSortingEnabled(true);

    HeaderCheckBoxRenderer *headerCheck = new HeaderCheckBoxRenderer();
    headerCheck->setModel(m_model);
    m_grid->registerHeaderRenderer("headerCheckBox",
                                   QSharedPointer<AuHeaderRenderer>(headerCheck));

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(120);
    m_log->setPlaceholderText(QStringLiteral(u"信号日志…"));

    connect(m_grid, &AuGridWidget::cellCheckStateChanged, this, [this](int row, int column, bool checked) {
        m_log->appendPlainText(QString("cellCheckStateChanged: row=%1 column=%2 checked=%3")
                                   .arg(row).arg(column).arg(checked));
    });

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *sortBtn = new QPushButton(QStringLiteral(u"按任务排序"), this);
    buttons->addWidget(sortBtn);
    buttons->addStretch(1);
    connect(sortBtn, &QPushButton::clicked, this, [this]() {
        QList<AuSortRule> rules;
        rules << AuSortRule{0, Qt::AscendingOrder};
        m_grid->setSortRules(rules);
    });

    m_contentLayout->addLayout(buttons);
    m_contentLayout->addWidget(m_grid, 1);
    m_contentLayout->addWidget(m_log);

    addNote("点击表头复选框 = 全选 / 清空该列（作用于当前全部可见行）；单元格点击 / 空格切换单行。");
    addNote("表头复选框为三态：由该列数据实时聚合，部分勾选显示半选；排序 / 过滤后状态自动跟随数据。");
    addNote("复选框热区内的点击不会触发表头排序；热区外点击表头仍可排序（任务 / 负责人列已开启 sortable）。");
    addNote("实现方式：自定义表头渲染器（hitTest 声明热区 + headerMouseClick 遍历写模型），"
            "两个复选框列共用同一渲染器实例，见 CustomAuGrid/HeaderCheckBoxRenderer。");
}
