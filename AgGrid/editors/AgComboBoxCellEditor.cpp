#include "AgComboBoxCellEditor.h"

#include <QComboBox>

AgComboBoxCellEditor::AgComboBoxCellEditor(const QStringList &items)
    : m_items(items)
{
}

void AgComboBoxCellEditor::setItems(const QStringList &items)
{
    m_items = items;
}

QStringList AgComboBoxCellEditor::items() const
{
    return m_items;
}

QWidget *AgComboBoxCellEditor::createEditor(QWidget *parent) const
{
    QComboBox *comboBox = new QComboBox(parent);
    comboBox->addItems(m_items);
    comboBox->setFrame(false);
    return comboBox;
}

void AgComboBoxCellEditor::setEditorValue(QWidget *editor, const QVariant &value) const
{
    QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
    if (comboBox)
        comboBox->setCurrentText(value.toString());
}

QVariant AgComboBoxCellEditor::editorValue(QWidget *editor) const
{
    QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
    if (comboBox)
        return comboBox->currentText();
    return QVariant();
}

void AgComboBoxCellEditor::setInitialTypedText(QWidget *editor, const QString &text) const
{
    QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
    if (!comboBox)
        return;
    comboBox->setCurrentText(text);
}
