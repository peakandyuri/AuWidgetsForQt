#include "PageHelpers.h"

#include "AgColumnDef.h"
#include "AgGridModel.h"

#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace PageHelpers {

QVector<QVariantMap> makeFlatRows(int rowCount)
{
    QVector<QVariantMap> rows;
    rows.reserve(rowCount);
    const QStringList categories = {
        QStringLiteral(u"水果"), QStringLiteral(u"蔬菜"), QStringLiteral(u"谷物"), QStringLiteral(u"坚果")
    };
    for (int r = 0; r < rowCount; ++r) {
        QVariantMap row;
        row.insert(QStringLiteral("name"), QString("R%1").arg(r));
        row.insert(QStringLiteral("count"), r * 10);
        row.insert(QStringLiteral("category"), categories.at(r % categories.size()));
        row.insert(QStringLiteral("enabled"), r % 3 != 0);
        rows.append(row);
    }
    return rows;
}

AgGridModel *makeFlatModel(int rowCount, QObject *parent)
{
    QVector<AgColumnDef> columns;
    AgColumnDef name("name", QStringLiteral(u"名称"));
    name.setWidth(140);
    columns.append(name);
    AgColumnDef count("count", QStringLiteral(u"数量"));
    count.setWidth(100);
    columns.append(count);
    AgColumnDef category("category", QStringLiteral(u"类别"));
    category.setWidth(110);
    columns.append(category);
    AgColumnDef enabled("enabled", QStringLiteral(u"启用"));
    enabled.setWidth(80);
    columns.append(enabled);

    AgGridModel *model = new AgGridModel(parent);
    model->setColumnDefs(columns);
    model->setRowData(makeFlatRows(rowCount));
    return model;
}

QPlainTextEdit *makeLog(int maxHeight)
{
    QPlainTextEdit *log = new QPlainTextEdit;
    log->setReadOnly(true);
    log->setMaximumHeight(maxHeight);
    log->setPlaceholderText(QStringLiteral(u"信号日志…"));
    return log;
}

void appendLog(QPlainTextEdit *log, const QString &text)
{
    log->appendPlainText(text);
}

QWidget *padded(QWidget *content, int margin)
{
    QWidget *wrapper = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(wrapper);
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->addWidget(content);
    return wrapper;
}

QWidget *composeDemo(QHBoxLayout *controls, QWidget *body, QWidget *footer)
{
    QWidget *panel = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);
    if (controls)
        layout->addLayout(controls);
    layout->addWidget(body, 1);
    if (footer)
        layout->addWidget(footer);
    return panel;
}

} // namespace PageHelpers
