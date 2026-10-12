# AuPlot 帮助文档

`AuPlot` 是自绘曲线绘图控件：双轴坐标 + 背景网格 + 曲线 / 点 / 线 / 面图元 + 拖动 /
缩放 / 框选交互 + 内置图例与标题 + 追踪器十字线，面向大数据量与实时滚动场景。
不依赖 AuCommon，仅用 Qt Widgets。

功能总览：

- **坐标轴**：固定四条轴（X1/Y1 默认显示，X2/Y2 双轴可选），每轴独立的量程、
  刻度、刻度值、网格、轴标题；自适应量程 / 固定量程 / 交互硬边界三套语义；
- **图元**：曲线（线 + 点标记 + 面填充）、散点、折线、多边形、全幅参考线，
  `zLevel` 控制上下层级，控件接管所有权；
- **图例**：内置绘制，点击条目切换曲线显隐（隐藏置灰保留），位置可选四边预留
  或四角悬浮；开放名称 / 图标 / 信号三个扩展点支持自制外部图例；
- **交互**：拖动平移、滚轮缩放（方向与步长可配）、Ctrl+左键框选缩放、双击恢复
  自适应量程、追踪器（多曲线吸附 + 汇总 tip）、十字线；
- **大数据**：百万级点按像素列 min/max 抽稀（峰谷不丢），`addPoint` O(1) 追加，
  滚动窗口裁头；NaN 点自动断段且不污染量程；
- **导出**：`toImage()` 任意尺寸 / 缩放倍数 / 透明背景。

## 1. 快速上手

```cpp
AuPlot *plot = new AuPlot(this);

AuPlotCurve *curve = new AuPlotCurve;
curve->setName(QStringLiteral("温度"));            // 名字非空才进图例
for (int i = 0; i < 200; ++i) {
    double x = 10.0 * i / 199;
    curve->addPoint(x, qSin(x) * 3 + x * 0.5);
}
plot->addItem(curve);                              // 控件接管所有权

// 交互默认全部关闭，按需开启
plot->setDragEnabled(true);
plot->setZoomEnabled(true);
plot->setTrackerEnabled(true);                     // 多曲线吸附追踪 tip
```

默认单轴（底部 X1 + 左侧 Y1），白底浅灰虚线网格，曲线按调色板自动取色。
数据变化后自适应量程自动跟随，无需手动重绘；`replot()` 仅供外部直接改配置后强制刷新。

## 2. 坐标轴与量程语义

`AuPlot` 固定持有四条轴：`xAxis()`（底部 X1）、`yAxis()`（左侧 Y1）默认显示；
`xAxis2()`（顶部）、`yAxis2()`（右侧）通过 `setDualAxisEnabled(true)` 或
`axis2->setVisible(true)` 开启。图元用 `setXAxis() / setYAxis()` 绑定到某条轴。

每条轴（`AuPlotAxis`）的范围有三层语义，交互（拖动 / 缩放 / 框选）都被钳制在边界内：

| API | 语义 |
| --- | --- |
| `setAutoRange(true)` | 默认。范围随绑定图元数据自适应（两侧留白 `setRangePadding`） |
| `setRange(min, max)` | 固定为该范围，同时作为拖动 / 缩放的硬边界 |
| `setViewRange(min, max)` | 只推进可见范围、不设边界（实时滚动窗口推进用），可拖回看历史 |
| `setLimits(min, max)` | 不改变当前范围，只给交互加硬边界；`clearLimits()` 清除 |

注意：**拖动 / 缩放过的轴会自动脱离自适应量程**（否则会被数据弹回）。恢复方法：
`axis->setAutoRange(true)`，或一键 `plot->resetAxes()`（所有轴恢复自适应，双击绘图区
等效）。`resetAxes()` 同步生效，调用后立刻读 `minimum() / maximum()` 即为新值。

刻度：大刻度显示数值（数量自动按"好数"步长 1/2/5×10ⁿ 生成，也可 `setTickCount(n)` 固定）；
小刻度不显示数值（`setMinorTickCount` 控制密度）。`setTickLabelsVisible(false)` 隐藏数值；
`setLabelFormatter()` 自定义显示文本（如 1~7 → "周一"~"周日"）；`setLabelPrecision(n)`
强制小数位。

网格：X 轴网格是纵向线、Y 轴是横向线，逐轴独立开关 / 线型 / 颜色 / 线宽；
`setMinorGridVisible(true)` 开启次级密网格。

轴标题（常用于单位）：`plot->yAxis()->setTitle(QStringLiteral("°C"))`，X 轴横排、
Y 轴旋转 90°，布局自动预留空间；颜色 / 字体用样式里的 `axisTitleColor / axisTitleFont`。

## 3. 图元：曲线 / 点 / 线 / 面 / 参考线

五类图元都继承 `AuPlotItem`，通过 `addItem()` 加入（接管所有权），`removeItem() /
clearItems()` 销毁。`setZLevel()` 控制层级（默认 0，越小越先画；参考线不参与自适应量程）。

```cpp
// 曲线：线（实/虚/点线 + 自定义 dash）+ 点标记（12 种样式）+ 基线填充
AuPlotCurve *curve = new AuPlotCurve;
curve->setLineColor(Qt::red);
curve->setLineStyle(Qt::DashLine);            // 或 setDashPattern({4, 2}) 自定义间隔
curve->setPointStyle(AuPlotPointStyle::HollowCircle);
curve->setFillVisible(true);                  // 曲线与 baseline(默认 0) 之间填充
plot->addItem(curve);

// 散点 / 折线 / 多边形 / 全幅参考线
AuPlotPoints *points = new AuPlotPoints;      // 标注采样点、离群点
AuPlotLine   *line   = new AuPlotLine;        // 两点线段 / 折线（阈值线等）
AuPlotArea   *area   = new AuPlotArea;        // 多边形区域着色（自动闭合）
AuPlotRefLine *ref   = new AuPlotRefLine(AuPlotRefLine::Orientation::Horizontal);
ref->setValue(6.0);                           // 只需一个数据值，横贯全幅
```

**无效数据（NaN 断点）**：坐标为 NaN / Inf 的点视为断点——线与填充在该点断开、
点标记跳过、包围盒与自适应量程自动忽略。传感器断采直接填 `qQNaN()` 即可：

```cpp
curve->addPoint(t, ok ? value : qQNaN());     // 无效段自动断开
```

## 4. 图例

名字（`AuPlotItem::setName()`）非空的图元自动进入内置图例，无需创建 / 管理图例对象：

```cpp
curve->setName(QStringLiteral("温度"));
plot->legend()->setPosition(AuPlotLegendPos::Top);  // 底/左/右/四角悬浮（默认顶部）
plot->legend()->setClickToToggleEnabled(true);      // 默认开：点击条目切换显隐
plot->legend()->setMaxRows(2);                      // 换行行数 / 分列列数上限（0 不限）
plot->setLegendVisible(false);                      // 图例整体显隐（Q_PROPERTY）
```

- **点击切换**：点击条目切换对应图元可见性；隐藏图元的条目置灰半透明保留，
  可再次点击恢复。自适应量程随之收窄（隐藏的图元不参与量程统计）。
- **位置**：`Top / Bottom / Left / Right` 在绘图区外侧预留边距，不遮挡曲线；
  `FloatingTopLeft / TopRight / BottomLeft / BottomRight` 悬浮在绘图区角落
  （半透明圆角背景），不占空间。
- **样式**：`AuPlotStyle` 的 `legendFont / legendTextColor / legendDimmedTextColor /
  legendBackgroundColor / legendBorderColor`。
- **信号**：`legendEntryClicked(AuPlotItem *)`（点击条目，无论是否切换）与
  `itemVisibilityChanged(AuPlotItem *, bool)`（图元可见性变化，代码 setVisible 同样触发）。

**自制外部图例**（侧边工具栏、可勾选列表等）：关掉内置图例，用 `items()` 遍历
名字 / 颜色 / 可见性，配合上述两个信号同步状态即可。自定义图元子类覆盖虚函数
`drawLegendIcon(QPainter *, const QRectF &)` 即可定义图例图标（默认不绘制），
内置五类图元各自实现了线段 / 标记 / 填充块的采样图标。

## 5. 标题

```cpp
plot->setTitle(QStringLiteral("设备运行监测"));     // 图表标题，顶部居中
plot->xAxis()->setTitle(QStringLiteral("时间（s）")); // 轴标题（常放单位）
```

标题非空即显示并自动预留布局空间：图表标题在最外层顶部（刻度字体加粗，可用
`titleFont / titleColor` 改），置空收回空间。

## 6. 交互

```cpp
// 拖动平移 / 滚轮缩放（方向可精细化）
plot->setDragEnabled(true);
plot->setDragDirections(Qt::Horizontal);      // 只横向拖
plot->setZoomEnabled(true);
plot->setZoomDirections(Qt::Horizontal | Qt::Vertical);
plot->setZoomFactor(1.2);                     // 每格滚轮缩放步长

// 框选缩放（方向同样受 zoomDirections 约束）
plot->setRubberBandZoomEnabled(true);         // 默认关
plot->setRubberBandModifiers(Qt::ControlModifier); // 默认 Ctrl+左键；NoModifier=普通拖动
// Esc / 右键取消框选；选区某方向 < 4px 时该方向不缩放
```

- **双击绘图区**恢复所有轴自适应量程（`setDoubleClickResetEnabled(false)` 可关）；
- **追踪器** `setTrackerEnabled(true)`：以鼠标 x 吸附所有可见曲线最近数据点
  （升序二分查找），每条曲线一个色点标记，数值合并进同一 tip（锚定鼠标、边界
  自动翻转）；`trackerChanged(curve, point)` 信号实时外发；
  `setTrackerSnapOutOfRange(true)` 让追踪线离开曲线数据范围后仍吸附端点；
- **十字线** `setCrosshairEnabled(true)`，方向可配（`setCrosshairDirections`）。

## 7. 大数据量与实时滚动

- **抽稀**：x 升序数据二分定位可见区间；点数远超像素列时按列做 min/max 抽稀
  （每列保留首 / 最小 / 最大 / 末），百万级点流畅且峰谷形状不丢；
- **追加**：`addPoint()` 均摊 O(1)，包围盒增量维护；配合固定量程轴可支撑
  15ms/拍级别的滚动绘制；
- **滚动窗口**：`removeFirstPoints(n)` 裁掉头部旧数据；视图推进用
  `xAxis()->setViewRange(newStart, newEnd)`（不设边界，用户拖走可回看历史）；
- 密集曲线自动切换 1px 装饰笔快速路径（视觉无差别，耗时降一个量级）。

完整滚动窗口示例见 FeatureDemo 的"实时绘图与滚动窗口"页。

## 8. 样式

三种用法（与 AuGridStyle 一致）：

```cpp
// 1) 全局：所有未显式设置样式的 AuPlot 生效（接管所有权）
AuPlotStyle::setGlobalStyle(AuPlotStyle::createDarkStyle());

// 2) 控件独立（接管所有权），随后可逐项微调
plot->setStyle(AuPlotStyle::createDarkStyle());
plot->style()->setGridColor(QColor(0x3a, 0x3d, 0x42));

// 3) 内置主题工厂：createLightStyle()（默认白底）/ createDarkStyle()
```

可配置项：背景（支持透明与半透明）、绘图区背景、轴 / 刻度值 / 网格颜色与字体、
曲线调色板（新图元按序自动取色）、追踪器 tip、十字线、图例、标题、框选矩形。
颜色约定：**无效 QColor = 关闭该元素或跟随刻度色**（如背景无效 = 透明）。

## 9. 导出图片

```cpp
QImage img = plot->toImage(QSize(1600, 1000), 2.0);   // 指定尺寸 + 2 倍采样
img.save("report.png");

QImage png = plot->toImage(QSize(), 1.0, true);       // 当前尺寸、透明背景
```

- `size` 为空时用控件当前尺寸；导出会临时按目标尺寸重排（隐藏控件也适用）；
- `devicePixelRatio` 提升清晰度（如 2.0 = 物理像素翻倍），文字 / 线条无锯齿；
- `transparentBackground = true` 时不填充控件背景，适合贴到报告 / 深色底图。

## 10. 坐标映射

外部绘制或命中测试可用公开的坐标换算（按绑定轴的量程）：

```cpp
QRectF rect = plot->plotRect();                    // 主绘图区（控件局部坐标）
qreal px = plot->xToPixel(3.5);                    // 数据 x -> 像素（默认 X1 轴）
qreal py = plot->yToPixel(42.0, AuPlotAxisPos::Y2);// 指定用哪条轴
double x  = plot->pixelToX(px);
double y  = plot->pixelToY(py, AuPlotAxisPos::Y2);
```

## 11. 常见问题

- **曲线加了但看不见？** 检查数据是否含 NaN（全 NaN 曲线无量程）；轴被
  `setRange` 固定在别的范围时数据可能在可视区外。
- **拖动 / 缩放后量程不跟数据了？** 设计如此（防弹回），`resetAxes()` 或双击恢复。
- **图例条目太多挤占绘图区？** 设 `legend()->setMaxRows(n)` 限制行 / 列数，
  或改用 Floating 位置悬浮在角落。
- **点击图例没反应？** 确认 `legend()->clickToToggleEnabled()` 未被关；图例
  点击只对 `setName` 非空的图元生效。
- **框选和拖动冲突？** 框选需要按住修饰键（默认 Ctrl）；把
  `setRubberBandModifiers(Qt::NoModifier)` 后普通拖动即框选（拖动平移失效）。
- **导出图与屏幕不一致？** 导出按目标尺寸重排，边距 / 刻度数量会随尺寸自适应，
  属正常现象。
