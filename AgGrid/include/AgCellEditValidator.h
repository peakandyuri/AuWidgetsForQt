#ifndef AGCELLEDITVALIDATOR_H
#define AGCELLEDITVALIDATOR_H

#include "AgGrid_global.h"

#include <QVariant>
#include <QString>

class AgGridModel;

enum class AgEditValidationResult {
    Accept,            // 接受新值，关闭编辑器并更新模型
    Reject,            // 拒绝新值，关闭编辑器但不更新模型
    RejectKeepEditing  // 拒绝新值，保持编辑器打开并提示错误
};

class AGGRID_EXPORT AgCellEditValidator
{
public:
    virtual ~AgCellEditValidator();

    // 校验编辑器提交值
    // errorMessage 在返回 RejectKeepEditing 或 Reject 时可用于提示
    virtual AgEditValidationResult validate(
        const AgGridModel *model,
        int row,
        int column,
        const QVariant &oldValue,
        const QVariant &newValue,
        QString &errorMessage) const = 0;
};

#endif // AGCELLEDITVALIDATOR_H
