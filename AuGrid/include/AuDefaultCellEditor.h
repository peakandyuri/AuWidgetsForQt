#ifndef AUDEFAULTCELLEDITOR_H
#define AUDEFAULTCELLEDITOR_H

#include "AuGrid_global.h"
#include "AuCellEditor.h"

/**
 * @brief 默认单行编辑器：内部使用 QLineEdit。
 *
 * 未通过 AuColumnDef::setCellEditor() 指定编辑器的可编辑列使用该实现。
 * 直接输入字符触发编辑时，字符注入到行编辑开头。
 */
class AUGRID_EXPORT AuDefaultCellEditor : public AuCellEditor
{
public:
    QWidget *createEditor(QWidget *parent) const override;
    void setEditorValue(QWidget *editor, const QVariant &value) const override;
    QVariant editorValue(QWidget *editor) const override;
    void setInitialTypedText(QWidget *editor, const QString &text) const override;
};

#endif // AUDEFAULTCELLEDITOR_H
