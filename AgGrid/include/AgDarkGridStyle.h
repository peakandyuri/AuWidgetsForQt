#ifndef AGDARKGRIDSTYLE_H
#define AGDARKGRIDSTYLE_H

#include "AgGridStyle.h"

/**
 * @brief 暗色主题样式（构造即配置好暗色调色板）。
 *
 * 等价便捷写法：AgGridStyle::createDarkStyle()。
 * 用法：grid->setStyle(new AgDarkGridStyle()) 或
 *      AgGridStyle::setGlobalStyle(new AgDarkGridStyle())。
 */
class AGGRID_EXPORT AgDarkGridStyle : public AgGridStyle
{
public:
    AgDarkGridStyle();
    ~AgDarkGridStyle() override;

    /// 返回暗色主题下各角色的值。
    QVariant value(StyleRole role, const AgGridModel *model, int row, int column) const override;
    /// 深拷贝。
    AgGridStyle *clone() const override;
};

#endif // AGDARKGRIDSTYLE_H
