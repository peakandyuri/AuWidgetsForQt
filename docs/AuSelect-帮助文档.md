# AuSelect 帮助文档

`AuSelect` 是替代 `QComboBox` 的下拉选择控件，支持单选 / 多选（chips）、分组、禁用项、
可编辑过滤、清除按钮、亮暗主题、三种尺寸预设与展开动画。依赖公共库 **AuCommon**（自绘
滚动条与投影弹层容器）。

## 1. 快速上手

```cpp
AuSelect *select = new AuSelect(this);
select->addItem(QStringLiteral("苹果"));
select->addItem(QStringLiteral("梨"), QVariant("pear"));   // 显示文本 + 用户数据
select->setCurrentIndex(0);

connect(select, &AuSelect::activated, [](int index) { /* 用户选择 */ });
```

与 QComboBox 相同风格的 API：`addItem / addItems / insertItem / removeItem / clear /
count / itemText / itemData / itemIcon / findText / findData / currentIndex /
currentText / currentData / setCurrentIndex / setCurrentText`。首个 `addItem` 会自动成为
当前项（与 QComboBox 一致）。

## 2. 分组与禁用项

```cpp
select->addGroup(QStringLiteral("水果"));   // 之后的选项归入该组
select->addItem(QStringLiteral("苹果"));
select->addItem(QStringLiteral("梨"));
select->addGroup(QStringLiteral("蔬菜"));
select->addItem(QStringLiteral("白菜"));
select->addItem(QStringLiteral("香菜（缺货）"));
select->setItemEnabled(select->count() - 1, false);   // 禁用项：灰显、不可选、无 hover
select->addGroup(QString());                           // 传空串结束分组，后续选项不缩进
```

组名行渲染为浅灰标题（明显区别于选项色），无 hover、无选中效果，其下选项整体缩进
`itemIndentWidth`（默认 16px）。

## 3. 可编辑与过滤

```cpp
select->setEditable(true);                          // 默认只读
select->setPlaceholderText(QStringLiteral("输入关键字过滤…"));
connect(select, &AuSelect::textEdited, ...);        // 实时输入回调
```

- 只读模式（默认）：点击弹出面板，编辑框显示当前选中项文本；
- 可编辑模式：输入实时过滤下拉项（不区分大小写的包含匹配），命中文字以
  `matchHighlightForegroundColor` 高亮；输入时若面板未打开会自动打开；
- 输入内容与任何选项都不匹配时，**失去焦点或面板隐藏后**自动还原为当前选中项文本
  （无选中则清空）；
- 过滤时组名行在其组内任一选项命中时保持显示。

## 4. 多选模式（chips）

```cpp
select->setMultiSelection(true);
select->setCheckedIndexes({1, 3});
connect(select, &AuSelect::checkedChanged, [](const QList<int> &indexes) { ... });
select->checkedIndexes();   // 按条目顺序返回勾选索引
select->checkedTexts();
select->checkedData();
```

- 点击选项切换勾选（再点取消），已勾选项右侧绘制对勾；
- 编辑区以 chip（标签块）呈现已选项，chip 右侧 × 可移除；chips 过多自动换行，
  控件高度随之增长；
- 面板展开时点击编辑区（文本框 / chip 主体 / 箭头）收起面板；chip 的 × 移除保持
  面板当前状态；点击面板与控件之外的区域、焦点移出控件、宿主窗口失去激活 / 被
  拖动或按 Esc 才收起；
- 多选模式的过滤输入始终可用（与 ant-design 一致）。

## 5. 清除按钮（清除选择项）

参考 `AuDatePicker` 的清除交互，**默认关闭，需手动开启**：

```cpp
select->setClearButtonEnabled(true);     // 开启清除按钮（默认关闭）
connect(select, &AuSelect::cleared, []() { /* 用户清空了选择项 */ });

select->hasSelection();                  // 是否已有选择项（单选已选中 / 多选存在勾选）
select->clearSelection();                // 程序化清除（与点击按钮同效；无选择时无动作）
```

- 开启后，**已有选择项时**鼠标悬停编辑框，右侧箭头图标切换为实心圆圈 ×（悬停图标本体
  时圆底加深），点击即清除选择，且不会弹出下拉面板；
- 单选：还原为未选中（`currentIndex(-1)`），编辑框清空显示占位文本；多选：清空全部
  勾选（chips 全部移除），等价于 `clearChecked()`；
- 清除后立即移开鼠标，箭头图标恢复；条目本身不受影响（不会清空下拉选项，区别于
  `clear()`）；
- 无选择项时不显示清除图标；`setEnabled(false)` 时按钮同样不可用；
- 配色由样式 `clearIconCircleColor`（圆底）/ `clearIconCircleHoverColor`（悬停圆底）/
  `clearIconCrossColor`（×）控制，亮暗主题各有预设。

## 6. 样式系统（AuSelectStyle）

三种用法：

```cpp
// 1) 全局样式（所有未单独 setStyle 的 AuSelect 自动跟随并实时刷新）
AuSelectStyle::setGlobalStyle(AuSelectStyle::createDarkStyle());

// 2) 控件独立样式（所有权转移给控件）
select->setStyle(AuSelectStyle::createLightStyle());
select->style()->setEditorBorderRadius(2);   // 再逐项微调

 // 3) 尺寸档位：Small / Medium / Large（重置编辑框高度与字体、选项高度、间距、圆角等尺寸类字段）
 select->setSizeMode(AuSelectStyle::SizeMode::Large);
 ```

尺寸档位预设（编辑框高度 / 字体 / 下拉项高度，间距统一 2px，下拉项比编辑框略小一号）：
小 24px / 12px / 22px，中 30px / 14px / 26px，大 36px / 16px / 32px。

`setSizeMode()` 首次调用时从全局样式**派生**出控件私有样式：之后全局主题切换时自动跟随
（配色继承新全局样式、尺寸档位保持不变），但对 `style()` 的手动微调会在主题切换时被覆盖；
`setStyle()` 显式设置的样式则完全接管、不再跟随全局。

**自定义字体**：控件级 `setFont()` 的字体优先于样式字体（编辑框、chips、下拉项同步生效，
用于接入应用字体规范，如微软雅黑）；未显式设置时使用样式 `font`。控件高度由尺寸档位的
sizeHint 严格决定（垂直 Fixed），不会被布局拉伸。

明亮主题预设：hover 背景 `#F5F5F5`（灰）、选中背景 `#E6F4FF`（淡蓝）、悬停已选中项
`#BAE0FF`（天蓝）、对勾 / 选中文字 `#1677FF`。暗色主题对应一套协调的深色取值。

主要字段（全部有对应 getter / setter）：

| 分组 | 字段 |
| --- | --- |
| 字体 | `font`（编辑框 / chip / 下拉项共用，随尺寸档位重置） |
| 编辑框 | `editorHeight`、`editorBorderRadius`（圆角可设）、`editorBorderWidth`、`editorBorderColor / Hover / Active / Focus`、`editorFocusBackgroundColor`（键盘焦点内部淡色底，无效色=不绘制）、`editorBackgroundColor`、`editorForegroundColor`、`editorPlaceholderColor`、`editorDisabled*`、`editorHorizontalPadding` |
| chip | `chipBackgroundColor / Hover`、`chipForegroundColor`、`chipBorderColor`（无效色=无边框）、`chipRemoveIconColor / Hover`、`chipHeight`、`chipSpacing` |
| 面板 | `panelBackgroundColor`、`panelBorderColor`、`panelBorderRadius`、`panelShadowColor`、`panelShadowRadius`、`panelMaxHeight` |
| 下拉项 | `itemHeight`、`itemSpacing`（间距）、`itemHorizontalPadding`、`itemIndentWidth`、`itemBorderRadius`、`itemForegroundColor`、`itemDisabledForegroundColor`、`itemHoverBackgroundColor`、`itemSelectedBackgroundColor / ForegroundColor`、`itemHoverSelectedBackgroundColor`、`itemCheckmarkColor`、`matchHighlightForegroundColor` |
| 组名 | `groupHeaderHeight`、`groupHeaderForegroundColor` |
| 箭头 | `arrowColor`、`arrowSlotWidth` |
| 清除图标 | `clearIconCircleColor`（实心圆底）、`clearIconCircleHoverColor`（悬停圆底）、`clearIconCrossColor`（×） |

## 7. 下拉面板行为

- **定位**：默认显示在编辑框下方（内容区顶边紧随编辑框 4px），下方空间不足时自动
  翻转到上方；整体收敛在所在屏幕的可用区内；
- **水平对齐**：内容区默认与编辑框左对齐；若面板更宽且右侧越界，自动切换为右对齐；
- **宽度模式**：
  ```cpp
  select->setPopupWidthMode(AuSelect::PopupMatchEditor);  // 与编辑框同宽（默认）
  select->setPopupWidthMode(AuSelect::PopupFixed);
  select->setFixedPopupWidth(320);                        // 固定宽度
  select->setPopupWidthMode(AuSelect::PopupFitContent);   // 按内容自适应
  ```
  三种模式都不小于编辑框宽度、不超过屏幕宽度；
- **高度**：内容总高与 `panelMaxHeight`（默认 280px，可用 `setMaxPopupHeight` 覆盖）
  取小，再收敛到所在一侧的可用空间；选项较多时使用 AuCommon 的自绘滚动条；
- **动画**：展开 / 收起各 100ms（OutCubic），下方弹出自上而下展开、上方弹出自下而上
  展开，内容随面板高度增加逐渐显露；
- **投影**：面板四周绘制 9-slice 预模糊投影（AuCommon::AuPopupPanel），颜色 / 强度可调，
  Windows 与 Linux 表现一致。

## 8. 事件回调

| 信号 | 时机 |
| --- | --- |
| `popupAboutToShow()` | 展开动画开始前 |
| `popupShown()` | 展开动画结束后 |
| `popupAboutToHide()` | 收起动画开始前 |
| `popupHidden()` | 面板隐藏后（此时不匹配的输入已被还原） |
| `activated(int)` | 用户激活选项（单选点选 / 回车；多选勾选、取消、chip 移除） |
| `currentIndexChanged(int)` / `currentTextChanged(QString)` | 当前项变化 |
| `checkedChanged(QList<int>)` | 多选勾选集合变化 |
| `cleared()` | 用户通过清除按钮 / `clearSelection()` 清空选择项 |
| `textEdited(QString)` | 可编辑输入（用户输入触发） |

展开时自动定位：单选滚动到当前选中项，多选滚动到第一个选中项。

## 9. 自定义渲染与自定义箭头

### 自定义委托（绘制图标、角标等）

继承 `AuSelectItemDelegate`，可通过 `AuSelectItemRole` 读取行类型、可用性、勾选态等，
或直接调用基类 `paint()` 画底后补充绘制；条目自带图标时内置委托即可渲染
（`addItem(text, data, icon)`）。

```cpp
class ColorDelegate : public AuSelectItemDelegate {
public:
    using AuSelectItemDelegate::AuSelectItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &opt,
               const QModelIndex &idx) const override {
        AuSelectItemDelegate::paint(p, opt, idx);   // 先画内置样式
        // 再补充自定义元素...
    }
};
select->setItemDelegate(new ColorDelegate(select->style(), select));  // 所有权转移给控件
```

### 替换箭头

```cpp
QLabel *arrow = new QLabel(QStringLiteral("▼"));
arrow->setFixedWidth(20);
select->setArrowWidget(arrow);      // 所有权转移给控件；传 nullptr 恢复内置箭头
```

内置箭头是一个随面板展开旋转 180° 的 chevron；替换后的箭头部件由用户自绘，可结合
第 8 节的面板回调实现自己的动效。开启清除按钮后，悬停已选项时右侧（含自定义箭头部件）
会被清除图标临时替换，点击清除后恢复。

## 10. 键盘交互

| 按键 | 行为 |
| --- | --- |
| `Down` / 点击 | 面板未展开时展开 |
| `Up` / `Down` | 高亮上 / 下一个可选项（自动跳过组名行与禁用项） |
| `PageUp` / `PageDown` / `Home` / `End` | 翻页 / 首尾 |
| `Enter` | 面板展开时激活高亮项，未展开时打开面板 |
| `Esc` | 关闭面板（并还原不匹配的输入） |
| `Tab` | 焦点移到表单下一个控件（面板展开时先收起） |

**焦点效果**：控件获得键盘焦点（Tab 切换或点击）时显示焦点边框 + 内部淡色底
（`editorFocusBorderColor` / `editorFocusBackgroundColor`），Tab 在表单控件间切换时
焦点位置一目了然。

## 11. AuCommon 公共库

AuSelect 与 AuGrid 共用的基础部件库（后续公共类统一放这里）：

- **`AuCommon::AuScrollBar`**：自绘滚动条（原 AuGridScrollBar，已抽取到公共库）。
  极简细滑块常驻、悬停加宽淡入滑轨；支持滑块 / 滑轨 / 分隔线颜色与轨道边距自定义；
  点击轨道翻页（按住连续翻页）、拖拽、滚轮与 Shift+滚轮按页滚动。AuGrid 已改为
  引用该类，行为不变。
- **`AuCommon::AuPopupPanel`**：带投影的通用弹层容器。弹出 / 收起动画、屏幕内定位
  （下->上翻转、左右对齐切换）、不抢焦点（Windows `WM_MOUSEACTIVATE` 返回
  `MA_NOACTIVATE`，X11 `WA_X11DoNotAcceptFocus`）、失焦自动收起。日期选择等弹层
  组件可直接复用。

### 构建集成（qmake）

```pro
INCLUDEPATH += $$PWD/../AuCommon/include $$PWD/../AuSelect/include
DEPENDPATH   += $$PWD/../AuCommon $$PWD/../AuSelect
win32: LIBS += -L$$PWD/../AuCommon/bin/ -lAuCommon -L$$PWD/../AuSelect/bin/ -lAuSelect
```

顶层 `AuWidgetsForQt.pro` 的 SUBDIRS 顺序：AuCommon -> AuGrid -> AuSelect ->
AuDatePicker -> Example（后四者 depend 于 AuCommon）。
