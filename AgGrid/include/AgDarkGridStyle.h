#ifndef AGDARKGRIDSTYLE_H
#define AGDARKGRIDSTYLE_H

#include "AgGridStyle.h"

class AGGRID_EXPORT AgDarkGridStyle : public AgGridStyle
{
public:
    AgDarkGridStyle();
    ~AgDarkGridStyle() override;

    QVariant value(StyleRole role, const AgGridModel *model, int row, int column) const override;
    AgGridStyle *clone() const override;
};

#endif // AGDARKGRIDSTYLE_H
