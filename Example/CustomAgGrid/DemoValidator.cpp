#include "DemoValidator.h"
#include "AgGridModel.h"

#include <QString>

AgEditValidationResult DemoValidator::validate(
    const AgGridModel *model,
    int row,
    int column,
    const QVariant &oldValue,
    const QVariant &newValue,
    QString &errorMessage) const
{
    Q_UNUSED(model)
    Q_UNUSED(row)
    Q_UNUSED(oldValue)

    if (column != 1)
        return AgEditValidationResult::Accept;

    const QString text = newValue.toString().trimmed();
    if (text.isEmpty()) {
        errorMessage = QStringLiteral(u"B 列不能为空");
        return AgEditValidationResult::RejectKeepEditing;
    }
    if (text.startsWith("X", Qt::CaseInsensitive)) {
        errorMessage = QStringLiteral(u"B 列不能以 X 开头");
        return AgEditValidationResult::RejectKeepEditing;
    }
    return AgEditValidationResult::Accept;
}
