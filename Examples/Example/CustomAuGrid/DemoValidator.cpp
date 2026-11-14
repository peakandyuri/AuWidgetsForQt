#include "DemoValidator.h"
#include "AuGridModel.h"

#include <QString>

AuEditValidationResult DemoValidator::validate(
    const AuGridModel *model,
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
        return AuEditValidationResult::Accept;

    const QString text = newValue.toString().trimmed();
    if (text.isEmpty()) {
        errorMessage = QStringLiteral(u"B 列不能为空");
        return AuEditValidationResult::RejectKeepEditing;
    }
    if (text.startsWith("X", Qt::CaseInsensitive)) {
        errorMessage = QStringLiteral(u"B 列不能以 X 开头");
        return AuEditValidationResult::RejectKeepEditing;
    }
    return AuEditValidationResult::Accept;
}
