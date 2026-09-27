#ifndef DEMOVALIDATOR_H
#define DEMOVALIDATOR_H

#include "AuCellEditValidator.h"

class AuGridModel;

class DemoValidator : public AuCellEditValidator
{
public:
    AuEditValidationResult validate(
        const AuGridModel *model,
        int row,
        int column,
        const QVariant &oldValue,
        const QVariant &newValue,
        QString &errorMessage) const override;
};

#endif // DEMOVALIDATOR_H
