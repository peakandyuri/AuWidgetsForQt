#ifndef AGMULTILINECELLEDITOR_H
#define AGMULTILINECELLEDITOR_H

#include "AgCellEditor.h"

/**
 * @brief 多行编辑器：内部使用 QPlainTextEdit。
 *
 * wantsReturnForNewLine() 返回 true：编辑期间 Return 键为换行，
 * 不提交编辑（区别于单行编辑器）。适用于长文本列。
 */
class AGGRID_EXPORT AgMultiLineCellEditor : public AgCellEditor
{
public:
    QWidget *createEditor(QWidget *parent) const override;
    void setEditorValue(QWidget *editor, const QVariant &value) const override;
    QVariant editorValue(QWidget *editor) const override;
    void setInitialTypedText(QWidget *editor, const QString &text) const override;
    bool wantsReturnForNewLine() const override;
};

#endif // AGMULTILINECELLEDITOR_H
