#ifndef AUCELLEDITVALIDATOR_H
#define AUCELLEDITVALIDATOR_H

#include "AuGrid_global.h"

#include <QVariant>
#include <QString>

class AuGridModel;

/// 编辑提交的校验结果。
enum class AuEditValidationResult {
    Accept,            ///< 接受新值，关闭编辑器并更新模型
    Reject,            ///< 拒绝新值，关闭编辑器但不更新模型
    RejectKeepEditing  ///< 拒绝新值，保持编辑器打开并提示错误
};

/**
 * @brief 编辑提交校验器抽象基类。
 *
 * 通过 AuGridWidget::setCellEditValidator() 设置，对全部编辑提交生效。
 * 提交流程：编辑器取值 → validate() 裁决 → Accept 才写入模型。
 * 返回 RejectKeepEditing / Reject 时可给 errorMessage 赋值用于提示。
 *
 * 示例（整数范围校验）：
 * @code
 *   AuEditValidationResult validate(const AuGridModel *, int, int column,
 *                                   const QVariant &, const QVariant &newValue,
 *                                   QString &errorMessage) const override {
 *       if (column == 1 && newValue.toInt() < 0) {
 *           errorMessage = "不能为负数";
 *           return AuEditValidationResult::RejectKeepEditing;
 *       }
 *       return AuEditValidationResult::Accept;
 *   }
 * @endcode
 */
class AUGRID_EXPORT AuCellEditValidator
{
public:
    virtual ~AuCellEditValidator();

    // 校验编辑器提交值
    // errorMessage 在返回 RejectKeepEditing 或 Reject 时可用于提示
    /// 校验一次编辑提交（newValue 为编辑器读取值）；纯虚。
    virtual AuEditValidationResult validate(
        const AuGridModel *model,
        int row,
        int column,
        const QVariant &oldValue,
        const QVariant &newValue,
        QString &errorMessage) const = 0;
};

#endif // AUCELLEDITVALIDATOR_H
