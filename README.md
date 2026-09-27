# AgWidgetsForQt

一套受 [AG Grid](https://www.ag-grid.com/) 交互风格启发的 Qt Widgets 自绘控件库。全部控件基于 QPainter 手绘实现，不依赖 QSS 与 QML，包含高性能数据表格、下拉选择、日期选择等常用控件，并附带一个按功能拆分的完整示例程序。

## 模块组成

工程为 qmake subdirs 结构，各子项目均为独立动态库，输出到各自模块的 `bin` 目录：

| 模块 | 说明 |
| --- | --- |
| **AgCommon** | 公共基础库：极简风格自绘滚动条 `AgScrollBar`、带投影的通用弹层容器 `AgPopupPanel` |
| **AgGrid** | 核心控件：基于 QPainter 自绘的高性能数据表格 `AgGridWidget` |
| **AgSelect** | 下拉选择控件 `AgSelect`（替代 QComboBox） |
| **AgDatePicker** | 日期选择控件 `AgDatePicker`（替代 QCalendarWidget） |
| **AgPlot** | 曲线绘制控件 `AgPlot`：双轴坐标、大/小刻度、背景网格、曲线/点/线/面图元、拖动缩放，支持百万级数据 |
| **AgWindowKit** | 无边框窗体库 `AgWindow`：系统 API 为主实现阴影 / 拖动 / 缩放 / 分屏，自绘标题栏（可插自定义组件）、Dialog 模态、圆角、任务栏与关闭拦截，支持继承定制与 .ui 直接装载 |
| **Example** | 示例程序：按功能拆分的多个演示窗口，覆盖各控件的主要特性 |
| **FeatureDemo** | 功能细化演示程序：左侧互斥切换控件，右侧按“一个功能一个 Tab”拆分演示，每页配套实现代码（PlainText），上手最直观 |

依赖关系：AgGrid、AgSelect、AgDatePicker 依赖 AgCommon；AgPlot 独立无依赖；AgWindowKit 独立无依赖；Example、FeatureDemo 依赖以上全部库；AgWindowKitDemo 依赖 AgWindowKit。

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

### AgDatePicker 日期选择

- 默认 `yyyy-MM-dd` 格式化显示，`setDisplayFormat()` 自定义；年 / 月 / 日三种选择粒度（`yyyy` / `yyyy-MM` / `yyyy-MM-dd`）
- **日期范围选择**（`SelectDateRange`）：左右双日历面板（右侧晚一个月、翻月联动），两次点击选起止日期，起点前后均可选（选之前的日期自动交换），淡蓝色范围带实时预览，编辑框开始 / 结束日期下方主色调下划线提示（带滑动动画），起止占位文本可自定义（`setRangePlaceholderText()`）
- **键盘操作**：面板展开时方向键上下左右移动焦点日期（跨月自动翻页）、PageUp / PageDown 切月、回车选中；月 / 年九宫格页同样支持；**Tab 在表单控件间切换，焦点控件显示明显焦点边框与淡色底**（AgSelect 一致）
- 日历面板 head / body / footer 三段式："xxxx年xx月" 年月分段可点击，Win10 风格年份 / 月份九宫格选择页，年份页整屏滚动动画（无滚动条）
- `setDateRange()` 限制可选范围，范围外日期灰显无悬停；前后月补齐日期灰显可点选
- 悬停编辑框出现实心圆圈 × 一键清除；footer 默认"今天 / 当月 / 今年"快捷项，支持左中右自定义链接与整体替换
- 亮 / 暗主题、小 / 中 / 大尺寸预设，编辑框高度可逐控件微调（`setEditorHeight()`）
- 默认字体微软雅黑（Ag 控件统一约定，样式 `setFont()` 可覆盖）

### AgPlot 曲线绘制

- **双轴支持**：底部 X1 / 左侧 Y1 默认显示（单轴），顶部 X2 / 右侧 Y2 通过 `setDualAxisEnabled()` 开启，四条轴独立配置
- **刻度系统**：大 / 小刻度两级（小刻度为相邻大刻度间平分），刻度数量按轴像素长度自动控制或 `setTickCount()` 固定；支持 `setLabelFormatter()` 自定义显示值（如 1~7 显示星期）、`setTickLabelsVisible(false)` 隐藏数值
- **背景网格**：横向 / 纵向 / 两者，默认虚线，可配置实线、点线等（线型 / 颜色 / 线宽逐轴可调），支持更密的小刻度网格
- **大数据量**：x 升序数据二分定位可见区间 + 按像素列 min/max 抽稀（保留峰谷极值），百万级点流畅绘制
- **点线面图元**：`AgPlotCurve`（曲线）/ `AgPlotPoints`（散点）/ `AgPlotLine`（参考线）/ `AgPlotArea`（多边形面），坐标按轴数据值，`setZLevel()` 控制层级（默认曲线 0，负值在曲线下方，先画层级低的）
- **内置点样式**：实心 / 空心圆、方块、菱形、三角、十字架、X、带圈十字架等
- **线型**：实线 / 虚线 / 点线，虚线间隔与相位可用 `setDashPattern()` / `setDashOffset()` 精细控制
- **面填充**：内部填充色支持透明度，边框实线 / 虚线可选
- **交互**（默认关闭）：拖动平移（可精细化只允许横向或纵向）、鼠标滚轮以光标为中心缩放（灵敏度 `setZoomFactor()` 可调，默认每格 1.2）；轴 `setRange()` / `setLimits()` 后拖动缩放被钳制在该范围内
- **追踪器与十字线**：`setTrackerEnabled()` 鼠标移入吸附最近曲线点（升序曲线二分查找，百万级数据流畅），绘制空心圆标记 + 引导线 + x/y 值 tooltip（值文本使用轴刻度格式化），`trackerChanged()` 信号实时回调；`setCrosshairEnabled()` 横贯绘图区的十字线跟随鼠标；tooltip / 十字线颜色字体在样式中独立配置
- **参考线**：`AgPlotRefLine` 横贯绘图区的水平 / 垂直全幅标线（阈值线、报警线），只需一个数据值，不参与自适应量程
- **样式**：白底默认，背景支持透明与任意颜色（含透明度），轴色 / 刻度值 / 网格 / 曲线 / 点线面颜色独立可控，新曲线按调色板自动取色；亮 / 暗主题工厂 + 全局样式 + 控件独立样式

## 快速开始

环境要求：Qt 5.x（Widgets 模块）、qmake、支持 C++11 的编译器（Windows 下建议使用 MinGW 工具链）。

1. 用 Qt Creator 打开根目录的 `AgWidgetsForQt.pro`，工程会自动加载全部子项目；
2. 构建并运行 `FeatureDemo`，可按“控件 → 功能 Tab → 演示 + 代码”逐个功能上手；运行 `Example` 可查看各控件特性的综合演示窗口；
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
├── AgDatePicker/        # 日期选择控件
├── AgPlot/              # 曲线绘制控件（坐标轴 / 曲线 / 点线面图元 / 样式）
├── AgWindowKit/         # 无边框窗体库（窗体容器 / 标题栏 / 样式）
├── AgWindowKitDemo/     # AgWindowKit 演示程序（特性 / Dialog / 继承与 ui 文件）
├── Example/             # 示例程序（demos / CustomAgGrid 自定义扩展示例）
├── FeatureDemo/         # 功能细化演示（一个功能一个 Tab + 实现代码）
└── docs/                # 帮助文档
```

## 文档

- [docs/AgGrid-帮助文档.md](docs/AgGrid-帮助文档.md)：AgGrid 控件详细用法
- [docs/AgSelect-帮助文档.md](docs/AgSelect-帮助文档.md)：AgSelect 控件详细用法
- [docs/AgDatePicker-帮助文档.md](docs/AgDatePicker-帮助文档.md)：AgDatePicker 控件详细用法
- [docs/AgWindowKit-帮助文档.md](docs/AgWindowKit-帮助文档.md)：AgWindowKit 无边框窗体详细用法

## 许可证

本项目基于 [MIT License](LICENSE) 开源。
