#ifndef AUDARKGRIDSTYLE_H
#define AUDARKGRIDSTYLE_H

#include "AuGridStyle.h"

/**
 * @brief 暗色主题样式（构造即配置好暗色调色板）。
 *
 * 等价便捷写法：AuGridStyle::createDarkStyle()。
 * 用法：grid->setStyle(new AuDarkGridStyle()) 或
 *      AuGridStyle::setGlobalStyle(new AuDarkGridStyle())。
 */
class AUGRID_EXPORT AuDarkGridStyle : public AuGridStyle
{
public:
    AuDarkGridStyle();
    ~AuDarkGridStyle() override;

    /// 返回暗色主题下各角色的值。
    QVariant value(StyleRole role, const AuGridModel *model, int row, int column) const override;
    /// 深拷贝。
    AuGridStyle *clone() const override;
};

#endif // AUDARKGRIDSTYLE_H
