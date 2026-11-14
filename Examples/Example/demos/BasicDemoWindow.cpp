#include "BasicDemoWindow.h"

#include "AuGridModel.h"
#include "AuGridWidget.h"
#include "AuComboBoxCellEditor.h"
#include "AuCellEditValidator.h"
#include "ButtonRenderer.h"
#include "HeaderButtonRenderer.h"
#include "DemoValidator.h"

#include <QDebug>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

BasicDemoWindow::BasicDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"基础功能"),
                     QStringLiteral(u"列宽模式（Fixed / AutoFit / Flex）、多级表头（Multi 分组）、"
                                     u"单元格编辑（双击 / 回车 / 直接输入）、下拉编辑器、提交校验、"
                                     u"自定义单元格渲染器（F 列按钮）与表头渲染器（E 列表头按钮）、动态可编辑回调。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
{
    const QStringList comboOptions = { "Alpha", "Beta", "Gamma", "Delta" };

    m_model = new AuGridModel(this);
    m_model->setColumnDefs(makeStandardColumns());
    m_model->setRowData(makeFlatRows(200));

    m_grid = new AuGridWidget(this);
    m_grid->setModel(m_model);

    m_grid->registerCellRenderer("button", QSharedPointer<AuCellRenderer>(new ButtonRenderer()));
    m_grid->registerHeaderRenderer("headerButton", QSharedPointer<AuHeaderRenderer>(new HeaderButtonRenderer()));
    m_grid->registerCellEditor("combo", QSharedPointer<AuCellEditor>(new AuComboBoxCellEditor(comboOptions)));
    m_grid->setCellEditValidator(QSharedPointer<AuCellEditValidator>(new DemoValidator()));

    // 动态可编辑示例：col2 只有在 col0 的值以 "R0" 开头时才可编辑
    m_grid->setCellEditableCallback([](const AuGridModel *model, int row, int column) {
        if (column != 2 || !model)
            return true;
        return model->cellValue(row, "col0").toString().startsWith("R0");
    });

    // 列定义示例：E 列（索引 3）使用表头按钮渲染器，F 列（索引 4）使用按钮单元格渲染器
    {
        QVector<AuColumnDef> columns = m_model->topLevelColumnDefs();
        columns[3].setHeaderRenderer("headerButton");
        columns[4].setCellRenderer("button");
        m_model->setColumnDefs(columns);
    }

    connect(m_grid, &AuGridWidget::cellValueChanged, this, [](int row, int column, const QVariant &value) {
        qDebug() << "Cell value changed: row=" << row << " column=" << column << " value=" << value;
    });

    // 动态数据更新：每秒修改一次 (row=5, col=1)，展示 dataChanged 增量重绘
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        static int n = 0;
        m_model->setCellValue(5, "col1", QString("Updated %1").arg(++n));
    });
    timer->start(1000);

    m_contentLayout->addWidget(m_grid, 1);

    addNote("A 列固定宽 90、B 列内容自适应、E/F 列按 2:1 弹性分配剩余空间；C 列禁止拖宽。");
    addNote("D 列双击弹出下拉编辑；B 列编辑为空或以 X 开头会被校验拒绝并保持编辑状态。");
    addNote("col2 仅在 col0 以 R0 开头时可编辑（动态可编辑回调）；F 列按钮为自定义渲染器。");
}
