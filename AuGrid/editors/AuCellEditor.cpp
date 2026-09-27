#include "AuCellEditor.h"

AuCellEditor::~AuCellEditor()
{
}

void AuCellEditor::setInitialTypedText(QWidget * /*editor*/, const QString & /*text*/) const
{
}

bool AuCellEditor::wantsReturnForNewLine() const
{
    return false;
}
