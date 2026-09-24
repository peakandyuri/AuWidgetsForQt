#ifndef AGDATEPICKER_H
#define AGDATEPICKER_H

#include "AgDatePicker_global.h"
#include "AgDatePickerStyle.h"

#include <QWidget>
#include <QDate>
#include <QPointer>
#include <functional>

class QLineEdit;
class QMouseEvent;
class QVariantAnimation;

class AgPopupPanel;
class AgDatePickerPanel;
class AgDateRangePanel;

/**
 * @brief 日期选择控件（目标：替代 QCalendarWidget / QDateEdit 的弹出形态）。
 *
 * 结构：圆角边框编辑区（QLineEdit 只读承载文本）+ 右侧日历图标 + 带投影的弹层
 * 日历面板（AgPopupPanel 内嵌 AgDatePickerPanel）。亮 / 暗两套内置主题，
 * 小 / 中 / 大三种尺寸预设（高度可逐控件微调）。
 *
 * 尺寸约定：控件为固定尺寸（Fixed 尺寸策略），不随父布局拉伸；默认宽度按显示
 * 格式的最长文本计算，可通过 setFixedWidth() / setMinimumWidth() / resize() 调整，
 * 高度用 setEditorHeight() 或 setSizeMode() 调整。弹层面板宽度固定（约 230px，
 * 中档），不随编辑框宽度变化。
 *
 * 交互约定：
 *  - 文本默认以 yyyy-MM-dd 格式化显示，setDisplayFormat() 可自定义
 *    （如 "yyyy/MM/dd"、"yyyy年MM月dd日"）；仅选月默认 yyyy-MM、仅选年默认 yyyy；
 *  - 点击编辑框 / 日历图标弹出面板；面板优先显示在编辑框下方，空间不足自动翻转；
 *  - 面板分 head / body / footer：head 左侧为 "<<"（上一年）"<"（上一月），
 *    中间 "xxxx年xx月"（年、月分段可点击，悬停变主题色，点击分别进入年份 / 月份
 *    选择页），右侧为 ">"（下一月）">>"（下一年）；
 *  - 年份选择页：九宫格风格（4 列 x 3 行 / 页）显示 12 个年份，支持鼠标滚动与
 *    head 右侧上下箭头翻页（每次一屏，带滚动动画、无滚动条），head 左侧显示当前
 *    页年份区间（如 2020 ~ 2031）；
 *  - 月份选择页：4 列 x 3 行一次显示 12 个月，head 左侧年份（可点击进入年份页），
 *    右侧上下箭头 / 鼠标滚动切换年份（格子不变）；
 *  - 日历 body：顶部周一 ~ 周日星期行，下方 6 周日期；前后月补齐日期以灰色显示，
 *    与当月日期区分但均可点击选择；
 *  - 点击年份进入年份选择页后按 Esc 返回日历页（月份页同理）；已在日历页（或
 *    月 / 年粒度的初始页）时 Esc 收起面板；选日粒度下点击年份直接回到对应年月的
 *    日历页，无需再选月；
 *  - setMinimumDate() / setMaximumDate() / setDateRange() 限制可选范围，
 *    范围外日期灰显、无悬停效果、不可点击；
 *  - 已选择日期时面板打开后默认定位到该日期所在月并高亮选中；
 *  - setSelectionMode() 支持仅选年 / 仅选月（yyyy-MM）/ 选日（默认）/ 日期范围
 *    （SelectDateRange），年 / 月模式直接呈现年份 / 月份选择页，同样遵守范围限制；
 *  - 键盘操作：面板展开时方向键上下左右移动焦点日期（自动翻月），PgUp / PgDn
 *    切换月份，回车选中焦点日期；月 / 年九宫格页同样支持方向键与回车；
 *  - 日期范围模式（SelectDateRange）：面板为左右双日历（右侧比左侧晚一个月，
 *    翻月联动），编辑框显示 "开始日期 ~ 结束日期"；第一次点击 / 回车选起点，
 *    此时起点前后日期均可选（选了之前的日期自动交换，第一次选择转为结束日期），
 *    起点到悬停 / 焦点日期之间以淡蓝色范围带实时预览；面板展开期间编辑框中
 *    开始日期下方有主色调下划线，选中起点后带动画滑到结束日期下方，选中终点
 *    后面板收起；
 *  - 清除：setClearButtonEnabled(true) 后，已选日期时悬停编辑框，日历图标切换为
 *    实心圆圈 ×，点击即清除；
 *  - footer：默认居中显示 "今天" / "当月" / "今年"（无下划线超链接效果，点击选择
 *    当前日期），setFooterVisible(false) 可隐藏；setFooterLink() 可配置左 / 中 /
 *    右三个超链接位（文本 + 点击回调），setFooterWidget() 可整体替换为自定义部件；
 *  - 面板展开 / 收起带动画，通过 popupAboutToShow()/popupShown()/
 *    popupAboutToHide()/popupHidden() 回调；Esc 关闭面板；
 *  - setEnabled(false) 整体禁用。
 */
class AGDATEPICKER_EXPORT AgDatePicker : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QDate date READ date WRITE setDate NOTIFY dateChanged)
    Q_PROPERTY(QString displayFormat READ displayFormat WRITE setDisplayFormat)
    Q_PROPERTY(SelectionMode selectionMode READ selectionMode WRITE setSelectionMode)
    Q_PROPERTY(QDate startDate READ startDate WRITE setStartDate NOTIFY dateRangeChanged)
    Q_PROPERTY(QDate endDate READ endDate WRITE setEndDate NOTIFY dateRangeChanged)

public:
    /// 选择粒度：到日（默认）/ 到月（yyyy-MM）/ 到年（yyyy）/ 日期范围（双日历）。
    enum SelectionMode {
        SelectDay,        ///< 年 + 月 + 日
        SelectMonth,      ///< 年 + 月
        SelectYear,       ///< 年
        SelectDateRange   ///< 开始日期 ~ 结束日期（双日历面板）
    };

    /// footer 快捷链接槽位。
    enum FooterSlot {
        FooterLeft,    ///< 左侧
        FooterCenter,  ///< 中间（默认 "今天" / "当月" / "今年"）
        FooterRight    ///< 右侧
    };

    explicit AgDatePicker(QWidget *parent = nullptr);
    ~AgDatePicker() override;

    // ---- 当前值 ----
    QDate date() const;                 ///< 范围模式下为开始日期
    bool hasSelection() const;
    QDate startDate() const;            ///< 范围模式：开始日期（无值返回无效 QDate）
    QDate endDate() const;              ///< 范围模式：结束日期（无值返回无效 QDate）

    // ---- 显示格式 ----
    QString displayFormat() const;

    // ---- 选择粒度 ----
    SelectionMode selectionMode() const;

    // ---- 可选范围 ----
    QDate minimumDate() const;                      ///< 无效 QDate = 不限
    QDate maximumDate() const;                      ///< 无效 QDate = 不限
    bool isDateSelectable(const QDate &date) const; ///< 范围判断（无范围时恒 true）

    // ---- 编辑框 ----
    QString placeholderText() const;
    int editorHeight() const;

    // ---- 范围模式占位文本（SelectDateRange）----
    /// 配置两侧占位文本（编辑框整体为空时灰显 "start ~ end"；起始已选而结束为空时
    /// 在分隔符后灰显结束占位）。传空 QString 恢复默认 "开始日期 / 结束日期"。
    void setRangePlaceholderText(const QString &start, const QString &end);
    QString rangeStartPlaceholder() const;
    QString rangeEndPlaceholder() const;

    // ---- 清除按钮 ----
    bool isClearButtonEnabled() const;

    // ---- footer ----
    bool isFooterVisible() const;
    QWidget *footerWidget() const;                  ///< 自定义 footer 部件（未设置返回 nullptr）

    // ---- 样式 / 尺寸 ----
    AgDatePickerStyle *style() const;               ///< 未单独设置时返回全局样式
    AgDatePickerStyle::SizeMode sizeMode() const;

    // ---- 面板 ----
    bool isPopupVisible() const;

public Q_SLOTS:
    // ---- 当前值 ----
    void setDate(const QDate &date);                ///< 传无效 QDate 清空；范围模式下仅设置开始日期
    /// 设置范围值（自动按时间先后排序；传无效开始日期则整体清空）。
    void setSelectedRange(const QDate &start, const QDate &end);
    void setStartDate(const QDate &start);          ///< 范围模式：仅设置开始日期
    void setEndDate(const QDate &end);              ///< 范围模式：仅设置结束日期
    void clear();

    // ---- 显示格式 ----
    void setDisplayFormat(const QString &format);

    // ---- 选择粒度 ----
    void setSelectionMode(SelectionMode mode);      ///< 未自定义格式时默认格式随之切换

    // ---- 可选范围 ----
    void setMinimumDate(const QDate &min);          ///< 传无效 QDate 取消下限
    void setMaximumDate(const QDate &max);          ///< 传无效 QDate 取消上限
    void setDateRange(const QDate &min, const QDate &max);

    // ---- 编辑框 ----
    void setPlaceholderText(const QString &text);
    void setEditorHeight(int height);               ///< 覆盖尺寸档位编辑框高度（派生独立样式）

    // ---- 清除按钮 ----
    void setClearButtonEnabled(bool enabled);

    // ---- footer ----
    void setFooterVisible(bool visible);
    /// 配置 footer 快捷链接（覆盖该槽位的默认项；callback 为空则仅展示）。
    void setFooterLink(FooterSlot slot, const QString &text,
                       const std::function<void()> &callback = nullptr);
    /// 恢复该槽位默认（中间槽位恢复 "今天" / "当月" / "今年"，左右恢复为空）。
    void resetFooterLink(FooterSlot slot);
    /// 整体替换 footer 为自定义部件（所有权转移给控件）；传 nullptr 恢复内置 footer。
    void setFooterWidget(QWidget *widget);

    // ---- 样式 / 尺寸 ----
    /// 设置控件独立样式（所有权转移给控件）；传 nullptr 恢复使用全局样式。
    void setStyle(AgDatePickerStyle *style);
    /// 切换尺寸档位（样式实例的尺寸类字段随之重置）。
    void setSizeMode(AgDatePickerStyle::SizeMode mode);

    // ---- 面板 ----
    void showPopup();
    void hidePopup();

Q_SIGNALS:
    /// 日期变化（程序设置或用户选择；无效 QDate = 已清空）。
    void dateChanged(const QDate &date);
    /// 范围模式：范围值变化（程序设置或用户选择；无效 QDate = 已清空）。
    void dateRangeChanged(const QDate &start, const QDate &end);
    /// 用户在面板中选择日期（含 footer 快捷选择；范围模式下为开始日期）。
    void activated(const QDate &date);
    /// 用户在面板中确定日期范围（第二次点击 / 回车）。
    void rangeActivated(const QDate &start, const QDate &end);
    /// 用户通过清除按钮 / clear() 清空。
    void cleared();
    /// 面板显示 / 隐藏回调（About = 动画开始前，完成 = 动画结束后）。
    void popupAboutToShow();
    void popupShown();
    void popupAboutToHide();
    void popupHidden();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void enterEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    friend class AgDatePickerStyle;
    void onGlobalStyleChanged();            ///< 全局样式变化（跟随全局 / 派生样式的控件响应）

    static QString defaultFormatForMode(SelectionMode mode);

    // 构建 / 样式 / 布局
    void ensurePopup();
    void ensurePopupContent();              ///< 按粒度创建弹层内容（单日历 / 双日历，切换时重建）
    void ensureOwnStyle();                 ///< 从全局样式派生私有样式（setSizeMode/setEditorHeight 用）
    void applyStyleToWidgets();
    void relayoutEditor();
    int effectiveEditorHeight() const;
    int fixedEditorWidth() const;          ///< 按显示格式最长文本计算的固定宽度
    QRect innerRect() const;
    QRect iconSlotRect() const;             ///< 日历 / 清除图标热区
    bool clearIconActive() const;           ///< 当前悬停状态下应显示清除图标
    void updateIconCursor(const QPoint &pos);

    // 值
    void updateEditorText();
    void syncPanelToValue();      ///< 值被程序改变时同步展开中的面板（高亮 + 翻到所在月）
    void commitRange(const QDate &start, const QDate &end);   ///< 范围模式提交
    void handlePanelDateSelected(const QDate &date);
    void handleEscape();                    ///< Esc：年 / 月选择页返回上一级，初始页收起面板
    void pushFooterConfig();                ///< footer 配置同步到面板（未弹出时挂起）
    void setFixedRange(const QDate &min, const QDate &max);

    // 范围模式交互
    void updateRangePreview();              ///< 预览终点 = 悬停日期（无则键盘焦点日期）
    qreal rangeEditorTextX() const;         ///< 编辑框文本起始 x（借输入法光标几何）
    QRectF rangeUnderlineRect(bool endSegment) const; ///< 下划线目标段几何
    void animateUnderlineTo(bool endSegment);         ///< 下划线移动（带动画 / 首次直接就位）
    void navigatePopup(int key);            ///< 面板展开期间的方向键 / 翻月键
    void activatePopupFocus();              ///< 面板展开期间的回车激活

    // footer 配置
    struct FooterLinkData {
        QString text;
        std::function<void()> callback;
        bool custom = false;                ///< 用户覆盖（否则中间槽位用内置默认项）
    };
    FooterLinkData m_footerLinks[3];

    QLineEdit *m_lineEdit;
    AgPopupPanel *m_panel;
    QWidget *m_content;                     ///< 当前安装的弹层内容（内容切换时由面板销毁旧部件）
    QPointer<AgDatePickerPanel> m_dayContent;   ///< 单日历内容（SelectDay/Month/Year）
    QPointer<AgDateRangePanel> m_rangeContent;  ///< 双日历内容（SelectDateRange）

    AgDatePickerStyle *m_ownStyle;
    bool m_styleFromSizeMode;               ///< own 样式由 setSizeMode/setEditorHeight 从全局样式派生
    AgDatePickerStyle::SizeMode m_overrideSizeMode;

    QDate m_date;                           ///< 范围模式下为开始日期
    QDate m_endDate;                        ///< 范围模式：结束日期
    QString m_displayFormat;
    bool m_formatCustom;                    ///< 用户显式设置过格式（切换粒度时不重置）

    SelectionMode m_selectionMode;
    QDate m_minimumDate;
    QDate m_maximumDate;

    // 范围选择过程状态（仅面板展开期间有效）
    bool m_picking;                         ///< 已选起点、待选终点
    QDate m_pickStart;                      ///< 本次选择的起点（提交前）
    QDate m_hoverDate;                      ///< 面板悬停日期（范围预览用）
    bool m_rangePlaceholderAuto;            ///< 范围模式默认占位文本（离开范围模式时清除）
    QString m_rangeStartPlaceholder;        ///< 范围模式开始占位（默认 "开始日期"）
    QString m_rangeEndPlaceholder;          ///< 范围模式结束占位（默认 "结束日期"）

    // 编辑框下划线（范围模式：提示当前选择的是开始 / 结束日期）
    QVariantAnimation *m_underlineAnim;
    QRectF m_underlineRect;                 ///< 当前位置（无效 = 不绘制）

    bool m_clearButtonEnabled;
    bool m_footerVisible;
    QWidget *m_footerWidget;                ///< 完全自定义 footer（所有权归控件）

    bool m_areaHovered;                     ///< 鼠标悬停在控件本体（非行编辑框区域）
    bool m_lineHovered;                     ///< 鼠标悬停在行编辑框上
    bool m_iconHovered;                     ///< 鼠标悬停在图标槽位
    int m_preferredHeight;                  ///< 当前尺寸档位下的期望高度
};

Q_DECLARE_METATYPE(AgDatePicker::SelectionMode)

#endif // AGDATEPICKER_H
