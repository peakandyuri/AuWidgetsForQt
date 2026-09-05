#ifndef DEMOSTYLE_H
#define DEMOSTYLE_H

#include "AgGridStyle.h"

class AgGridModel;

class DemoStyle : public AgGridStyle
{
public:
    QVariant value(StyleRole role, const AgGridModel *model, int row, int column) const override;
};

#endif // DEMOSTYLE_H
