# AuWindowKit 帮助文档

AuWindowKit 是一套无边框窗体库，以**系统 API 为主、Qt 渲染为辅**，提供统一风格的窗体容器（`AuWindow`）、自绘标题栏（`AuTitleBar`）与样式系统（`AuWindowStyle`），用于替代 `QMainWindow` / `QDialog`。

## 模块组成

```
AuWindowKit/
├── AuWindowKit.pro           # 模块工程文件（输出 bin/libAuWindowKit）
├── include/
│   ├── AuWindowKit_global.h  # 导出宏
│   ├── AuWindow.h            # 窗体容器
│   ├── AuTitleBar.h          # 自绘标题栏
│   └── AuWindowStyle.h       # 样式（亮 / 暗主题工厂 + 全局样式）
├── widget/
│   ├── AuWindow.cpp
│   └── AuTitleBar.cpp
└── style/
    └── AuWindowStyle.cpp
```

## 平台实现策略

| 平台 | 边框 / 阴影 | 拖动与缩放 | 边缘分屏 | 圆角 |
| --- | --- | --- | --- | --- |
| **Windows** | 保留 `WS_THICKFRAME` 等原生样式，经 `WM_NCCALCSIZE` 去掉可见边框；阴影由 DWM 提供 | 系统非客户区（`WM_NCHITTEST`） | 原生 Aero Snap（拖到屏幕上/左/右边缘） | Win11 走 DWM 圆角偏好，Win10 用窗口区域 |
| **Linux/X11** | Qt 无边框 + 透明背景（ARGB）。KWin/UKUI 系 WM 上通过 `_KDE_NET_WM_SHADOW` 协议由混成器绘制**与原生窗口相同的主题阴影**（窗口即纯内容，无边距）；其他 WM 回退为自绘阴影 | 拖动 / 缩放经 `_NET_WM_MOVERESIZE` 交给 WM：**拖顶最大化预览、边缘半屏、snap-assist、拖出屏幕全部为系统原生行为**；WM 忽略时自动回退手动拖动（左右可出屏） | KWin 系原生边缘分屏（含贴边后 snap-assist） | 混成下靠 alpha 通道 + 抗锯齿平滑呈现；无混成时区域裁剪 |
| **其它平台** | 自绘阴影 | 手动拖动 / 缩放（钳制最小宽高） | 无 | 区域裁剪 |

最小宽高的约束方式：Windows 经 `WM_GETMINMAXINFO` 对原生边框缩放生效；其它平台由手动缩放路径与布局 `minimumSize` 保证。

## 快速开始

### 1. 作为普通窗体使用（QMainWindow 风格）

```cpp
#include "AuWindow.h"

AuWindow *win = new AuWindow;
win->setWindowTitle(QStringLiteral("主窗口"));
win->resize(960, 640);

QWidget *content = new QWidget;
QVBoxLayout *lay = new QVBoxLayout(content);
lay->addWidget(new QLabel("内容"));
win->setCentralWidget(content);

win->show();
```

`setWindowTitle()` / `setWindowIcon()` 会自动同步到自绘标题栏，无需额外设置。

### 2. 继承定制（推荐的工程做法）

业务窗体基类继承 `AuWindow` 统一外观，业务窗口再继承业务基类：

```cpp
// MyWin.h —— 业务窗体基类
class MyWin : public AuWindow {
    Q_OBJECT
public:
    explicit MyWin(QWidget *parent = nullptr) : AuWindow(parent) {
        setStyle(AuWindowStyle::createDarkStyle());  // 统一暗黑主题
        setWindowRadius(8);                          // 统一圆角
        setButtonVisible(AuTitleBar::ButtonMaximize, false);
    }
};

// 业务窗口：直接用 Qt Designer 设计
class SettingsPage : public MyWin {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget *parent = nullptr) : MyWin(parent) {
        ui->setupUi(this);   // 根布局自动转移到标题栏下方内容区
    }
private:
    Ui::SettingsPage *ui = new Ui::SettingsPage;
};
```

**布局自动转移的原理**：`ui->setupUi(this)` 会把根布局安装在窗体（`this`）上，
`AuWindow` 监听 `childEvent`，把安装到窗体上的布局连同受管控件整体转移到
标题栏下方的内容区（clientWidget），因此继承链上任意层级 `setupUi(this)`
都能自动获得标题栏。`setCentralWidget()` 与 `setupUi(this)` 二选一即可，
若两者都使用，后安装的布局会取代先前的中央控件。

### 3. Dialog 方式展示（模态）

```cpp
DialogWindow dialog(AuWindow::Modality::WindowModal, parentWindow);
const int code = dialog.exec();          // 模态阻塞，Esc 默认 reject
if (code == AuWindow::Accepted) { ... }
```

- `setDialogMode(true)`：挂 `Qt::Dialog` 标志，显示时自动居中于父窗口；
- `setModality(Modality::WindowModal / ApplicationModal)`：模态范围；
- `exec()` / `accept()` / `reject()` / `done()` / `result()` 与 `QDialog` 语义一致；
- Windows 上模态期间点击父窗口的系统提示音、父窗口标题栏闪烁等效果
  由操作系统基于窗口所有权关系保证，与原生对话框一致。

### 4. 标题栏自定义组件

```cpp
// 靠左（标题后）：纯展示，不抢占鼠标（该区域仍可拖动窗体）
QLabel *version = new QLabel("v1.0.0");
win->addTitleBarWidget(version, AuTitleBar::SlotAfterTitle, false);

// 居中（标题与按钮之间的空白区中间）
QLabel *clock = new QLabel(QTime::currentTime().toString("HH:mm:ss"));
win->addTitleBarWidget(clock, AuTitleBar::SlotCenter, false);

// 靠右（操作按钮左侧）：交互组件，默认抢占鼠标（可正常点击）
QPushButton *btn = new QPushButton("反馈");
win->addTitleBarWidget(btn, AuTitleBar::SlotBeforeButtons);
win->setTitleBarWidgetGrabMouse(btn, true);   // 可动态切换
win->removeTitleBarWidget(btn);               // 移除
```

抢占鼠标 = 组件正常接收鼠标事件（按钮、下拉框等交互组件）；
不抢占 = 组件设置 `WA_TransparentForMouseEvents`，鼠标事件穿透、区域可拖动窗体，
适合纯展示内容（版本号、状态文字等）。

### 5. 关闭拦截

```cpp
win->setCloseInterceptionEnabled(true);
connect(win, &AuWindow::closeIntercepted, win, [win]() {
    if (QMessageBox::question(win, "确认", "确定退出吗？") == QMessageBox::Yes)
        win->forceClose();     // 无视拦截真正关闭
});
```

拦截对 `close()`、标题栏关闭按钮、系统关闭消息（Alt+F4 等）统一生效；
`exec()` 模式下点 X 默认等价 `reject()`（与 QDialog 一致）。

### 6. X11 系统移动（随 Qt 版本自适应）

```cpp
win->setSystemWindowMoveEnabled(false);   // 强制手动路径；Qt 5.15+ 默认开启
```

- **Qt 5.15 及以上**：默认经 `QWindow::startSystemMove() / startSystemResize()`
  把拖动 / 缩放交给窗口管理器，获得完整原生 Quick Tiling（拖顶最大化、
  左右半屏、铺屏预览、snap-assist、拖动贴靠窗口自动还原）。
- **Qt 5.12（本仓库当前版本）**：默认使用 Qt 手动拖动 / 缩放 + 模块内置
  边缘吸附（见「实现要点」第 5 条）。原因：该版本存在上游 bug QTBUG-69716，
  WM 经 `_NET_WM_MOVERESIZE` 接管指针后，本进程后续鼠标移动事件将不可逆
  丢失（光标冻结）——Qt 官方在 5.15 才修复并转正为公开 API。

Windows 平台始终走原生非客户区路径，不受此开关影响。

### 7. 任务栏显示控制

```cpp
win->setTaskbarVisible(false);   // 不显示任务栏图标（同时不出现在 Alt+Tab）
```

Windows 通过 `WS_EX_TOOLWINDOW` 实现，显示时自动带实时缩略图预览；
X11 通过 `Qt::Tool` 标志实现。建议在 `show()` 前设置。

### 8. 样式与主题

```cpp
// 方式一：全局样式（所有未显式 setStyle 的 AuWindow 自动跟随）
AuWindowStyle::setGlobalStyle(AuWindowStyle::createDarkStyle());

// 方式二：单窗体独立样式（窗体接管所有权）
win->setStyle(AuWindowStyle::createLightStyle());

// 方式三：在独立样式上逐项微调
AuWindowStyle *s = AuWindowStyle::createDarkStyle();
s->setTitleBarHeight(40);
s->setCloseButtonHoverBackgroundColor(Qt::red);
win->setStyle(s);
```

可配置项包括：窗体背景 / 描边、标题栏高度与前后景（激活 / 非激活态）、
标题字体与颜色、图标尺寸与间距、操作按钮尺寸 / 图标色 / 悬停与按下背景
（关闭按钮独立配色）、阴影留白 / 模糊 / 颜色（非 Windows 平台生效）。

### 9. 圆角

```cpp
win->setWindowRadius(8);   // 默认 0（直角）
```

Windows 上 1~5 映射为系统小圆角（4px）、>5 映射为系统标准圆角（8px），
配套阴影由系统提供；其它平台按像素裁剪实现。

## API 一览（AuWindow）

| 分类 | API |
| --- | --- |
| 内容区 | `clientWidget()` `setCentralWidget()` `centralWidget()` |
| 标题栏 | `titleBar()` `setTitleBar()` `addTitleBarWidget()` `removeTitleBarWidget()` `setTitleBarWidgetGrabMouse()` |
| 操作按钮 | `setButtonVisible()` `isButtonVisible()`（`AuTitleBar::ButtonMinimize/Maximize/Close`） |
| 圆角 | `setWindowRadius()` `windowRadius()` |
| 关闭拦截 | `setCloseInterceptionEnabled()` `isCloseInterceptionEnabled()` `forceClose()` |
| 任务栏 | `setTaskbarVisible()` `isTaskbarVisible()` |
| X11 系统移动 | `setSystemWindowMoveEnabled()` `isSystemWindowMoveEnabled()` |
| 边缘吸附 | `setEdgeSnapEnabled()` `isEdgeSnapEnabled()` |
| Dialog | `setDialogMode()` `setModality()` `exec()` `done()` `accept()` `reject()` `result()` `setResult()` |
| 样式 | `setStyle()` `style()` |
| 信号 | `closeIntercepted()` `finished(int)` `accepted()` `rejected()` `maximizeStateChanged(bool)` |

## 与 QMainWindow 的对应关系

| QMainWindow / QDialog | AuWindow |
| --- | --- |
| 系统标题栏 | 自绘 `AuTitleBar`（图标 / 标题 / 三按钮 / 自定义组件） |
| `setCentralWidget()` | `setCentralWidget()`（或直接 `setupUi(this)`） |
| `showMaximized()` 等窗口操作 | 继承自 QWidget，行为一致 |
| `QDialog::exec()` | `exec()`（配合 `setDialogMode(true)`） |
| 系统边框拖拽缩放 | 保留（Windows 原生热区 / X11 `_NET_WM_MOVERESIZE`） |

## 注意事项

1. **X11 混成检测**：优先读取 EWMH 混成选择；ukui-kwin 等定制混成器不注册该选择，
   此时以窗口实际 visual（ARGB depth 32）兜底判断——透明生效即启用平滑圆角，
   未生效（无混成）则退化为直角。
2. **X11 原生阴影**：KWin/UKUI 系窗口管理器支持 `_KDE_NET_WM_SHADOW` 协议，
   库会自动设置该属性，由混成器绘制与原生窗口**完全相同的主题阴影**
   （窗口本身即纯内容，无边距环）。非 KWin 系 WM 回退为自绘阴影
   （圆角矩形 SDF + 高斯距离场，亮 / 暗背景均有柔和过渡）。
3. **X11 圆角平滑**：混成下窗口为 ARGB 透明背景，圆角靠 alpha 通道抗锯齿呈现，
   不使用 1-bit 区域裁剪（那是锯齿来源）。
4. **X11 系统移动通道（可选）**：`setSystemWindowMoveEnabled(true)` 后拖动 /
   缩放经 `_NET_WM_MOVERESIZE` 消息交给窗口管理器，可获得拖顶最大化预览、
   边缘半屏分屏、snap-assist、拖动最大化窗口自动还原跟随等 WM 原生行为。
   注意 Qt 5.12 上该通道会导致接管后指针事件丢失（QTBUG-69716），默认
   关闭；WM 未接管时模块自动回退手动路径（150ms 宽限期判定）。
5. **X11 边缘吸附（手动路径，Qt 5.12 默认）**：手动拖动中光标进入屏幕边缘
   吸附带（约 10px）时显示半透明铺屏预览，松开按光标位置吸附——拖到顶部 →
   最大化（仅当标题栏显示最大化按钮），左 / 右边缘 → 贴边半屏（按阴影环
   补偿外扩）。贴靠后再拖标题栏自动还原吸附前尺寸并跟随（保持抓取点相对
   位置）；手动缩放会使还原基准失效。可用 `setEdgeSnapEnabled(false)` 关闭。
6. **X11 缩放手柄**：窗口边缘布置了透明手柄子控件（厚度 = 阴影边距或 6px），
   光标由 Qt 自动切换为对应方向的缩放形状，按下即进入缩放；
   最大化 / 全屏时手柄自动隐藏。
7. **Windows 最小化动画**：保留了原生窗口样式，最小化 / 最大化动画、
   任务栏缩略图、Aero Snap、Alt+Space 系统菜单均为系统原生行为。
8. **子类化标题栏**：可继承 `AuTitleBar` 重写 `paintEvent` 定制绘制，
   通过 `AuWindow::setTitleBar()` 替换（构造时需传入窗体指针 `new MyBar(this)`）。
9. **整窗皮肤**：AuWindow 只负责窗体框架（标题栏 / 背景 / 阴影），
   内容区控件配色建议搭配 QSS 或调色板（Demo 的主题切换有示例）。
