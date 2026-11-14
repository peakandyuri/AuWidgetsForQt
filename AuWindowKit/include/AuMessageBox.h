#ifndef AUMESSAGEBOX_H
#define AUMESSAGEBOX_H

#include "AuWindowKit_global.h"
#include "AuWindow.h"
#include "AuPushButton.h"

#include <QPixmap>
#include <QHash>
#include <QPair>
#include <QVector>

class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class AuMessageBoxStyle;

/**
 * @brief 统一风格消息框：QMessageBox 用法的 AuWindow 版本。
 *
 * 图标、正文、按钮全部自绘 / 模块内取值，不落平台原生对话框，跨平台观感一致；
 * 窗体骨架（无边框标题栏、圆角、阴影、模态 exec）直接继承 AuWindow。
 *
 * 基本用法（与 QMessageBox 同构）：
 * \code
 * if (AuMessageBox::question(this, QStringLiteral("删除确认"),
 *         QStringLiteral("确定要删除选中的 3 个文件吗？")) == AuMessageBox::Yes) {
 *     doDelete();
 * }
 * \endcode
 *
 * 实例用法（自定义按钮 / 文案 / 内容）：
 * \code
 * AuMessageBox box(this);
 * box.setWindowTitle(QStringLiteral("更新"));
 * box.setIcon(AuMessageBox::Information);
 * box.setText(QStringLiteral("发现新版本 2.3.1"));
 * box.setInformativeText(QStringLiteral("更新需重启应用"));
 * box.setStandardButtons(AuMessageBox::Ok | AuMessageBox::Cancel);
 * box.setButtonText(AuMessageBox::Ok, QStringLiteral("立即更新"));  // 逐实例改文案
 * if (box.exec() == AuMessageBox::Ok) { ... }
 * \endcode
 *
 * 按钮文案（改中文零配置）：
 *  - 默认文案即为中文：确定 / 是 / 否 / 取消 / 关闭；
 *  - 全局覆盖：AuMessageBox::setDefaultButtonText()（应用启动时设置一次即可）；
 *  - 实例覆盖：setButtonText()，优先级高于全局覆盖；
 *  - 任意按钮：addButton(文字, 角色) 返回 AuPushButton*，点击后经
 *    clickedButton() / finished() 区分。
 *
 * 自定义内容：setCustomContent() 在文本与按钮行之间插入任意控件
 * （"不再提示"勾选框、进度条、输入框等），布局参与整体尺寸计算。
 *
 * 样式：本体（文本 / 图标 / 按钮）经 AuMessageBoxStyle（全局 / 实例 / 亮暗
 * 工厂三分法）；窗体骨架继续跟随 AuWindowStyle::globalStyle()。
 *
 * 与 QMessageBox 的差异：
 *  - 模态范围跟随 AuWindow::exec()（有父窗口 WindowModal，无父窗口
 *    ApplicationModal）；
 *  - Esc / 标题栏关闭 / 关闭按钮 = 取消：返回值为取消类标准按钮
 *    （Cancel 优先于 Close、No；集合内不存在时回退 Ok / NoButton）；
 *  - 按钮排布固定一套（主按钮靠右、取消靠最右），不随平台主题切换；
 *  - setDetailedText()（"显示详情"折叠区）暂未提供，后续版本补充。
 */
class AUWINDOWKIT_EXPORT AuMessageBox : public AuWindow
{
    Q_OBJECT

public:
    /// 内置图标（矢量绘制的圆形徽章；贴图自定义见 setIconPixmap）。
    enum Icon {
        NoIcon = 0,
        Information = 1,
        Warning = 2,
        Critical = 3,
        Question = 4
    };
    Q_ENUM(Icon)

    /// 标准按钮。数值与 QMessageBox::StandardButton 对应项一致，便于经验换算。
    enum StandardButton {
        NoButton = 0x00000000,
        Ok       = 0x00000400,
        Yes      = 0x00004000,
        No       = 0x00010000,
        Close    = 0x00200000,
        Cancel   = 0x00400000
    };
    Q_ENUM(StandardButton)
    Q_DECLARE_FLAGS(StandardButtons, StandardButton)

    /// 按钮角色（数值与 QMessageBox::ButtonRole 对应项一致）。
    enum ButtonRole {
        InvalidRole = -1,
        AcceptRole = 0,     ///< 接受动作（点击后 done(Accepted)）
        RejectRole = 1,     ///< 拒绝动作（点击后 done(Rejected)）
        DestructiveRole = 2,
        ActionRole = 3,     ///< 一般动作（点击后 done(Rejected)，用 clickedButton 区分）
        HelpRole = 4,
        YesRole = 5,
        NoRole = 6
    };
    Q_ENUM(ButtonRole)

    explicit AuMessageBox(QWidget *parent = nullptr);
    ~AuMessageBox() override;

    // ---- 静态便捷函数（模态执行，返回被点击的标准按钮） ----
    static StandardButton information(QWidget *parent, const QString &title, const QString &text,
                                      StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton warning(QWidget *parent, const QString &title, const QString &text,
                                  StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton critical(QWidget *parent, const QString &title, const QString &text,
                                   StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton question(QWidget *parent, const QString &title, const QString &text,
                                   StandardButtons buttons = StandardButtons(Yes | No),
                                   StandardButton defaultButton = NoButton);

    // ---- 标准按钮文案（改中文零配置：默认即中文，支持全局 / 实例两级覆盖） ----
    /// 全局覆盖标准按钮文案（应用启动时设置一次，对所有消息框生效）。
    /// text 传空字符串恢复内置默认文案。
    static void setDefaultButtonText(StandardButton button, const QString &text);
    /// 当前生效的标准按钮文案（全局覆盖后返回覆盖值）。
    static QString standardButtonText(StandardButton button);
    /// 实例覆盖标准按钮文案（优先级高于全局覆盖）；text 传空字符串恢复全局值。
    void setButtonText(StandardButton button, const QString &text);

    // ---- 文本 ----
    void setText(const QString &text);
    QString text() const;
    void setInformativeText(const QString &text);
    QString informativeText() const;

    // ---- 图标 ----
    void setIcon(Icon icon);
    Icon icon() const;
    /// 自定义图标贴图（优先于内置图标；传空 QPixmap 恢复内置图标）。
    void setIconPixmap(const QPixmap &pixmap);
    QPixmap iconPixmap() const;

    // ---- 按钮 ----
    void setStandardButtons(StandardButtons buttons);
    StandardButtons standardButtons() const;
    /// 已创建的标准按钮（未在集合内返回 nullptr）。
    AuPushButton *button(StandardButton which) const;
    /// 添加自定义按钮（排在标准按钮之后）；点击后 done(角色语义的结果码)，
    /// 用 clickedButton() / buttonClicked() 区分具体按钮。
    AuPushButton *addButton(const QString &text, ButtonRole role = ActionRole);
    /// 设置默认按钮（主按钮形态 + 回车触发 + 获得初始焦点）。传 nullptr 取消。
    void setDefaultButton(StandardButton which);
    void setDefaultButton(AuPushButton *button);
    AuPushButton *defaultButton() const;
    /// 最近一次点击的按钮（含自定义按钮；尚未点击返回 nullptr）。
    AuPushButton *clickedButton() const;

    // ---- 内容级自定义 ----
    /// 在文本与按钮行之间插入自定义控件（勾选框 / 进度条 / 输入框等）。
    /// 重复调用替换旧控件（旧控件销毁）；传 nullptr 移除。
    void setCustomContent(QWidget *widget);
    QWidget *customContent() const;

    // ---- 样式 ----
    /// 消息框本体样式（接管所有权）；nullptr 恢复跟随全局样式。
    /// 传入 AuMessageBoxStyle::globalStyle() 返回的全局指针时直接跳过不处理
    /// （全局对象归 setGlobalStyle 管理，不可被实例接管）。
    /// 注意：AuWindow::setStyle(AuWindowStyle*) 仍然可用（using 保留），
    /// 窗体骨架样式请走 AuWindowStyle::globalStyle()。
    using AuWindow::setStyle;
    void setStyle(AuMessageBoxStyle *style);
    /// 当前生效的消息框本体样式（实例样式优先，否则全局样式）。
    AuMessageBoxStyle *style() const;
    /// 显式取窗体骨架样式（被本类 style() 覆盖隐藏，此处转回 AuWindow 语义）。
    AuWindowStyle *windowStyle() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// 任一按钮被点击（标准按钮给按钮值，自定义按钮给 NoButton）。
    void buttonClicked(AuMessageBox::StandardButton button);

protected:
    void setVisible(bool visible) override;
    void showEvent(QShowEvent *event) override;

private:
    friend class AuMessageBoxStyle;

    // 全局本体样式变化通知（AuMessageBoxStyle 派发；实例样式不受影响）
    void onGlobalMessageStyleChanged();
    // 样式 → 子控件下发（字体 / 颜色 / 间距 / 按钮视觉 / 图标尺寸）
    void applyMessageStyle();
    /// 单个按钮的视觉下发（普通 / 主按钮两套取值）
    void applyButtonVisual(AuPushButton *button);
    // 构建内容区（文本块 + 按钮行）
    void buildContent();
    // 按标准按钮集合重建按钮行（保留自定义按钮）
    void rebuildButtonRow();
    // 内置矢量图标绘制
    void refreshIcon();
    // 按内容计算窗体尺寸并固定（消息框不支持用户缩放）
    void fixContentSize();
    // 静态便捷函数共用执行体
    static StandardButton runMessageBox(QWidget *parent, const QString &title, const QString &text,
                                        Icon icon, StandardButtons buttons, StandardButton defaultButton);
    // 展示顺序内的第一个标准按钮（默认按钮缺省选取）
    AuPushButton *firstStandardButton() const;
    // Esc / 关闭兜底返回值：取消类按钮优先（Cancel > Close > No），回退 Ok
    StandardButton escapeStandardButton() const;
    // 实例生效文案：实例覆盖 → 全局覆盖 → 内置中文默认
    QString effectiveButtonText(StandardButton button) const;
    // 单个标准按钮的文案刷新（覆盖变化时调用）
    void refreshStandardButtonText(StandardButton button);
    static QString builtinButtonText(StandardButton button);

    QWidget *m_content;
    QVBoxLayout *m_contentLayout;
    QHBoxLayout *m_textRowLayout;               ///< 图标 + 文本列的横排（iconSpacing 取值点）
    QLabel *m_iconLabel;
    QLabel *m_messageLabel;
    QLabel *m_informativeLabel;
    QWidget *m_customContent;
    QHBoxLayout *m_buttonRow;

    Icon m_icon;
    QPixmap m_iconPixmap;                       // 非空 = 自定义贴图优先
    StandardButtons m_standardButtonFlags;
    QHash<int, AuPushButton *> m_standardButtons;
    QHash<int, QString> m_buttonTextOverrides;  // 实例级文案覆盖
    QVector<QPair<AuPushButton *, ButtonRole>> m_customButtons;
    AuPushButton *m_defaultButton;
    StandardButton m_defaultStandard;           ///< 默认按钮对应的标准按钮值（自定义按钮为 NoButton）
    AuPushButton *m_clickedButton;
    StandardButton m_clickedStandard;

    AuMessageBoxStyle *m_style;                 // nullptr = 跟随全局样式
    bool m_sizeFixed;                           // 已按内容固定尺寸
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AuMessageBox::StandardButtons)

#endif // AUMESSAGEBOX_H
