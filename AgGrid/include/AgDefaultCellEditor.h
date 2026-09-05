#ifndef AGDEFAULTCELLEDITOR_H
#define AGDEFAULTCELLEDITOR_H

#include "AgGrid_global.h"
#include "AgCellEditor.h"

class AGGRID_EXPORT AgDefaultCellEditor : public AgCellEditor
{
public:
    QWidget *createEditor(QWidget *parent) const override;
    void setEditorValue(QWidget *editor, const QVariant &value) const override;
    QVariant editorValue(QWidget *editor) const override;
    void setInitialTypedText(QWidget *editor, const QString &text) const override;
};

#endif // AGDEFAULTCELLEDITOR_H
