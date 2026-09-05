#ifndef DEMOVALIDATOR_H
#define DEMOVALIDATOR_H

#include "AgCellEditValidator.h"

class AgGridModel;

class DemoValidator : public AgCellEditValidator
{
public:
    AgEditValidationResult validate(
        const AgGridModel *model,
        int row,
        int column,
        const QVariant &oldValue,
        const QVariant &newValue,
        QString &errorMessage) const override;
};

#endif // DEMOVALIDATOR_H
