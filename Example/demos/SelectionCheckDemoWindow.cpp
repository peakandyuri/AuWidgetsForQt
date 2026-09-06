#include "SelectionCheckDemoWindow.h"

#include "AgColumnDef.h"
#include "AgGridModel.h"
#include "AgGridWidget.h"

#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

SelectionCheckDemoWindow::SelectionCheckDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"选中与勾选"),
                     QStringLiteral(u"内置行号列与复选框列；表头复选框支持全选 / 半选；"
                                     u"行可选回调可置灰个别行；选中 API 与勾选状态查询。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
    , m_log(nullptr)
{
    // 三列示例：名称 / 数量 / 启用（数据列复选框）
    QVector<AgColumnDef> columns;
    AgColumnDef colName("name", QStringLiteral(u"名称"));
    colName.setWidthMode(AgColumnDef::ColumnWidthMode::Flex);
    columns.append(colName);
    AgColumnDef colCount("count", QStringLiteral(u"数量"));
    colCount.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    colCount.setWidth(120);
    columns.append(colCount);
    AgColumnDef colEnabled("enabled", QStringLiteral(u"启用"));
    colEnabled.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    colEnabled.setWidth(90);
    colEnabled.setCheckbox(true);
    colEnabled.setEditable(true);
    columns.append(colEnabled);

    m_model = new AgGridModel(this);
    m_model->setColumnDefs(columns);

    QVector<QVariantMap> rows;
    for (int r = 0; r < 30; ++r) {
        QVariantMap row;
        row.insert("name", QString("R%1").arg(r));
        row.insert("count", r * 10);
        row.insert("enabled", r % 3 != 0);
        rows.append(row);
    }
    m_model->setRowData(rows);

    m_grid = new AgGridWidget(this);
    m_grid->setModel(m_model);
    m_grid->setRowNumberColumnVisible(true);
    m_grid->setCheckboxColumnVisible(true);

    // 行可选回调：名称以 R1 开头的行不可勾选（复选框置灰）
    m_grid->setRowCheckableCallback([](const AgGridModel *model, int row) {
        return !model->cellValue(row, "name").toString().startsWith("R1");
    });

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(140);
    m_log->setPlaceholderText(QStringLiteral(u"信号日志…"));

    auto appendLog = [this](const QString &text) {
        m_log->appendPlainText(text);
    };

    connect(m_grid, &AgGridWidget::rowCheckedChanged, this, [appendLog](int row, bool checked) {
        appendLog(QString("rowCheckedChanged: row=%1 checked=%2").arg(row).arg(checked));
    });
    connect(m_grid, &AgGridWidget::checkedStateChanged, this, [this, appendLog]() {
        appendLog(QString("checkedStateChanged: 可见=%1 原始=%2")
                      .arg(m_grid->checkedRows().size())
                      .arg(m_grid->checkedOriginalRows().size()));
    });
    connect(m_grid, &AgGridWidget::cellCheckStateChanged, this, [appendLog](int row, int column, bool checked) {
        appendLog(QString("cellCheckStateChanged: row=%1 column=%2 checked=%3").arg(row).arg(column).arg(checked));
    });
    connect(m_grid, &AgGridWidget::selectionChanged, this, [appendLog]() {
        appendLog(QStringLiteral("selectionChanged"));
    });

    // 控制按钮
    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *selectRowBtn = new QPushButton(QStringLiteral(u"选中第 2 行"), this);
    QPushButton *selectCellBtn = new QPushButton(QStringLiteral(u"选中 (2,1)"), this);
    QPushButton *clearSelBtn = new QPushButton(QStringLiteral(u"清除选中"), this);
    QPushButton *printBtn = new QPushButton(QStringLiteral(u"打印勾选"), this);
    QPushButton *selectAllBtn = new QPushButton(QStringLiteral(u"全选"), this);
    QPushButton *clearCheckedBtn = new QPushButton(QStringLiteral(u"清空勾选"), this);
    buttons->addWidget(selectRowBtn);
    buttons->addWidget(selectCellBtn);
    buttons->addWidget(clearSelBtn);
    buttons->addWidget(printBtn);
    buttons->addWidget(selectAllBtn);
    buttons->addWidget(clearCheckedBtn);
    buttons->addStretch(1);

    connect(selectRowBtn, &QPushButton::clicked, this, [this]() { m_grid->selectRow(2); });
    connect(selectCellBtn, &QPushButton::clicked, this, [this]() { m_grid->selectCell(2, 1); });
    connect(clearSelBtn, &QPushButton::clicked, this, [this]() { m_grid->clearSelection(); });
    connect(printBtn, &QPushButton::clicked, this, [this, appendLog]() {
        auto joinInts = [](const QSet<int> &set) {
            QStringList parts;
            const QList<int> values = set.values();
            for (int v : values)
                parts.append(QString::number(v));
            return parts.join(',');
        };
        appendLog(QString("checkedRows(可见)=%1").arg(joinInts(m_grid->checkedRows())));
        appendLog(QString("checkedOriginalRows=%1").arg(joinInts(m_grid->checkedOriginalRows())));
    });
    connect(selectAllBtn, &QPushButton::clicked, this, [this]() { m_grid->selectAllRows(); });
    connect(clearCheckedBtn, &QPushButton::clicked, this, [this]() { m_grid->clearAllCheckedRows(); });

    m_contentLayout->addLayout(buttons);
    m_contentLayout->addWidget(m_grid, 1);
    m_contentLayout->addWidget(m_log);

    addNote("名称以 R1 开头的行复选框置灰不可勾选（行可选回调）。");
    addNote("数据列\"启用\"为列定义复选框（值存模型，点击发 cellCheckStateChanged）；"
            "左侧复选框列为内置勾选列（状态存控件，跨排序过滤稳定）。");
    addNote("勾选集合持久化建议使用 checkedOriginalRows()（原始行号）。");
}
