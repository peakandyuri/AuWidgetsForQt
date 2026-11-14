#ifndef DEMOSTYLE_H
#define DEMOSTYLE_H

#include "AuGridStyle.h"

class AuGridModel;

class DemoStyle : public AuGridStyle
{
public:
    QVariant value(StyleRole role, const AuGridModel *model, int row, int column) const override;
};

#endif // DEMOSTYLE_H
