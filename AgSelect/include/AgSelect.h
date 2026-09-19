#ifndef AGSELECT_H
#define AGSELECT_H

#include "AgSelect_global.h"

#include <QWidget>

/**
 * @brief 下拉选择控件（目标：替代 QComboBox）。
 *
 * 当前为工程骨架版本：仅包含保证动态库可编译、可链接的最小占位实现，
 * 组件 API 尚未实现。后续版本将在此类上补齐（参考 AG Grid 交互风格）：
 *  - 选项数据模型与自定义渲染；
 *  - 弹层下拉列表，支持滚动、搜索过滤、分组；
 *  - 单选 / 多选、可编辑（自动补全）模式；
 *  - 样式系统（与 AgGridStyle 一致的“角色 → 值”查询表）。
 */
class AGSELECT_EXPORT AgSelect : public QWidget
{
    Q_OBJECT

public:
    explicit AgSelect(QWidget *parent = nullptr);
    ~AgSelect();
};

#endif // AGSELECT_H
