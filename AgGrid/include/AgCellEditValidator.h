#ifndef AGCELLEDITVALIDATOR_H
#define AGCELLEDITVALIDATOR_H

#include "AgGrid_global.h"

#include <QVariant>
#include <QString>

class AgGridModel;

/// 编辑提交的校验结果。
enum class AgEditValidationResult {
    Accept,            ///< 接受新值，关闭编辑器并更新模型
    Reject,            ///< 拒绝新值，关闭编辑器但不更新模型
    RejectKeepEditing  ///< 拒绝新值，保持编辑器打开并提示错误
};

/**
 * @brief 编辑提交校验器抽象基类。
 *
 * 通过 AgGridWidget::setCellEditValidator() 设置，对全部编辑提交生效。
 * 提交流程：编辑器取值 → validate() 裁决 → Accept 才写入模型。
 * 返回 RejectKeepEditing / Reject 时可给 errorMessage 赋值用于提示。
 *
 * 示例（整数范围校验）：
 * @code
 *   AgEditValidationResult validate(const AgGridModel *, int, int column,
 *                                   const QVariant &, const QVariant &newValue,
 *                                   QString &errorMessage) const override {
 *       if (column == 1 && newValue.toInt() < 0) {
 *           errorMessage = "不能为负数";
 *           return AgEditValidationResult::RejectKeepEditing;
 *       }
 *       return AgEditValidationResult::Accept;
 *   }
 * @endcode
 */
class AGGRID_EXPORT AgCellEditValidator
{
public:
    virtual ~AgCellEditValidator();

    // 校验编辑器提交值
    // errorMessage 在返回 RejectKeepEditing 或 Reject 时可用于提示
    /// 校验一次编辑提交（newValue 为编辑器读取值）；纯虚。
    virtual AgEditValidationResult validate(
        const AgGridModel *model,
        int row,
        int column,
        const QVariant &oldValue,
        const QVariant &newValue,
        QString &errorMessage) const = 0;
};

#endif // AGCELLEDITVALIDATOR_H
