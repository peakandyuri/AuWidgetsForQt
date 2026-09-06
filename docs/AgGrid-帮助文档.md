# AgGrid for Qt 使用帮助文档

基于 QPainter 自绘的高性能数据表格控件（参考 AG Grid 交互风格），适用于 Qt 5 / C++11 及以上，支持十万行级数据虚拟化渲染。

## 目录

- [1. 功能特性](#1-功能特性)
- [2. 构建与集成](#2-构建与集成)
- [3. 快速上手](#3-快速上手)
- [4. 核心概念与坐标体系](#4-核心概念与坐标体系)
- [5. AgGridModel 数据模型](#5-aggridmodel-数据模型)
- [6. AgColumnDef 列定义](#6-agcolumndef-列定义)
- [7. AgGridWidget 表格控件](#7-aggridwidget-表格控件)
- [8. 单元格合并（Span）专题](#8-单元格合并span专题)
- [9. 样式系统：AgGridStyle / AgDarkGridStyle](#9-样式系统aggridstyle--agdarkgridstyle)
- [10. 单元格渲染器](#10-单元格渲染器)
- [11. 表头渲染器](#11-表头渲染器)
- [12. 单元格编辑器](#12-单元格编辑器)
- [13. 编辑校验器](#13-编辑校验器)
- [14. 多表格同步：AgGridSyncGroup](#14-多表格同步aggridsyncgroup)
- [15. 信号速查表](#15-信号速查表)
- [16. 样式键（StyleRole）速查表](#16-样式键stylerole速查表)
- [17. 常见问题与注意事项](#17-常见问题与注意事项)

---

## 1. 功能特性

- **虚拟化渲染**：只绘制可视区（含缓冲区）内的行，轻松支持十万行级数据
- **多级表头**：列定义支持树形分组，自动展开为多级表头
- **列宽模式**：固定像素（Fixed）/ 内容自适应（AutoFit）/ 按权重分配剩余空间（Flex）
- **列停靠（Pin）**：列可停靠到左侧 / 右侧，滚动时保持可见
- **排序 / 过滤**：单列排序、多规则逻辑排序、自定义过滤回调
- **单元格编辑**：双击 / 回车 / 直接输入字符进入编辑；内置单行、多行、下拉三种编辑器；支持自定义编辑器与提交校验
- **单元格合并**：类似 `QTableView::setSpan` 的行列合并能力
- **内置辅助列**：可选的行号列、复选框列、行拖拽手柄列
- **行拖拽排序**：拖动手柄整行拖动重排，带动画与自动滚动
- **列拖拽排序 / 显隐动画**：任意层级的列可拖动重排，列显隐带位移动画
- **多表格同步**：多个表格的滚动与列宽可互相同步（数据对比场景）
- **样式系统**：全局样式 / 独立样式、浅色与暗色主题、小中大三种尺寸模式、支持按行列上下文自定义样式值
- **自定义渲染**：单元格 / 表头渲染器、编辑器均为接口 + 注册表设计，可任意扩展

---

## 2. 构建与集成

仓库根目录 `AgGridForQt.pro` 为子目录工程，包含两个子项目：

| 子项目 | 说明 |
|---|---|
| `AgGrid` | 控件库（默认编译为动态库 `AgGrid.dll`，输出到 `AgGrid/bin/`） |
| `Example` | 功能演示程序 |

命令行构建（Qt 5.12 + MinGW 示例）：

```bash
mkdir build && cd build
qmake ../AgGridForQt.pro
mingw32-make -j4
```

在自己的工程中集成：

1. 头文件包含路径添加 `AgGrid/include`
2. 链接 `AgGrid/bin` 下的导入库（Windows 为 `-lAgGrid`）
3. **运行时**确保 `AgGrid.dll` 位于 exe 同目录（Windows 加载器优先搜索 exe 所在目录；`Example.pro` 已内置构建后自动拷贝脚本，可直接参考）

所有公开类均以 `AGGRID_EXPORT` 导出，声明于 `AgGrid_global.h`。

---

## 3. 快速上手

```cpp
#include "AgGridModel.h"
#include "AgGridWidget.h"

// 1. 构建模型
AgGridModel *model = new AgGridModel(this);

QVector<AgColumnDef> columns;
AgColumnDef name("name", "姓名");          // field, 表头名（缺省同 field）
name.setWidth(120);                        // 固定列宽
name.setSortable(true);                    // 允许点击表头排序
columns.append(name);

AgColumnDef age("age", "年龄");
age.setWidthMode(AgColumnDef::ColumnWidthMode::Flex);  // 按权重占满剩余宽度
age.setFlexWeight(1);
age.setEditable(true);                     // 允许编辑
columns.append(age);

model->setColumnDefs(columns);

// 2. 填充数据：每行是一个 QVariantMap，键为 field
QVector<QVariantMap> rows;
for (int i = 0; i < 1000; ++i) {
    rows.append({ {"name", QString("用户%1").arg(i)}, {"age", 20 + i % 30} });
}
model->setRowData(rows);

// 3. 创建表格
AgGridWidget *grid = new AgGridWidget(this);
grid->setModel(model);
layout()->addWidget(grid);
```

未显式调用 `setStyle()` 的表格自动使用全局样式（默认浅色主题）。切换暗色主题：

```cpp
AgGridStyle::setGlobalStyle(AgGridStyle::createDarkStyle());
```

---

## 4. 核心概念与坐标体系

### 4.1 数据流

```
QVector<QVariantMap> 原始数据
        │  setRowData()
        ▼
m_originalRowData（原始行，索引称 originalRow）
        │  过滤回调 + 排序规则
        ▼
m_rowData（可见行，索引称 visibleRow，通过 m_visibleToOriginal / m_originalToVisible 双向映射）
```

- **visibleRow（可见行号）**：`AgGridWidget` 中绝大多数接口（选择、滚动、合并、编辑等）使用当前可见行号。
- **originalRow（原始行号）**：与数据项一一对应，不随排序过滤变化。**勾选状态、行拖拽信号**使用原始行号，保证排序/过滤后勾选不串行。

### 4.2 列索引

- 列定义是树（分组列 + 叶子列），**叶子列按从左到右的顺序编号**，`AgGridWidget` / `AgGridModel` 接口中的 `column` 均指叶子列索引。
- 隐藏列宽度按 0 处理，但索引不变。

### 4.3 特殊列编号

`AgGridWidget` 中以下负值表示内置辅助列（用于事件/命中测试）：

| 值 | 含义 |
|---|---|
| `-1` | 复选框列（`kCheckboxColumn`） |
| `-2` | 行拖拽手柄列（`kDragHandleColumn`） |
| `-3` | 行号列（`kRowNumberColumn`） |

### 4.4 键盘与鼠标交互

| 操作 | 行为 |
|---|---|
| 单击单元格 | 选中该格，发出 `cellClicked`；Ctrl+单击取消选中 |
| 双击 / Enter / F2 | 开始编辑（可编辑列） |
| 直接输入可打印字符 | 以该字符为初始值开始编辑 |
| 方向键 | 移动选中格（自动跳过隐藏列与合并覆盖格） |
| Tab / Backtab | 跳到下一个 / 上一个可编辑格 |
| Ctrl+C | 复制当前选中格内容 |
| Space | 复选框列 / 复选框单元格切换勾选 |
| 表头单击 | 排序（列 `sortable` 且全局排序开启时） |
| 表头边缘拖动 | 调整列宽（列 `resizable` 时） |
| 表头拖动 | 列重排（任意层级分组可整体拖动） |
| 拖拽手柄列拖动 | 行拖拽重排 |
| 右键 | 发出 `cellRightClicked` / `headerRightClicked` |

---

## 5. AgGridModel 数据模型

**头文件**：`AgGrid/include/AgGridModel.h`
**基类**：`QObject`

数据模型，负责列定义、行数据、过滤排序、行列宽。与视图解耦，多个 `AgGridWidget` 可共享同一个模型。

### 5.1 列定义

| 函数 | 说明 |
|---|---|
| `void setColumnDefs(const QVector<AgColumnDef> &defs)` | 设置列定义树（可为分组列），触发 `columnsChanged` + `layoutChanged` |
| `const QVector<AgColumnDef> &topLevelColumnDefs() const` | 顶层列定义列表 |
| `int columnCount() const` | 叶子列数量 |
| `int headerLevelCount() const` | 表头层级数（无分组时为 1） |
| `int columnIndexForField(const QString &field) const` | field → 叶子列索引；不存在返回 -1 |
| `QVector<QString> leafFieldsForField(const QString &field) const` | 返回某列（含分组列）下的全部叶子 field，按顺序 |
| `QString fieldAtColumn(int column) const` | 叶子列索引 → field |
| `AgColumnDef columnDefAt(int column) const` | 叶子列索引 → 列定义（返回拷贝） |
| `QVariant headerValue(int column) const` | 叶子列的表头文字（`headerName()`） |

### 5.2 行数据

| 函数 | 说明 |
|---|---|
| `void setRowData(const QVector<QVariantMap> &rows)` | 设置数据（深拷贝为原始数据），随后立即应用过滤排序，发出 `rowsChanged` + `layoutChanged` |
| `const QVector<QVariantMap> &rowData() const` | **可见**数据（过滤排序后） |
| `const QVector<QVariantMap> &originalRowData() const` | **原始**数据 |
| `int rowCount() const` | 可见行数 |
| `int originalRowCount() const` | 原始行数 |
| `int originalRowForVisibleRow(int visibleRow) const` | 可见行 → 原始行；无效返回 -1 |
| `int visibleRowForOriginalRow(int originalRow) const` | 原始行 → 可见行；被过滤隐藏返回 -1 |

### 5.3 单元格读写

| 函数 | 说明 |
|---|---|
| `QVariant cellValue(int row, int column) const` | 按可见行 + 叶子列读值 |
| `QVariant cellValue(int row, const QString &field) const` | 按可见行 + field 读值 |
| `bool setCellValue(int row, const QString &field, const QVariant &value)` | 写值（修改原始数据），成功发出 `dataChanged`；row 为可见行 |
| `bool setCellValue(int row, int column, const QVariant &value)` | 同上，按叶子列索引 |

### 5.4 列宽与停靠

| 函数 | 说明 |
|---|---|
| `void setColumnWidth(int column, int width)` / `void setColumnWidth(const QString &field, int width)` | 设置列宽（仅对 Fixed 模式有意义），发出 `columnWidthsChanged` |
| `int columnWidth(int column) const` / `int columnWidth(const QString &field) const` | 读取列宽 |
| `void setColumnPinMode(int column, AgColumnDef::PinMode mode)` / `void setColumnPinMode(const QString &field, ...)` | 设置列停靠（field 可指向分组列，整体停靠） |
| `AgColumnDef::PinMode columnPinMode(int column) const` | 读取停靠状态 |

### 5.5 排序与过滤

| 函数 | 说明 |
|---|---|
| `void sort(int column, Qt::SortOrder order)` | 单列排序 |
| `void sort(const QList<AgSortRule> &rules)` | 多规则逻辑排序（按顺序依次比较）；空列表等价 `clearSort()` |
| `void clearSort()` | 清除排序（恢复原始顺序） |
| `void setFilterCallback(const std::function<bool(const QVariantMap &row)> &callback)` | 设置过滤回调：**返回 true 保留该行** |
| `void clearFilterCallback()` | 清除过滤 |
| `bool hasFilterCallback() const` | 是否设置了过滤回调 |

排序 / 过滤立即生效并重建可见映射，发出 `rowsChanged` + `layoutChanged`。

### 5.6 行移动与清理

| 函数 | 说明 |
|---|---|
| `void moveRows(const QVector<int> &fromOriginalIndices, int targetOriginalIndex)` | 把一批**原始行**移动到目标原始行索引之前（行拖拽的模型侧入口），发出 `rowsMoved` + `rowsChanged` + `layoutChanged` |
| `void clear()` | 清空全部数据与列定义 |

### 5.7 信号

| 信号 | 触发时机 |
|---|---|
| `dataChanged(topRow, leftColumn, bottomRow, rightColumn)` | `setCellValue` 成功后（可见行坐标） |
| `rowsChanged()` | 可见行集合内容变化（数据、过滤、排序、移动） |
| `rowsMoved(fromOriginalIndices, targetOriginalIndex)` | `moveRows` 完成后 |
| `columnsChanged()` | 列定义被替换 |
| `columnWidthsChanged()` | 列宽变化 |
| `layoutChanged()` | 需要重建布局的大变化（setRowData / setColumnDefs / 排序过滤 / 行移动） |
| `filterChanged()` | 过滤状态变化（由视图转发为同名信号） |

---

## 6. AgColumnDef 列定义

**头文件**：`AgGrid/include/AgColumnDef.h`
值类型（可拷贝），描述一列（或一个分组列）的全部属性。

### 6.1 构造

```cpp
AgColumnDef();                                                     // 全默认值
explicit AgColumnDef(const QString &field, const QString &headerName = QString());
```

`headerName` 缺省时使用 `field`。

### 6.2 枚举

```cpp
enum class ColumnWidthMode { Fixed, AutoFit, Flex };   // 列宽模式
enum class PinMode        { None, Left, Right };       // 停靠模式
enum class TextWrapMode   { NoWrap, NoWrapTruncate, NoWrapOverflow, WordWrap };
```

- `Fixed`：使用 `width()` 的固定像素值
- `AutoFit`：按表头与**可视内容**自适应（只增不减，滚动时按需增长，避免抖动）
- `Flex`：按 `flexWeight()` 权重分享剩余空间，受 `minWidth()` 约束

- `NoWrap`：单行裁剪；`NoWrapTruncate`：单行省略号；`NoWrapOverflow`：单行溢出显示
- `WordWrap`：自动换行，行高按内容自适应（渲染器 `preferredHeight` 参与行高计算）

### 6.3 成员函数与默认值

| 属性 | 读 / 写 | 默认值 | 说明 |
|---|---|---|---|
| field | `field()` / `setField()` | 空 | 数据键名，行 `QVariantMap` 的键 |
| headerName | `headerName()` / `setHeaderName()` | 同 field | 表头文字 |
| width | `width()` / `setWidth()` | 120 | Fixed 模式列宽（像素） |
| minWidth | `minWidth()` / `setMinWidth()` | 40 | 最小列宽（AutoFit/Flex/手动调整均受约束） |
| maxWidth | `maxWidth()` / `setMaxWidth()` | 0（不限制） | 最大列宽 |
| widthMode | `widthMode()` / `setWidthMode()` | Fixed | 列宽模式 |
| flexWeight | `flexWeight()` / `setFlexWeight()` | 1 | Flex 权重 |
| sortable | `isSortable()` / `setSortable()` | false | 点击表头排序 |
| editable | `isEditable()` / `setEditable()` | false | 允许编辑（复选框列表示可点击切换） |
| filterable | `isFilterable()` / `setFilterable()` | false | 预留 |
| visible | `isVisible()` / `setVisible()` | true | 初始可见性（运行时用 `AgGridWidget::setColumnVisible`） |
| resizable | `isResizable()` / `setResizable()` | true | 允许拖动调宽 / 参与 AutoFit |
| pinMode | `pinMode()` / `setPinMode()` | None | 左/右停靠 |
| cellRenderer | `cellRenderer()` / `setCellRenderer()` | 空 | 单元格渲染器注册名；为空用默认渲染器；checkbox 列强制用内置复选框渲染器 |
| headerRenderer | `headerRenderer()` / `setHeaderRenderer()` | 空 | 表头渲染器注册名；为空绘制标准表头 |
| cellEditor | `cellEditor()` / `setCellEditor()` | 空 | 编辑器注册名；为空用单行编辑器 |
| checkbox | `isCheckbox()` / `setCheckbox()` | false | 复选框列：值按 bool 渲染为复选框，点击/空格直接切换 |
| alignment | `alignment()` / `setAlignment()` | AlignLeft\|AlignVCenter | 单元格文字对齐 |
| textWrapMode | `textWrapMode()` / `setTextWrapMode()` | NoWrap | 换行模式 |
| headerFont | `headerFont()` / `setHeaderFont()` / `hasHeaderFont()` | 未设置 | 表头字体（覆盖样式默认） |
| headerForegroundColor | `headerForegroundColor()` / `setHeaderForegroundColor()` / `hasHeaderForegroundColor()` | 未设置 | 表头前景色（覆盖样式默认） |
| headerAlignment | `headerAlignment()` / `setHeaderAlignment()` / `hasHeaderAlignment()` | AlignCenter | 表头对齐（覆盖样式默认） |

### 6.4 分组（多级表头）

| 函数 | 说明 |
|---|---|
| `bool isGroup() const` | 是否分组列（有子列即为分组） |
| `const QVector<AgColumnDef> &children() const` | 子列列表 |
| `void setChildren(const QVector<AgColumnDef> &children)` | 设置子列 |
| `void addChild(const AgColumnDef &child)` | 追加子列 |
| `int childCount() const` | 子列数量 |

分组列的 `headerName` 显示为跨子列的表头节点，可整体停靠、隐藏、拖动。

---

## 7. AgGridWidget 表格控件

**头文件**：`AgGrid/include/AgGridWidget.h`
**基类**：`QWidget`

### 7.1 构造与模型

| 函数 | 说明 |
|---|---|
| `explicit AgGridWidget(QWidget *parent = nullptr)` | 构造；默认使用全局样式，`visibleRowBuffer=3`，开启鼠标追踪与键盘焦点 |
| `void setModel(AgGridModel *model)` | 绑定模型（不获得所有权）；切换模型会清空选择、勾选、合并区域与隐藏列状态 |
| `AgGridModel *model() const` | 当前模型 |

### 7.2 几何

| 函数 | 说明 |
|---|---|
| `int headerHeight() const` | 表头总高（层级数 × 单级高度） |
| `int headerLevelHeight() const` | 单级表头高度 |
| `int rowHeight() const` | 默认行高（样式键 `RowHeight`）；启用 WordWrap 时个别行可更高 |
| `int visibleRowBuffer() const` / `void setVisibleRowBuffer(int buffer)` | 可视区上下额外渲染的缓冲行数（默认 3） |

### 7.3 样式与尺寸模式

| 函数 | 说明 |
|---|---|
| `void setStyle(AgGridStyle *style)` | 设置独立样式（控件接管所有权）；传 `nullptr` 恢复使用全局样式 |
| `AgGridStyle *style() const` | 当前样式 |
| `void setSizeMode(SizeMode mode)` / `SizeMode sizeMode() const` | 小 / 中 / 大三种尺寸模式（Small/Medium/Large），内部克隆样式并调整高度与字体；未显式 `setStyle` 时跟随全局样式的尺寸变化 |

### 7.4 渲染器 / 编辑器注册

| 函数 | 说明 |
|---|---|
| `void registerCellRenderer(const QString &name, QSharedPointer<AgCellRenderer> renderer)` | 注册单元格渲染器，供 `AgColumnDef::setCellRenderer(name)` 引用 |
| `void registerCellEditor(const QString &name, QSharedPointer<AgCellEditor> editor)` | 注册编辑器，供 `AgColumnDef::setCellEditor(name)` 引用 |
| `void registerHeaderRenderer(const QString &name, QSharedPointer<AgHeaderRenderer> renderer)` | 注册表头渲染器，供 `AgColumnDef::setHeaderRenderer(name)` 引用 |

内置默认：单行编辑器（`AgDefaultCellEditor`）、多行编辑器（`AgMultiLineCellEditor`）、复选框单元格渲染器。未注册名的列使用默认文本渲染器/编辑器。

### 7.5 列宽与自适应

| 函数 | 说明 |
|---|---|
| `void setColumnWidth(int column, int width)` / `void setColumnWidth(const QString &field, int width)` | 设置列宽（转发模型） |
| `void autoSizeColumn(int column)` | 按内容自适应该列宽度 |
| `void autoSizeAllColumns()` | 自适应全部列 |

AutoFit 列在滚动、数据变化时自动增量测量（只增不减）。

### 7.6 编辑

| 函数 | 说明 |
|---|---|
| `bool editCell(int row, int column)` | 程序化开始编辑；返回是否成功 |
| `bool editCell(int row, int column, const QString &initialText)` | 同上，带初始文本（对应键盘直接输入） |
| `bool isEditing() const` | 是否有编辑器打开 |
| `void closeEditor(bool commit = true)` | 关闭编辑器；`commit=true` 提交（经校验），否则放弃 |

编辑合并单元格时始终编辑锚点数据。编辑器打开时滚动会自动跟随并裁剪到可视区。

### 7.7 滚动与定位

```cpp
enum class ScrollHint { EnsureVisible, PositionTop, PositionCenter, PositionBottom };
```

| 函数 | 说明 |
|---|---|
| `void scrollToRow(int row, ScrollHint hint = EnsureVisible)` | 垂直定位某行 |
| `void scrollToColumn(int column, ScrollHint hint = EnsureVisible)` | 水平定位某列 |
| `void scrollToCell(int row, int column, ScrollHint hint = EnsureVisible)` | 定位单元格；合并单元格按整个合并区域计算 |
| `int verticalOffset() const` / `void setVerticalOffset(int value)` | 垂直滚动像素偏移，可直接读写用于外部同步 |
| `int horizontalOffset() const` / `void setHorizontalOffset(int value)` | 水平滚动像素偏移 |

相关信号：`verticalScrollChanged(int)`、`horizontalScrollChanged(int)`。

### 7.8 列停靠 / 隐藏

| 函数 | 说明 |
|---|---|
| `void pinColumn(int column, AgColumnDef::PinMode mode)` / `void pinColumn(const QString &field, ...)` | 停靠 / 取消停靠（field 可指向分组列） |
| `AgColumnDef::PinMode columnPinMode(int column) const` | 查询停靠状态 |
| `void setColumnVisible(int column, bool visible)` / `void setColumnVisible(const QString &field, bool visible)` | 隐藏 / 显示列（带位移动画；field 可指向分组列整体显隐） |
| `bool isColumnVisible(int column) const` | 查询可见性 |

隐藏列宽度按 0 参与布局；停靠列不会滑入其他停靠区域。合并区域不会跨越停靠区域边界（`setSpan` 会自动截断）。

### 7.9 选择

| 函数 | 说明 |
|---|---|
| `void selectRow(int row)` | 选中行（保留当前列） |
| `void selectCell(int row, int column)` | 选中单元格；落在合并覆盖格上时自动映射到锚点 |
| `void clearSelection()` | 取消选中 |
| `void syncSelectionAfterLayoutChange()` | 布局变化后按原始行恢复选中（内部自动调用） |
| `int selectedRow() const` / `int selectedColumn() const` | 当前选中（-1 表示无） |

信号：`selectionChanged()`。

### 7.10 单元格合并

```cpp
void setSpan(int row, int column, int rowSpanCount, int columnSpanCount);
void clearSpan(int row, int column);
void clearSpans();
int  rowSpan(int row, int column) const;
int  columnSpan(int row, int column) const;
bool hasSpans() const;
```

详见[第 8 节](#8-单元格合并span专题)。

### 7.11 行号列 / 复选框列 / 拖拽手柄列

| 函数 | 说明 |
|---|---|
| `void setRowNumberColumnVisible(bool visible)` / `bool isRowNumberColumnVisible() const` | 行号列开关；显示"原始行号 + 1" |
| `void setCheckboxColumnVisible(bool visible)` / `bool isCheckboxColumnVisible() const` | 复选框列开关；隐藏时清空全部勾选 |
| `void setRowDragEnabled(bool enabled)` / `bool isRowDragEnabled() const` | 行拖拽手柄列开关（行拖拽总开关） |
| `void setDragPreviewTextCallback(const std::function<QString(const AgGridModel *, const QVector<int> &)> &callback)` | 自定义拖动时浮动提示文字（默认显示行数） |

复选框列与拖拽手柄列同时开启时合并为一个固定列单元格，中间无分隔线。表头复选框支持全选 / 半选状态。

### 7.12 勾选（注意：按原始行）

| 函数 | 说明 |
|---|---|
| `bool isRowChecked(int row) const` | 该可见行是否勾选 |
| `void setRowChecked(int row, bool checked)` | 设置勾选 |
| `bool isRowCheckable(int row) const` | 该行是否允许勾选 |
| `void setRowCheckableCallback(const std::function<bool(const AgGridModel *, int)> &callback)` | 自定义行可选回调（返回 false 的行复选框置灰） |
| `void selectAllRows()` | 全选（表头复选框同效） |
| `void clearAllCheckedRows()` | 清空勾选 |
| `QSet<int> checkedRows() const` | 勾选行集合（**可见行号**） |
| `QSet<int> checkedOriginalRows() const` | 勾选行集合（**原始行号**，跨排序过滤稳定，推荐持久化时使用） |

信号：`rowCheckedChanged(int row, bool checked)`、`checkedStateChanged()`。

### 7.13 排序 / 过滤（转发模型）

| 函数 | 说明 |
|---|---|
| `void setSortingEnabled(bool enabled)` / `bool isSortingEnabled() const` | 表头点击排序总开关（默认关闭） |
| `int sortedColumn() const` / `Qt::SortOrder sortOrder() const` | 当前排序列与方向（-1 表示未排序） |
| `void setSortRules(const QList<AgSortRule> &rules)` | 多规则逻辑排序 |
| `QList<AgSortRule> sortRules() const` | 当前排序规则 |
| `void clearSort()` | 清除排序 |
| `void setFilterCallback(const std::function<bool(const QVariantMap &row)> &callback)` | 过滤回调（返回 true 保留） |
| `void clearFilter()` | 清除过滤 |
| `bool isFiltered() const` | 是否设置了过滤回调 |

信号：`sortChanged(int column, Qt::SortOrder order)`、`filterChanged()`。

### 7.14 编辑辅助回调

| 函数 | 说明 |
|---|---|
| `void setCellEditValidator(QSharedPointer<AgCellEditValidator> validator)` | 设置全局提交校验器（见第 13 节） |
| `void setCellEditableCallback(const std::function<bool(const AgGridModel *, int row, int column)> &callback)` | 动态可编辑回调：在列 `editable` 基础上按行/列最终裁决 |

### 7.15 交互状态查询（渲染器辅助）

| 函数 | 说明 |
|---|---|
| `int selectedRow() / selectedColumn()` | 当前选中格 |
| `bool hasSpans()` 等 | 见合并专题 |

---

## 8. 单元格合并（Span）专题

API 参考 `QTableView::setSpan` 设计。

### 8.1 接口

```cpp
void setSpan(int row, int column, int rowSpanCount, int columnSpanCount);
void clearSpan(int row, int column);      // 清除该单元格所在合并
void clearSpans();                        // 清除全部合并
int  rowSpan(int row, int column) const;  // 该单元格所在合并的行跨度（无合并为 1）
int  columnSpan(int row, int column) const;
bool hasSpans() const;
```

### 8.2 行为语义

- `(row, column)` 是合并区域**左上角锚点**；合并后锚点按整个合并区域绘制与居中，**被覆盖单元格不再显示其数据**（不绘制内容、不绘制内部边框、不参与行高与 AutoFit 测量）。
- `rowSpanCount` 或 `columnSpanCount` 传 **1x1 等价于清除该处合并**。
- 新的合并会**自动替换**与其重叠的旧合并。
- 跨度超过模型边界时**自动截断**。
- 合并区域**不会跨越列停靠区域边界**：从锚点向右延伸遇到不同停靠区域的可见列即截断（隐藏列跳过）。
- 合并按**当前可见行坐标**存储，与 `selectCell` 一致。

### 8.3 交互语义

| 操作 | 行为 |
|---|---|
| 点击 / 悬停 / 右键合并区域任意位置 | 命中并映射到锚点（`cellClicked`/`cellHovered` 等回调返回锚点坐标） |
| 选中合并区域任意位置 | 选中锚点；高亮背景与焦点边框覆盖整个合并区域 |
| 键盘导航（方向键） | 跳过被覆盖格，落在锚点上 |
| Tab 导航 / 编辑 | 始终编辑锚点数据 |
| 滚动定位（`scrollToCell` 等） | 按整个合并区域计算可视范围 |
| 编辑器几何 / 复制 | 按合并矩形处理 |

hover 高亮在合并单元格上是**原子化**的：进入/离开合并区域任意位置，整个合并区域与锚点行在同一帧内同步高亮/清除。

### 8.4 生命周期

| 事件 | 合并区域的行为 |
|---|---|
| `setSpan` 重叠替换 / 1x1 清除 | 立即生效 |
| 排序 / 过滤 / 行移动（`layoutChanged`） | **自动清空全部合并**（可见行坐标已失效） |
| 行数 / 列数减少（`rowsChanged` / `columnsChanged`） | 锚点越界的合并被剔除；存续合并按当前范围自动截断 |
| `setModel()` 换模型 | 清空全部合并 |
| 行拖拽重排过程中 | 保持不变（由拖拽逻辑接管，结束后合并停留在可见行坐标上） |

### 8.5 示例

```cpp
grid->setSpan(0, 0, 3, 1);   // 纵向：第 0~2 行的第 0 列合并
grid->setSpan(0, 2, 1, 2);   // 横向：第 0 行的第 2~3 列合并
grid->setSpan(5, 4, 2, 2);   // 2x2：第 5~6 行、第 4~5 列

grid->setSpan(0, 0, 1, 1);   // 清除 (0,0) 所在合并
qDebug() << grid->rowSpan(0, 0);   // 3（若 (0,0) 是上面 3 行合并的锚点）
```

---

## 9. 样式系统：AgGridStyle / AgDarkGridStyle

**头文件**：`AgGrid/include/AgGridStyle.h`、`AgDarkGridStyle.h`

### 9.1 概述

样式是"角色（StyleRole）→ 值（QVariant）"的查询表，支持三种粒度：

1. **全局样式**（`setGlobalStyle`）：所有未显式设置样式的控件自动使用，且全局样式变化时自动刷新已注册的控件
2. **控件独立样式**（`AgGridWidget::setStyle`）：控件接管所有权
3. **按行列上下文**：`value()` / `color()` / `integer()` 等都接受 `model, row, column` 参数，子类可按单元格位置返回不同值

### 9.2 主要成员

| 函数 | 说明 |
|---|---|
| `virtual QVariant value(StyleRole role, const AgGridModel *model = nullptr, int row = -1, int column = -1) const` | 核心查询入口；子类重写以自定义样式 |
| `template <typename T> T valueAs(StyleRole role, ...)` | 类型化读取 |
| `QColor color(StyleRole role, ...)` | 以 QColor 读取（无效值表示不绘制该元素） |
| `int integer(StyleRole role, ...)` | 以整数读取 |
| `QFont font(model, row, column)` | 单元格字体（`Font` 键） |
| `Qt::Alignment alignment(model, row, column)` | 默认文字对齐 |
| `SizeMode sizeMode() / void setSizeMode(SizeMode)` | 小 / 中 / 大尺寸模式（影响高度、字体、内边距等） |
| `virtual AgGridStyle *clone() const` | 深拷贝（`setSizeMode` 内部用于克隆） |
| `static AgGridStyle *globalStyle()` / `static void setGlobalStyle(AgGridStyle *)` | 全局样式读写（控件自动观察全局样式变化） |
| `static AgGridStyle *createLightStyle()` / `static AgGridStyle *createDarkStyle()` | 内置浅色 / 暗色主题工厂（返回新实例，所有权归调用者） |
| `virtual QVariant defaultValue(StyleRole role) const`（protected） | 各角色默认值，子类可部分覆盖 |

全部 StyleRole 含义见[第 16 节速查表](#16-样式键stylerole速查表)。

### 9.3 AgDarkGridStyle

`AgGridStyle` 的暗色主题实现（构造即配置暗色调色板），同样提供 `clone()`。等价的便捷写法是 `AgGridStyle::createDarkStyle()`。

### 9.4 自定义样式示例

```cpp
class MyStyle : public AgGridStyle {
public:
    QVariant value(StyleRole role, const AgGridModel *model,
                   int row, int column) const override {
        // 示例：错误值标红（列 1）
        if (role == CellForegroundColor && column == 1 && model
            && model->cellValue(row, 1).toInt() < 0)
            return QColor(Qt::red);
        return AgGridStyle::value(role, model, row, column);
    }
};
grid->setStyle(new MyStyle());
```

---

## 10. 单元格渲染器

**头文件**：`AgGrid/include/AgCellRenderer.h`、`AgDefaultCellRenderer.h`、`AgCheckBoxCellRenderer.h`

### 10.1 AgCellRendererContext

```cpp
struct AgCellRendererContext {
    bool hovered = false;                  // 是否悬停
    bool selected = false;                 // 是否为当前选中格
    Qt::Alignment alignment;               // 最终对齐（列对齐 + 样式覆盖后）
};
```

### 10.2 AgCellRenderer（抽象基类）

| 虚函数 | 说明 |
|---|---|
| `void paint(painter, rect, value, columnDef, row, column, model, style, textColor)` **纯虚** | 基础绘制入口 |
| `void paint(..., const AgCellRendererContext &context)` | 带 hover/selected 状态的绘制入口（控件调用此版本）；默认转调基础接口保持兼容 |
| `void cellMouseEnter(row, column, cellPos)` / `cellMouseLeave(row, column)` / `cellMouseMove(row, column, cellPos)` | 鼠标进入 / 离开 / 在格内移动；`cellPos` 以单元格左上角为原点。渲染器可在内部记录状态用于重绘（如按钮 hover 效果） |
| `bool wantsHoverCursor(row, column, cellPos)` | 返回 true 显示手型光标（如按钮热区） |
| `int preferredHeight(rect, value, columnDef, row, column, model, style)` | 返回该格期望高度参与行高计算；返回 -1 用默认行高（WordWrap 文本渲染器用此机制实现自动行高） |
| `QVariant copyContent(value, columnDef, row, column, model)` | Ctrl+C 复制内容；返回无效 QVariant 表示不提供复制 |

调用时机说明：hover 状态变化、鼠标进出都会触发控件重绘相应区域；渲染器内部状态（如按钮悬停）应在 `cellMouseEnter/Leave` 中更新。

### 10.3 内置实现

| 类 | 说明 |
|---|---|
| `AgDefaultCellRenderer` | 默认文本渲染器：按 `textWrapMode` 绘制文字，支持 `CellBackgroundColor` / `CellForegroundColor` 上下文样式与自动行高 |
| `AgCheckBoxCellRenderer` | 复选框渲染器：值按 bool 绘制复选框，整格可点击（配合 `setEditable(true)`），`wantsHoverCursor` 返回 true |

### 10.4 自定义示例（按钮列）

```cpp
class ButtonRenderer : public AgCellRenderer {
public:
    void paint(QPainter *p, const QRect &rect, const QVariant &value,
               const AgColumnDef &def, int row, int column,
               const AgGridModel *model, const AgGridStyle *style,
               const QColor &textColor, const AgCellRendererContext &ctx) const override {
        // 用 ctx.hovered 画按钮 + 用内部 QHash 记录热区供 wantsHoverCursor 使用
    }
    bool wantsHoverCursor(int row, int column, const QPoint &cellPos) const override {
        return hitButton(cellPos);   // 命中按钮热区时显示手型
    }
    QVariant copyContent(...) const override { return QVariant(); }  // 按钮列不复制文本
};

grid->registerCellRenderer("button", QSharedPointer<AgCellRenderer>(new ButtonRenderer()));
def.setCellRenderer("button");
```

---

## 11. 表头渲染器

**头文件**：`AgGrid/include/AgHeaderRenderer.h`

### 11.1 AgHeaderRendererContext

```cpp
struct AgHeaderRendererContext {
    bool hovered = false;              // 鼠标是否悬停在该节点上
    int column = -1;                   // 该节点首个叶子列索引
    int level = 0;                     // 1 起始的层级
    bool isGroup = false;              // 是否分组节点
    bool sorted = false;               // 是否为当前排序列
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    QPoint headerPos = QPoint(-1, -1); // 鼠标相对节点左上角坐标（无效为 -1,-1）
};
```

### 11.2 AgHeaderRenderer（抽象基类）

| 虚函数 | 说明 |
|---|---|
| `void paint(painter, rect, columnDef, column, level, model, style, textColor, context)` **纯虚** | 绘制表头节点 |
| `void headerMouseEnter / headerMouseLeave / headerMouseMove(column, level, rect, headerPos)` | 鼠标进出 / 移动；`rect` 为节点矩形（控件坐标） |
| `bool wantsHoverCursor(column, level, rect, headerPos)` | 手型光标 |
| `bool hitTest(column, level, rect, headerPos)` | 命中可点击区域（如按钮）；返回 true 时点击**不触发排序** |
| `void headerMouseClick(column, level, rect, headerPos)` | 可点击区域上按下并释放时回调 |

默认（未设置 `headerRenderer`）绘制标准表头：背景、文字、排序箭头。示例见 `Example/CustomAgGrid/HeaderButtonRenderer`。

---

## 12. 单元格编辑器

**头文件**：`AgGrid/include/AgCellEditor.h` 及三个内置实现

### 12.1 AgCellEditor（抽象基类）

| 虚函数 | 说明 |
|---|---|
| `QWidget *createEditor(QWidget *parent) const` **纯虚** | 创建编辑器控件（parent 为 AgGridWidget） |
| `void setEditorValue(QWidget *editor, const QVariant &value) const` **纯虚** | 模型值 → 编辑器 |
| `QVariant editorValue(QWidget *editor) const` **纯虚** | 编辑器 → 模型值 |
| `void setInitialTypedText(QWidget *editor, const QString &text) const` | 键盘直接输入触发编辑时注入初始字符（默认无操作） |
| `bool wantsReturnForNewLine() const` | 返回 true 表示 Return 键作为换行而非提交（多行编辑器用） |

编辑生命周期：双击 / Enter / F2 / 直接输入字符 → `createEditor` + `setEditorValue`（直接输入还会调 `setInitialTypedText`）→ 编辑中滚动自动跟随 → 提交时 `editorValue` 取值 → 经校验器裁决 → 写入模型并发出 `cellEditingFinished`、`cellValueChanged`。

### 12.2 内置实现

| 类 | 编辑控件 | 特性 |
|---|---|---|
| `AgDefaultCellEditor` | QLineEdit | 单行编辑；直接输入字符注入到开头 |
| `AgMultiLineCellEditor` | QPlainTextEdit | 多行编辑；`wantsReturnForNewLine()` 返回 true（Enter 换行） |
| `AgComboBoxCellEditor(const QStringList &items)` | QComboBox（可编辑） | 下拉选择；`setItems()` / `items()` 管理选项 |

### 12.3 注册与使用

```cpp
grid->registerCellEditor("combo",
    QSharedPointer<AgCellEditor>(new AgComboBoxCellEditor({"Alpha", "Beta"})));
def.setCellEditor("combo");    // 该列使用下拉编辑
def.setEditable(true);         // 必须可编辑
```

---

## 13. 编辑校验器

**头文件**：`AgGrid/include/AgCellEditValidator.h`

```cpp
enum class AgEditValidationResult {
    Accept,            // 接受新值，关闭编辑器并更新模型
    Reject,            // 拒绝新值，关闭编辑器但不更新模型
    RejectKeepEditing  // 拒绝新值，保持编辑器打开并提示错误
};

class AgCellEditValidator {
public:
    virtual ~AgCellEditValidator();
    virtual AgEditValidationResult validate(
        const AgGridModel *model, int row, int column,
        const QVariant &oldValue, const QVariant &newValue,
        QString &errorMessage) const = 0;
};
```

通过 `AgGridWidget::setCellEditValidator()` 设置，对全部编辑提交生效。`errorMessage` 用于提示（校验未通过时编辑器保持打开）。示例见 `Example/CustomAgGrid/DemoValidator`。

---

## 14. 多表格同步：AgGridSyncGroup

**头文件**：`AgGrid/include/AgGridSyncGroup.h`
**基类**：`QObject`

让多个表格的滚动偏移与列宽互相跟随（数据对比场景）。

```cpp
enum SyncFeature {
    NoSync = 0x0,
    SyncScroll        = 0x1,   // 垂直/水平滚动偏移
    SyncColumnWidths  = 0x2,   // 列宽（叶子列逐列拷贝）
    SyncAll           = SyncScroll | SyncColumnWidths
};
```

| 函数 | 说明 |
|---|---|
| `void addGrid(AgGridWidget *grid)` / `void removeGrid(AgGridWidget *grid)` / `void clear()` | 成员管理（内部用 QPointer，控件销毁自动失效） |
| `int gridCount() const` / `bool contains(const AgGridWidget *) const` | 成员查询 |
| `void setSyncEnabled(bool enabled)` / `bool isSyncEnabled() const` | 同步总开关 |
| `void setFeatures(SyncFeatures features)` / `SyncFeatures features() const` | 同步特性组合 |
| `void syncFrom(AgGridWidget *source)` | 一次性把 source 的滚动偏移与全部列宽推送给其他成员 |

用法：

```cpp
AgGridSyncGroup *sync = new AgGridSyncGroup(this);
sync->addGrid(gridA);
sync->addGrid(gridB);
sync->setFeatures(AgGridSyncGroup::SyncAll);
sync->syncFrom(gridA);      // 初始对齐
```

---

## 15. 信号速查表（AgGridWidget）

| 信号 | 说明 |
|---|---|
| `cellClicked(int row, int column)` | 单元格被单击（含特殊列编号；合并格返回锚点坐标） |
| `cellDoubleClicked(int row, int column)` | 单元格被双击 |
| `cellHovered(int row, int column)` | 悬停单元格变化 |
| `cellRightClicked(int row, int column, const QPoint &globalPos)` | 单元格右键（全局坐标可用于菜单） |
| `headerRightClicked(int column, int level, const QPoint &globalPos)` | 表头右键（level 为 1 起始层级） |
| `selectionChanged()` | 选中变化 |
| `cellEditingStarted(int row, int column)` | 编辑开始 |
| `cellEditingFinished(int row, int column)` | 编辑结束（提交或取消） |
| `cellValueChanged(int row, int column, const QVariant &value)` | 编辑提交且通过校验 |
| `cellCheckStateChanged(int row, int column, bool checked)` | 数据列内置复选框被切换 |
| `rowCheckedChanged(int row, bool checked)` | 复选框列某行勾选变化（可见行号） |
| `checkedStateChanged()` | 勾选集合变化（全选/清空/行拖拽影响等） |
| `sortChanged(int column, Qt::SortOrder order)` | 排序变化 |
| `filterChanged()` | 过滤状态变化 |
| `verticalScrollChanged(int value)` / `horizontalScrollChanged(int value)` | 滚动偏移变化（所有来源） |
| `columnWidthsChanged()` | 列宽变化（模型层转发） |
| `rowsDragged(const QVector<int> &originalRows, int targetOriginalIndex)` | 行拖拽完成（**原始行号**） |

---

## 16. 样式键（StyleRole）速查表

带 📍 的键支持按 `(model, row, column)` 上下文返回不同值。

### 尺寸

| 键 | 类型 | 说明 |
|---|---|---|
| `HeaderHeight` | int | 单级表头高度 |
| `RowHeight` | int | 默认行高 |
| `ScrollBarExtent` | int | 滚动条厚度 |
| `CornerRadius` | int | 表格面板圆角（0 为直角） |

### 表头

| 键 | 类型 | 说明 |
|---|---|---|
| `HeaderBackgroundColor` | QColor | 表头背景 |
| `HeaderForegroundColor` | QColor | 表头文字色 |
| `HeaderBorderColor` | QColor | 表头边框 / 区域分隔线颜色 |
| `HeaderFont` | QFont | 表头字体（列定义可覆盖） |
| `HeaderAlignment` | Qt::Alignment | 表头对齐（列定义可覆盖） |

### 单元格 / 网格线

| 键 | 类型 | 说明 |
|---|---|---|
| `CellBorderColor` 📍 | QColor | 单元格边框 |
| `GridLineColor` | QColor | 网格线 |
| `RowBackgroundColor` | QColor | 行背景 |
| `SelectedRowBackgroundColor` | QColor | 选中行背景 |
| `AlternateRowBackgroundColor` | QColor | 斑马纹背景（无效色则关闭） |
| `HoverRowBackgroundColor` | QColor | 悬停行背景（合并单元格为整体高亮） |
| `FocusedCellBorderColor` 📍 | QColor | 焦点格边框颜色（无效色不绘制） |
| `FocusedCellBorderWidth` 📍 | int | 焦点格边框宽度 |
| `CellHorizontalPadding` 📍 | int | 单元格水平内边距 |
| `CellVerticalPadding` 📍 | int | 单元格垂直内边距 |
| `CellBackgroundColor` 📍 | QColor | 单元格自定义背景（有效时由渲染器绘制并叠加 hover） |
| `CellForegroundColor` 📍 | QColor | 单元格自定义前景色 |
| `CellTextAlignment` 📍 | int (Qt::Alignment) | 单元格对齐覆盖（0 表示不覆盖） |
| `BorderColor` | QColor | 表格最外层边框 |

### 滚动条

| 键 | 类型 | 说明 |
|---|---|---|
| `ScrollBarSliderColor` | QColor | 滑块颜色 |
| `ScrollBarTrackBackgroundColor` | QColor | 滑轨背景 |

### 字体

| 键 | 类型 | 说明 |
|---|---|---|
| `Font` 📍 | QFont | 单元格字体 |
| `DefaultTextAlignment` 📍 | Qt::Alignment | 默认文字对齐 |

### 复选框

| 键 | 类型 | 说明 |
|---|---|---|
| `CheckboxSize` | int | 复选框边长 |
| `CheckboxCheckedColor` | QColor | 选中填充色 |
| `CheckboxUncheckedColor` | QColor | 未选中填充色 |
| `CheckboxBorderColor` | QColor | 边框色 |
| `CheckboxCheckmarkColor` | QColor | 对勾颜色 |

### 行拖拽

| 键 | 类型 | 说明 |
|---|---|---|
| `DragHandleIconColor` | QColor | 手柄圆点颜色 |
| `DragHandleHoveredIconColor` | QColor | 悬停时圆点颜色 |
| `DragHandleDotSize` | int | 圆点直径 |
| `DragHandleBackgroundColor` | QColor | 手柄格背景 |
| `DragHandleHoveredBackgroundColor` | QColor | 悬停时背景 |
| `DraggedRowOverlayColor` | QColor | 拖动中行上的半透明覆盖色 |

### 行号列

| 键 | 类型 | 说明 |
|---|---|---|
| `RowNumberFontBold` | bool | 行号加粗 |
| `RowNumberForegroundColor` | QColor | 行号文字颜色 |

---

## 17. 常见问题与注意事项

1. **合并单元格按可见行坐标存储**：排序 / 过滤 / 行移动会自动清空全部合并（坐标已失效）；行数列数减少时自动剔除越界合并。若需要"合并跟随数据行"，需在数据变化后由调用方重新 `setSpan`。
2. **勾选与行拖拽使用原始行号**：`checkedOriginalRows()`、`rowsDragged` 信号跨排序过滤稳定；界面展示相关用可见行号。
3. **AutoFit 只增不减**：自适应列宽按可视内容增量测量，避免滚动时宽度抖动；需要精确收缩时调用 `autoSizeColumn()`。
4. **过滤回调返回 true 表示保留**该行。
5. **复选框数据列**：`setCheckbox(true)` + `setEditable(true)` 才能点击切换；值按 bool 读写。
6. **停靠与合并**：合并区域不跨越停靠边界，跨界的 `setSpan` 会被自动截断到锚点所在区域。
7. **样式无效色（QColor 无效）表示不绘制该元素**：例如把 `AlternateRowBackgroundColor` 设为无效色可关闭斑马纹。
8. **DLL 更新**：Windows 下 exe 优先加载同目录的 `AgGrid.dll`，重编译库后确认 exe 旁的 DLL 已更新（Example.pro 已内置自动拷贝）。
9. **内存所有权**：模型由调用者管理（通常 `new AgGridModel(parent)`）；`setStyle()` 传入的样式由控件接管并销毁；注册的渲染器 / 编辑器通过 `QSharedPointer` 管理。
