# AgWidgetsForQt

一套受 [AG Grid](https://www.ag-grid.com/) 交互风格启发的 Qt Widgets 自绘控件库。全部控件基于 QPainter 手绘实现，不依赖 QSS 与 QML，包含高性能数据表格、下拉选择、日期选择等常用控件，并附带一个按功能拆分的完整示例程序。

## 模块组成

工程为 qmake subdirs 结构，各子项目均为独立动态库，输出到各自模块的 `bin` 目录：

| 模块 | 说明 |
| --- | --- |
| **AgCommon** | 公共基础库：极简风格自绘滚动条 `AgScrollBar`、带投影的通用弹层容器 `AgPopupPanel` |
| **AgGrid** | 核心控件：基于 QPainter 自绘的高性能数据表格 `AgGridWidget` |
| **AgSelect** | 下拉选择控件 `AgSelect`（替代 QComboBox） |
| **AgDatePicker** | 日期选择控件 `AgDatePicker`（替代 QCalendarWidget，目前为骨架版本，API 开发中） |
| **Example** | 示例程序：按功能拆分的多个演示窗口，覆盖各控件的主要特性 |

依赖关系：AgGrid、AgSelect 依赖 AgCommon；AgDatePicker 无依赖；Example 依赖以上全部库。

## 主要特性

### AgGridWidget 数据表格

- **虚拟化渲染**：仅绘制可视区及缓冲区内的行，支持十万行级数据
- **多级表头**：树形列定义，支持列停靠（左/右 Pin）、列拖拽排序与显隐动画
- **排序与过滤**：单列排序、多规则逻辑排序、自定义过滤回调
- **行选中**：单选 / 多选 / 不可选三种模式
- **单元格编辑**：可插拔编辑器（单行 / 多行 / 下拉框），配合提交校验器使用
- **单元格合并**：`setSpan`，语义参考 `QTableView::setSpan`
- **内置辅助列**：行号列、复选框列、行拖拽手柄列，支持行拖拽重排
- **树表格**：`AgTreeRow` 支持树形数据的展开 / 收起（带动画）
- **多表同步**：配合 `AgGridSyncGroup` 同步多个表格的滚动与列宽
- **加载遮罩**：`setLoading` 显示蒙层 + 旋转圆环 + 文案，淡入淡出
- **样式系统**：亮 / 暗主题，小 / 中 / 大三种尺寸密度；支持全局样式与控件独立样式；单元格渲染器、表头渲染器、编辑器均可自定义扩展

### AgSelect 下拉选择

- QComboBox 风格 API（`addItem` / `currentIndex` / `currentText` 等），迁移成本低
- 单选与多选（chips 标签块）模式，支持分组标题与禁用项
- 可编辑模式支持输入实时过滤，命中文字高亮
- 弹层带投影与展开动画，下方空间不足自动翻转，键盘完整导航（Up/Down/PageUp/Enter/Esc 等）

## 快速开始

环境要求：Qt 5.x（Widgets 模块）、qmake、支持 C++11 的编译器（Windows 下建议使用 MinGW 工具链）。

1. 用 Qt Creator 打开根目录的 `AgWidgetsForQt.pro`，工程会自动加载全部子项目；
2. 构建并运行 `Example`，即可通过各演示窗口查看控件功能；
3. 在自己的工程中引用时，将各模块的 `include` 目录加入头文件搜索路径，并链接对应 `bin` 目录下的库，具体写法可参考 `Example/Example.pro`。

基本用法示例：

```cpp
AgGridModel *model = new AgGridModel(this);
model->setColumnDefs({...});
model->setRowData(rows);

AgGridWidget *grid = new AgGridWidget(this);
grid->setModel(model);
```

## 目录结构

```
AgWidgetsForQt/
├── AgWidgetsForQt.pro   # 顶层 subdirs 工程文件
├── AgCommon/            # 公共基础部件（滚动条、弹层面板）
├── AgGrid/              # 数据表格控件（core / renderers / editors / style 等）
├── AgSelect/            # 下拉选择控件
├── AgDatePicker/        # 日期选择控件（开发中）
├── Example/             # 示例程序（demos / CustomAgGrid 自定义扩展示例）
└── docs/                # 帮助文档
```

## 文档

- [docs/AgGrid-帮助文档.md](docs/AgGrid-帮助文档.md)：AgGrid 控件详细用法
- [docs/AgSelect-帮助文档.md](docs/AgSelect-帮助文档.md)：AgSelect 控件详细用法

## 许可证

本项目基于 [MIT License](LICENSE) 开源。
