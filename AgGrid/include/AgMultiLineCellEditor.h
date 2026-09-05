#ifndef AGMULTILINECELLEDITOR_H
#define AGMULTILINECELLEDITOR_H

#include "AgCellEditor.h"

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
