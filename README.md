# AuWidgetsForQt

一套基于 QPainter 自绘的 Qt Widgets 控件库。全部控件手绘实现，不依赖 QSS 与 QML，包含高性能数据表格、下拉选择、日期选择等常用控件，并附带一个按功能拆分的完整示例程序。

## 模块组成

工程为 qmake subdirs 结构，各子项目均为独立动态库，输出到各自模块的 `bin` 目录：

| 模块 | 说明 |
| --- | --- |
| **AuCommon** | 公共基础库：极简风格自绘滚动条 `AuScrollBar`、带投影的通用弹层容器 `AuPopupPanel` |
| **AuGrid** | 核心控件：基于 QPainter 自绘的高性能数据表格 `AuGridWidget` |
| **AuSelect** | 下拉选择控件 `AuSelect`（替代 QComboBox） |
| **AuDatePicker** | 日期选择控件 `AuDatePicker`（替代 QCalendarWidget） |
| **AuPlot** | 曲线绘制控件 `AuPlot`：双轴坐标、大/小刻度、背景网格、曲线/点/线/面图元、拖动缩放，支持百万级数据 |
| **AuWindowKit** | 无边框窗体库 `AuWindow`：系统 API 为主实现阴影 / 拖动 / 缩放 / 分屏，自绘标题栏（可插自定义组件）、Dialog 模态、圆角、任务栏与关闭拦截，支持继承定制与 .ui 直接装载 |
| **Example** | 示例程序：按功能拆分的多个演示窗口，覆盖各控件的主要特性 |
| **FeatureDemo** | 功能细化演示程序：左侧互斥切换控件，右侧按“一个功能一个 Tab”拆分演示，每页配套实现代码（PlainText），上手最直观 |

依赖关系：AuGrid、AuSelect、AuDatePicker 依赖 AuCommon；AuPlot 独立无依赖；AuWindowKit 独立无依赖；Example、FeatureDemo 依赖以上全部库；AuWindowKitDemo 依赖 AuWindowKit。

## 主要特性

### AuGridWidget 数据表格

- **虚拟化渲染**：仅绘制可视区及缓冲区内的行，支持十万行级数据
- **多级表头**：树形列定义，支持列停靠（左/右 Pin）、列拖拽排序与显隐动画
- **排序与过滤**：单列排序、多规则逻辑排序、自定义过滤回调
- **行选中**：单选 / 多选 / 不可选三种模式
- **单元格编辑**：可插拔编辑器（单行 / 多行 / 下拉框），配合提交校验器使用
- **单元格合并**：`setSpan`，语义参考 `QTableView::setSpan`
- **内置辅助列**：行号列、复选框列、行拖拽手柄列，支持行拖拽重排
- **树表格**：`AuTreeRow` 支持树形数据的展开 / 收起（带动画）
- **多表同步**：配合 `AuGridSyncGroup` 同步多个表格的滚动与列宽
- **加载遮罩**：`setLoading` 显示蒙层 + 旋转圆环 + 文案，淡入淡出
- **样式系统**：亮 / 暗主题，小 / 中 / 大三种尺寸密度；支持全局样式与控件独立样式；单元格渲染器、表头渲染器、编辑器均可自定义扩展

### AuSelect 下拉选择

- QComboBox 风格 API（`addItem` / `currentIndex` / `currentText` 等），迁移成本低
- 单选与多选（chips 标签块）模式，支持分组标题与禁用项
- 可编辑模式支持输入实时过滤，命中文字高亮
- 弹层带投影与展开动画，下方空间不足自动翻转，键盘完整导航（Up/Down/PageUp/Enter/Esc 等）

### AuDatePicker 日期选择

- 默认 `yyyy-MM-dd` 格式化显示，`setDisplayFormat()` 自定义；年 / 月 / 日三种选择粒度（`yyyy` / `yyyy-MM` / `yyyy-MM-dd`）
- **日期范围选择**（`SelectDateRange`）：左右双日历面板（右侧晚一个月、翻月联动），两次点击选起止日期，起点前后均可选（选之前的日期自动交换），淡蓝色范围带实时预览，编辑框开始 / 结束日期下方主色调下划线提示（带滑动动画），起止占位文本可自定义（`setRangePlaceholderText()`）
- **键盘操作**：面板展开时方向键上下左右移动焦点日期（跨月自动翻页）、PageUp / PageDown 切月、回车选中；月 / 年九宫格页同样支持；**Tab 在表单控件间切换，焦点控件显示明显焦点边框与淡色底**（AuSelect 一致）
- 日历面板 head / body / footer 三段式："xxxx年xx月" 年月分段可点击，Win10 风格年份 / 月份九宫格选择页，年份页整屏滚动动画（无滚动条）
- `setDateRange()` 限制可选范围，范围外日期灰显无悬停；前后月补齐日期灰显可点选
- 悬停编辑框出现实心圆圈 × 一键清除；footer 默认"今天 / 当月 / 今年"快捷项，支持左中右自定义链接与整体替换
- 亮 / 暗主题、小 / 中 / 大尺寸预设，编辑框高度可逐控件微调（`setEditorHeight()`）
- 默认字体微软雅黑（Au 控件统一约定，样式 `setFont()` 可覆盖）

### AuPlot 曲线绘制

- **双轴支持**：底部 X1 / 左侧 Y1 默认显示（单轴），顶部 X2 / 右侧 Y2 通过 `setDualAxisEnabled()` 开启，四条轴独立配置
- **刻度系统**：大 / 小刻度两级（小刻度为相邻大刻度间平分），刻度数量按轴像素长度自动控制或 `setTickCount()` 固定；支持 `setLabelFormatter()` 自定义显示值（如 1~7 显示星期）、`setTickLabelsVisible(false)` 隐藏数值
- **背景网格**：横向 / 纵向 / 两者，默认虚线，可配置实线、点线等（线型 / 颜色 / 线宽逐轴可调），支持更密的小刻度网格
- **大数据量**：x 升序数据二分定位可见区间 + 按像素列 min/max 抽稀（保留峰谷极值），百万级点流畅绘制
- **点线面图元**：`AuPlotCurve`（曲线）/ `AuPlotPoints`（散点）/ `AuPlotLine`（参考线）/ `AuPlotArea`（多边形面），坐标按轴数据值，`setZLevel()` 控制层级（默认曲线 0，负值在曲线下方，先画层级低的）
- **内置点样式**：实心 / 空心圆、方块、菱形、三角、十字架、X、带圈十字架等
- **线型**：实线 / 虚线 / 点线，虚线间隔与相位可用 `setDashPattern()` / `setDashOffset()` 精细控制
- **面填充**：内部填充色支持透明度，边框实线 / 虚线可选
- **交互**（默认关闭）：拖动平移（可精细化只允许横向或纵向）、鼠标滚轮以光标为中心缩放（灵敏度 `setZoomFactor()` 可调，默认每格 1.2；方向可用 `setZoomDirections()` 只缩放横向 / 纵向或两者一起）；轴 `setRange()` / `setLimits()` 后拖动缩放被钳制在该范围内，轴范围默认随数据自适应（`setAutoRange(true)` 可恢复）
- **追踪器与十字线**：`setTrackerEnabled()` 鼠标移入时以鼠标 x 同时吸附所有可见曲线的最近数据点（升序曲线二分查找，百万级数据流畅），每条曲线绘制一个空心圆标记，所有曲线的数值合并在同一个 tooltip 中逐行显示（行首色点区分曲线，锚定鼠标位置、可视区域放不下自动翻转），追踪线离开曲线数据范围后该曲线的标记与数值行自动隐藏（`setTrackerSnapOutOfRange(true)` 改为继续吸附端点），`trackerChanged()` 信号实时回调；`setCrosshairEnabled()` 横贯绘图区的十字线跟随鼠标（最多横竖两条，`setCrosshairDirections()` 可单独开启横线或纵线）；tooltip / 十字线颜色字体在样式中独立配置
- **实时绘图**：`addPoint()` O(1)（包围盒增量维护，百万点预填充仅 ~90ms），固定量程下单帧（追加 + 全量重绘）约 3ms，可支撑 15ms/拍 级高频追加；滚动窗口用 `xAxis()->setViewRange()` 推进可视区（不设交互边界，历史数据可拖动 / 缩放回看），`removeFirstPoints()` 裁剪头部旧数据防止内存无限增长
- **参考线**：`AuPlotRefLine` 横贯绘图区的水平 / 垂直全幅标线（阈值线、报警线），只需一个数据值，不参与自适应量程
- **样式**：白底默认，背景支持透明与任意颜色（含透明度），轴色 / 刻度值 / 网格 / 曲线 / 点线面颜色独立可控，新曲线按调色板自动取色；亮 / 暗主题工厂 + 全局样式 + 控件独立样式

## 快速开始

环境要求：Qt 5.x（Widgets 模块）、qmake、支持 C++11 的编译器（Windows 下建议使用 MinGW 工具链）。

1. 用 Qt Creator 打开根目录的 `AuWidgetsForQt.pro`，工程会自动加载全部子项目；
2. 构建并运行 `FeatureDemo`，可按“控件 → 功能 Tab → 演示 + 代码”逐个功能上手；运行 `Example` 可查看各控件特性的综合演示窗口；
3. 在自己的工程中引用时，将各模块的 `include` 目录加入头文件搜索路径，并链接对应 `bin` 目录下的库，具体写法可参考 `Example/Example.pro`。

基本用法示例：

```cpp
AuGridModel *model = new AuGridModel(this);
model->setColumnDefs({...});
model->setRowData(rows);

AuGridWidget *grid = new AuGridWidget(this);
grid->setModel(model);
```

## 目录结构

```
AuWidgetsForQt/
├── AuWidgetsForQt.pro   # 顶层 subdirs 工程文件
├── AuCommon/            # 公共基础部件（滚动条、弹层面板）
├── AuGrid/              # 数据表格控件（core / renderers / editors / style 等）
├── AuSelect/            # 下拉选择控件
├── AuDatePicker/        # 日期选择控件
├── AuPlot/              # 曲线绘制控件（坐标轴 / 曲线 / 点线面图元 / 样式）
├── AuWindowKit/         # 无边框窗体库（窗体容器 / 标题栏 / 样式）
├── AuWindowKitDemo/     # AuWindowKit 演示程序（特性 / Dialog / 继承与 ui 文件）
├── Example/             # 示例程序（demos / CustomAuGrid 自定义扩展示例）
├── FeatureDemo/         # 功能细化演示（一个功能一个 Tab + 实现代码）
└── docs/                # 帮助文档
```

## 文档

- [docs/AuGrid-帮助文档.md](docs/AuGrid-帮助文档.md)：AuGrid 控件详细用法
- [docs/AuSelect-帮助文档.md](docs/AuSelect-帮助文档.md)：AuSelect 控件详细用法
- [docs/AuDatePicker-帮助文档.md](docs/AuDatePicker-帮助文档.md)：AuDatePicker 控件详细用法
- [docs/AuWindowKit-帮助文档.md](docs/AuWindowKit-帮助文档.md)：AuWindowKit 无边框窗体详细用法

## 许可证

本项目基于 [MIT License](LICENSE) 开源。
