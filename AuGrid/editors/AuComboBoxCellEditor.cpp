#include "AuComboBoxCellEditor.h"

#include <QComboBox>

AuComboBoxCellEditor::AuComboBoxCellEditor(const QStringList &items)
    : m_items(items)
{
}

void AuComboBoxCellEditor::setItems(const QStringList &items)
{
    m_items = items;
}

QStringList AuComboBoxCellEditor::items() const
{
    return m_items;
}

QWidget *AuComboBoxCellEditor::createEditor(QWidget *parent) const
{
    QComboBox *comboBox = new QComboBox(parent);
    comboBox->addItems(m_items);
    comboBox->setFrame(false);
    return comboBox;
}

void AuComboBoxCellEditor::setEditorValue(QWidget *editor, const QVariant &value) const
{
    QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
    if (comboBox)
        comboBox->setCurrentText(value.toString());
}

QVariant AuComboBoxCellEditor::editorValue(QWidget *editor) const
{
    QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
    if (comboBox)
        return comboBox->currentText();
    return QVariant();
}

void AuComboBoxCellEditor::setInitialTypedText(QWidget *editor, const QString &text) const
{
    QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
    if (!comboBox)
        return;
    comboBox->setCurrentText(text);
}
