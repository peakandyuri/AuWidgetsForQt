#ifndef AGDEFAULTCELLEDITOR_H
#define AGDEFAULTCELLEDITOR_H

#include "AgGrid_global.h"
#include "AgCellEditor.h"

/**
 * @brief 默认单行编辑器：内部使用 QLineEdit。
 *
 * 未通过 AgColumnDef::setCellEditor() 指定编辑器的可编辑列使用该实现。
 * 直接输入字符触发编辑时，字符注入到行编辑开头。
 */
class AGGRID_EXPORT AgDefaultCellEditor : public AgCellEditor
{
public:
    QWidget *createEditor(QWidget *parent) const override;
    void setEditorValue(QWidget *editor, const QVariant &value) const override;
    QVariant editorValue(QWidget *editor) const override;
    void setInitialTypedText(QWidget *editor, const QString &text) const override;
};

#endif // AGDEFAULTCELLEDITOR_H
