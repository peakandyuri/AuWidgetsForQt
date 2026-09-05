#ifndef AGCOMBOBOXCELLEDITOR_H
#define AGCOMBOBOXCELLEDITOR_H

#include "AgGrid_global.h"
#include "AgCellEditor.h"

#include <QStringList>

class AGGRID_EXPORT AgComboBoxCellEditor : public AgCellEditor
{
public:
    explicit AgComboBoxCellEditor(const QStringList &items = QStringList());

    void setItems(const QStringList &items);
    QStringList items() const;

    QWidget *createEditor(QWidget *parent) const override;
    void setEditorValue(QWidget *editor, const QVariant &value) const override;
    QVariant editorValue(QWidget *editor) const override;
    void setInitialTypedText(QWidget *editor, const QString &text) const override;

private:
    QStringList m_items;
};

#endif // AGCOMBOBOXCELLEDITOR_H
