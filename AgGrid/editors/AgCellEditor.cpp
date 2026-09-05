#include "AgCellEditor.h"

AgCellEditor::~AgCellEditor()
{
}

void AgCellEditor::setInitialTypedText(QWidget * /*editor*/, const QString & /*text*/) const
{
}

bool AgCellEditor::wantsReturnForNewLine() const
{
    return false;
}
