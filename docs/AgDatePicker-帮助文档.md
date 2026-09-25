# AgDatePicker 帮助文档

`AgDatePicker` 是替代 `QCalendarWidget` / `QDateEdit` 弹出形态的日期选择控件，支持
年 / 月 / 日三种选择粒度与**日期范围选择（双日历）**、**键盘操作（方向键移动焦点 +
回车选中）**、可选范围、悬停清除、年份 / 月份九宫格选择页（整屏滚动动画）、
footer 快捷项与自定义、亮暗主题、三种尺寸预设。默认字体**微软雅黑**（Ag 控件统一
约定，可通过样式 `setFont()` 覆盖）。依赖公共库 **AgCommon**（投影弹层容器
`AgPopupPanel`）。

## 1. 快速上手

```cpp
AgDatePicker *picker = new AgDatePicker(this);
picker->setDate(QDate::currentDate());          // 默认格式 yyyy-MM-dd 显示
connect(picker, &AgDatePicker::dateChanged, [](const QDate &date) { /* 值变化 */ });
connect(picker, &AgDatePicker::activated, [](const QDate &date) { /* 用户选择 */ });
```

- 编辑框只读，点击（编辑框或右侧日历图标）弹出面板；面板优先出现在编辑框下方，
  空间不足自动翻转到上方，展开 / 收起带 100ms 动画；
- 点击面板外、窗口失活、焦点移出或按 **Esc** 自动收起（年 / 月选择页中按 Esc 先
  返回上一级视图，见下文）；
- 无值时面板定位到当前月份；已选值时定位到所选日期所在月并高亮选中；
- `date()` 返回当前日期，无效 `QDate` 表示未选择；`hasSelection()` 便捷判断。

**宽度模式**：默认宽度按显示格式的最长文本自适应（`WidthFitContent`，固定尺寸策略，
不随父布局拉伸）；需要跟随布局水平拉伸时切换 `WidthStretch`（最小宽度仍为格式自适应
宽度，文本不截断）。两种模式下都可用 `setFixedWidth()` / `setMinimumWidth()` 手动调整：

```cpp
picker->setEditorWidthMode(AgDatePicker::WidthStretch);  // 跟随布局水平拉伸
picker->setFixedWidth(260);        // 固定宽度（两种模式下均可手动固定）
picker->setMinimumWidth(200);      // 最小宽度
picker->setEditorHeight(42);       // 高度（或用 setSizeMode 切换档位）
```

弹层面板宽度始终固定（按内容计算，中档约 230px），**不随编辑框宽度拉伸或收缩**。

## 2. 显示格式

```cpp
picker->setDisplayFormat(QStringLiteral("yyyy/MM/dd"));
picker->setDisplayFormat(QStringLiteral("yyyy年MM月dd日"));
```

- 默认格式随选择粒度切换：选日 `yyyy-MM-dd`、选月 `yyyy-MM`、选年 `yyyy`；
- 一旦调用过 `setDisplayFormat()` 即视为自定义格式，此后切换粒度不再重置；
- 格式语法同 `QDate::toString()`（Qt 的日期模板：`yyyy / MM / dd / d / M` 等）。

## 3. 选择粒度（选年 / 选月 / 选日 / 日期范围）

```cpp
picker->setSelectionMode(AgDatePicker::SelectMonth);       // 仅选到月（yyyy-MM）
picker->setSelectionMode(AgDatePicker::SelectYear);        // 仅选到年（yyyy）
picker->setSelectionMode(AgDatePicker::SelectDay);         // 选到日（默认）
picker->setSelectionMode(AgDatePicker::SelectDateRange);   // 日期范围（双日历，见 3.1）
```

- **SelectDay**：弹出日历页。点击 head 中间的 "xxxx年" 进入年份选择页、点击 "xx月"
  进入月份选择页，逐级选回日历；
- **SelectMonth**：直接弹出月份选择页，点击月份即选定；
- **SelectYear**：直接弹出年份选择页，点击年份即选定。

### 3.1 日期范围选择（SelectDateRange，双日历）

```cpp
picker->setSelectionMode(AgDatePicker::SelectDateRange);
picker->setSelectedRange(QDate(2026, 9, 1), QDate(2026, 9, 30));  // 程序设置（自动排序）
QDate start = picker->startDate();
QDate end   = picker->endDate();
connect(picker, &AgDatePicker::dateRangeChanged, [](const QDate &s, const QDate &e) {});
connect(picker, &AgDatePicker::rangeActivated, [](const QDate &s, const QDate &e) {});
```

**面板结构**：左右两个日历并排（面板加宽），右侧比左侧晚一个月；任一侧翻月 /
滚轮切换时另一侧自动联动，始终保持一个月差（左侧只显示 `<<` `<` 箭头，右侧只显示
`>` `>>` 箭头，内侧翻月由联动完成）。

**选择流程**（两次点击 / 两次回车）：

1. 面板展开时编辑框显示 `开始日期 ~ 结束日期` 占位（默认格式 yyyy-MM-dd），**开始
   日期下方出现一条主色调下划线**，提示当前选择的是开始日期；
2. 第一次点击 / 回车选中开始日期：面板保持展开，编辑框显示 `2026-09-15 ~ `，下划线
   **带动画滑到结束日期下方**；此时候选终点为任意可选日期——**起点之前或之后的日期
   均可选择**，鼠标悬停或方向键移动焦点时，起点与该日期之间实时显示**淡蓝色范围带**
   预览；
3. 第二次点击 / 回车确定结束日期：若选了开始日期**之前**的日期，自动交换（第一次
   选择转为结束日期）；提交范围、发出 `dateRangeChanged` / `rangeActivated`，面板
   收起；重新展开时定位到开始日期所在月，已提交范围以端点 + 范围带形式展示，可重新
   选择；
4. 选点过程中按 **Esc** 或点击面板外收起：撤销本次未完成的选点，恢复已提交范围。

**取值 API**：`startDate() / endDate()`；`setSelectedRange(start, end)`（自动按先后
排序，开始无效则整体清空）、`setStartDate() / setEndDate()`（仅改一侧，结束早于开始
时清空结束）；`setDate()` 在范围模式下等价于 `setStartDate()`；`date()` 返回开始日期，
`clear()` 清空两侧。范围限制（`setMinimumDate / setMaximumDate / setDateRange`）对
两个端点同样生效。

**占位文本**：编辑框整体为空时灰显 "开始日期 ~ 结束日期"；起始已选而结束为空时在
分隔符后灰显结束占位，可自定义：

```cpp
picker->setRangePlaceholderText(QStringLiteral("从"), QStringLiteral("至"));
picker->setRangePlaceholderText(QString(), QString());   // 恢复默认
```

`AgDateRangePanel`（双日历容器）与 `AgDatePickerPanel` 亦可独立内嵌使用。

## 4. 可选范围

```cpp
picker->setMinimumDate(QDate(2026, 1, 1));   // 只能选择此日期（含）之后
picker->setMaximumDate(QDate(2026, 12, 31)); // 只能选择此日期（含）之前
picker->setDateRange(QDate(2026, 1, 1), QDate(2026, 12, 31));  // 同时设置
picker->setMinimumDate(QDate());             // 传无效 QDate 取消下限
```

- 范围外日期灰显（更浅一层）、**无悬停效果、不可点击**；
- 前后月补齐日期同样受范围约束；
- 月份 / 年份粒度按"与范围有交集"判定可选：如范围从 2026-03-15 起，则 2026 年 3 月
  在选月粒度下可选（月内存在合法日期）；
- 年份选择页的滚动范围自动收敛到可选年份（未设范围时为当前年 ±100）；
- footer 内置 "今天 / 当月 / 今年" 快捷项目标不在范围内时同样禁用；
- `isDateSelectable(date)` 可查询某日期是否可选。

## 5. 日历面板结构

```
+--------------------------------------------------+
|  <<   <       2026年 9月          >    >>         |   head
+--------------------------------------------------+
|   一   二   三   四   五   六   日                |   星期行（周一起）
|   31    1    2    3    4    5    6                |
|   ...            （6 周 x 7 列）                  |   body
+--------------------------------------------------+
|                      今天                         |   footer
+--------------------------------------------------+
```

- **head**：左 `<<`（上一年）`<`（上一月），右 `>`（下一月）`>>`（下一年）；中间
  "xxxx年" 与 "xx月" 分段可点击（类似超链接，悬停变主题色），分别进入年份 / 月份页；
- **星期行**：周一 ~ 周日；
- **日期格**：当月日期正常色，前后月补齐日期灰显但仍可点击选择，"今天" 以主题色
  文字强调，选中日期蓝底白字圆角块；
- 鼠标滚轮在日历页切换上 / 下月。

### 5.1 年份选择页（点击 "xxxx年" 进入）

- 4 列 x 3 行一页显示 12 个年份（Win10 风格九宫格）；
- head 左侧显示当前页年份区间（如 `2020 ~ 2031`），右侧上下箭头每次滚动一屏；
- 鼠标滚轮同样整屏滚动，带 150ms 平滑滚动动画，**不显示滚动条**；
- 滚动范围收敛到可选年份；范围外年份灰显不可点；
- 点击年份：选日粒度**直接回到对应年月的日历页**（月份保持不变，无需再选月）；
  选月粒度进入该年的月份页；选年粒度即选定该年；
- 按 **Esc** 返回上一级视图（选日的日历页 / 选月的月份页）。

### 5.2 月份选择页（点击 "xx月" 进入）

- 4 列 x 3 行一次显示 12 个月；
- head 左侧年份文本可点击（进入年份页），右侧上下箭头 / 鼠标滚轮切换年份
  （格子与数据不变，切换即时无动画）；
- 选日粒度下点击月份回到该年月的日历页；选月粒度下点击即选定；
- 按 **Esc** 返回日历页（选日粒度）或收起面板（选月粒度的初始页）。

## 6. 清除（悬停 × 图标）

```cpp
picker->setClearButtonEnabled(true);    // 默认开启
```

已选择日期时，鼠标悬停编辑框，右侧日历图标切换为**实心圆圈 ×**（悬停加深），点击即
清除并发出 `cleared()` 与 `dateChanged(QDate())`；清除不会弹出面板。程序清除用
`clear()` 或 `setDate(QDate())`。

## 7. footer

### 7.1 内置快捷项与隐藏

默认居中显示无下划线超链接样式的快捷项：选日粒度为 "今天"、选月为 "当月"、选年为
"今年"，点击选择当前日期（范围外自动禁用）。

```cpp
picker->setFooterVisible(false);   // 整体隐藏 footer
```

### 7.2 左 / 中 / 右快捷链接

```cpp
picker->setFooterLink(AgDatePicker::FooterLeft, QStringLiteral("昨天"), [picker]() {
    picker->setDate(QDate::currentDate().addDays(-1));   // 回调内自行决定行为
});
picker->setFooterLink(AgDatePicker::FooterRight, QStringLiteral("一周后"), ...);
picker->resetFooterLink(AgDatePicker::FooterCenter);      // 恢复内置"今天"
```

三个槽位（`FooterLeft / FooterCenter / FooterRight`）可分别配置文本与点击回调，
覆盖该槽位的内置项；回调内可继续操作控件（如 setDate）。

**footer 回调可控制面板收起**：回调里调用 `hidePopup()` 即可（例如自定义"完成"
按钮选中值后关闭面板）：

```cpp
picker->setFooterLink(AgDatePicker::FooterRight, QStringLiteral("完成"), [picker]() {
    picker->hidePopup();   // 点击 footer 链接后收起面板
});
```

**范围模式（双日历面板）的 footer**：同样支持 `setFooterLink` 三槽位自定义链接
（无内置快捷项；未配置任何链接时不显示 footer 条，配置后自动显示）。`setFooterWidget`
整体部件替换仅对单日历面板生效。

### 7.3 完全自定义 footer

```cpp
QWidget *myFooter = new QWidget();
// ... 自由布局（按钮、复选框等）
picker->setFooterWidget(myFooter);   // 所有权转移给控件，覆盖整个 footer 区
picker->setFooterWidget(nullptr);    // 恢复内置 footer
```

设置自定义部件后内置链接不再绘制（顶部分隔线保留）；`setFooterVisible(false)` 对
自定义部件同样生效（整体隐藏）。

## 8. 样式系统（AgDatePickerStyle）

三种用法与 AgSelectStyle 一致：

```cpp
// 1) 全局样式（所有未单独 setStyle 的 AgDatePicker 自动跟随并实时刷新）
AgDatePickerStyle::setGlobalStyle(AgDatePickerStyle::createDarkStyle());

// 2) 控件独立样式（所有权转移给控件）
picker->setStyle(AgDatePickerStyle::createLightStyle());
picker->style()->setEditorBorderRadius(2);   // 再逐项微调

// 3) 尺寸档位：Small / Medium / Large
picker->setSizeMode(AgDatePickerStyle::SizeMode::Large);
```

尺寸档位预设（编辑框高度 / 字体 / 日期格宽 x 高 / 图标槽）：
小 24px / 12px / 24x22 / 20px，中 30px / 14px / 30x28 / 24px，大 36px / 16px / 36x32 / 28px。
面板宽度 = 7 列日期格 + 间距 + 左右内边距（中档约 230px）；日期范围双日历面板宽度
约为一倍半（两个日历 + 间距）。档位切换会同时重置字体为**微软雅黑**对应字号
（Ag 控件统一默认字体族），可再用 `style()->setFont()` 覆盖。

**高度自定义**：除档位预设外可逐控件调整编辑框高度（从全局样式派生私有样式，
之后切换全局主题时配色自动跟随、高度保持）：

```cpp
picker->setEditorHeight(42);   // 覆盖档位预设高度
```

`setSizeMode()` / `setEditorHeight()` 首次调用时从全局样式**派生**控件私有样式；`setStyle()`
显式设置的样式完全接管、不再跟随全局。控件级 `setFont()` 优先于样式字体，同步作用于
编辑框与面板。

样式字段覆盖：编辑框（背景 / 文字 / 占位 / 边框常态悬停激活**焦点**禁用 /
`editorFocusBackgroundColor` 焦点内部淡色底 / 圆角 / 内边距）、
日历图标与清除图标（圆底 / 悬停 / × 颜色）、面板（背景 / 边框 / 圆角 / 投影 / 内边距）、
head（高度 / 箭头按钮尺寸与颜色 / 悬停背景 / 标题与悬停色 / 区间文本色）、星期行、
格子（尺寸 / 间距 / 圆角 / 当月 / 邻月灰 / 禁用 / 悬停背景 / 选中底色与文字色 / 今天色 /
**范围带底色 `cellInRangeBackgroundColor`**）、footer（高度 / 顶部分隔线 / 链接三态色）。
颜色字段为无效 `QColor` 时不绘制对应元素。

## 9. 信号

| 信号 | 触发时机 |
| --- | --- |
| `dateChanged(const QDate &)` | 值变化（程序设置或用户选择；无效日期 = 清空；范围模式为开始日期） |
| `dateRangeChanged(const QDate &, const QDate &)` | 范围模式：范围值变化（开始 / 结束） |
| `activated(const QDate &)` | 用户在面板中选择（含 footer 快捷项；范围模式为开始日期） |
| `rangeActivated(const QDate &, const QDate &)` | 范围模式：用户确定范围（第二次点击 / 回车） |
| `cleared()` | 用户点击清除图标或调用 `clear()` |
| `popupAboutToShow() / popupShown()` | 面板弹出动画前 / 后 |
| `popupAboutToHide() / popupHidden()` | 面板收起动画前 / 后 |

## 10. 键盘与禁用

- **Tab 切换**：控件内部的行编辑框参与表单 Tab 焦点链；获得键盘焦点时编辑框显示
  焦点边框 + 内部淡色底（`editorFocusBorderColor` / `editorFocusBackgroundColor`），
  焦点位置一目了然；面板展开时按 Tab 收起面板并移到下一个控件；
- 面板关闭时：`Enter / Return / Space / Down` 弹出面板；
- 面板打开时（焦点日期以主题色圆角圈高亮，随按键移动）：
  - `← / →` 焦点日期 -1 / +1 天，`↑ / ↓` -7 / +7 天（跨月自动翻月；落在当前页
    可见的前后月补齐日上不翻页，移出 42 格后翻页）；
  - `PageUp / PageDown` 焦点日期 ±1 个月（日历页）或整屏翻页（年份页）；
  - `Enter / Return` 选中焦点日期（等价点击）；日期范围模式下第一次回车选开始
    日期、第二次回车确定结束日期（焦点日期即预览终点）；
  - 月份 / 年份九宫格页同样支持方向键（左右 ±1 格、上下 ∓4 格）与回车激活；
  - 焦点移动自动钳制到可选范围边界；
- `Esc` 在年 / 月选择页返回上一级视图（年 / 月页 → 日历页，或月粒度的年页 →
  月页），已在初始页时收起面板；范围模式选点过程中 Esc 撤销本次选点并收起；
- `setEnabled(false)`：整体灰显、禁止弹出，已展开的面板自动收起。

## 11. 工程接入

```pro
INCLUDEPATH += $$PWD/../AgDatePicker/include
win32: LIBS += -L$$PWD/../AgDatePicker/bin/ -lAgDatePicker
else:unix: LIBS += -L$$PWD/../AgDatePicker/bin/ -lAgDatePicker
```

Qt 5.12+（Widgets），无其它外部依赖。完整示例见 `Example/demos/AgDatePickerDemoWindow.cpp`
（格式 / 范围 / 粒度 / footer / 尺寸 / 亮暗主题一页演示）。
