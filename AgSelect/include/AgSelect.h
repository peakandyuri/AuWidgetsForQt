#ifndef AGSELECT_H
#define AGSELECT_H

#include "AgSelect_global.h"
#include "AgSelectStyle.h"

#include <QWidget>
#include <QIcon>
#include <QVariant>

class QLineEdit;
class QMouseEvent;
class QPropertyAnimation;

class AgPopupPanel;
class AgSelectListModel;
class AgSelectFilterProxyModel;
class AgSelectItemDelegate;
class AgSelectListView;

/**
 * @brief 下拉选择控件（替代 QComboBox）。
 *
 * 结构：圆角边框编辑区（QLineEdit 承载文本）+ 右侧下拉箭头 + 带投影的弹层面板
 * （QListView 渲染选项，AgScrollBar 滚动）。亮 / 暗两套内置主题，小 / 中 / 大
 * 三种尺寸预设，单选与多选（chips）模式。
 *
 * 交互约定：
 *  - 编辑框默认只读，点击弹出面板；setEditable(true) 后允许输入，输入实时过滤
 *    下拉项（命中文字高亮），选择后回填文本；输入内容与任何选项都不匹配时，
 *    失去焦点或面板隐藏后还原为当前选中项文本（无选中则清空）；
 *  - 面板默认与编辑框同宽，优先显示在编辑框下方，下方空间不足自动翻转到上方；
 *    宽度可切换为按内容自适应或固定宽度，始终收敛在屏幕可用区内；
 *  - 面板展开 / 收起带 100ms 动画（下方自上而下、上方自下而上），
 *    通过 popupAboutToShow()/popupShown()/popupAboutToHide()/popupHidden() 回调；
 *  - 面板展开时点击编辑区（文本框 / chip 主体 / 空白区 / 箭头）收起面板；多选 chip 的
 *    × 移除不受影响（面板保持当前状态）；点击面板与控件之外的区域、焦点移出控件、
 *    宿主窗口失去激活 / 被拖动或按下 Esc 时自动收起；
 *  - 面板展开即获得键盘焦点：Up/Down/Home/End/PageUp/PageDown 移动高亮、Enter 选择、
 *    Esc 关闭，不会误操作其它焦点控件；
 *  - 多选：点击选项切换勾选（已勾选项右侧显示对勾），编辑区以 chip（标签块）
 *    呈现已选项，chip 右侧 × 可移除，支持换行；
 *  - 支持分组（组名行不可选中、无 hover）、禁用项（灰显不可选）、自定义委托渲染；
 *  - setEnabled(false) 整体禁用；键盘支持 Up/Down/Home/End/PageUp/PageDown/
 *    Enter 选择、Esc 关闭。
 */
class AGSELECT_EXPORT AgSelect : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString currentText READ currentText WRITE setCurrentText NOTIFY currentTextChanged)
    Q_PROPERTY(bool editable READ isEditable WRITE setEditable)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)

public:
    /// 下拉面板宽度模式。
    enum PopupWidthMode {
        PopupMatchEditor,   ///< 与编辑框同宽（默认）
        PopupFixed,         ///< 固定宽度 fixedPopupWidth()（不小于编辑框宽度）
        PopupFitContent     ///< 按选项内容自适应（不小于编辑框宽度）
    };

    explicit AgSelect(QWidget *parent = nullptr);
    ~AgSelect() override;

    // ---- 条目管理（QComboBox 风格 + 分组）----
    int addItem(const QString &text, const QVariant &userData = QVariant(),
                const QIcon &icon = QIcon());
    int addItems(const QStringList &texts);
    /// 追加一个分组标题行，其后的 addItem 归入该组；传空串结束当前分组（后续项不缩进）。
    void addGroup(const QString &title);
    void insertItem(int index, const QString &text, const QVariant &userData = QVariant(),
                    const QIcon &icon = QIcon());
    void removeItem(int index);
    void clear();
    int count() const;
    QString itemText(int index) const;
    QVariant itemData(int index) const;
    QIcon itemIcon(int index) const;
    bool isItemEnabled(int index) const;
    void setItemEnabled(int index, bool enabled);
    void setItemIcon(int index, const QIcon &icon);
    /// 按显示文本查找（flags 支持 MatchExactly / MatchContains / MatchCaseSensitive）。
    int findText(const QString &text,
                 Qt::MatchFlags flags = static_cast<Qt::MatchFlags>(Qt::MatchExactly)) const;
    /// 按用户数据查找（flags 同 findText）。
    int findData(const QVariant &data,
                 Qt::MatchFlags flags = static_cast<Qt::MatchFlags>(Qt::MatchExactly)) const;

    // ---- 当前值（单选语义；多选模式下为最近一次激活的选项）----
    int currentIndex() const;
    QString currentText() const;
    QVariant currentData() const;

    // ---- 多选 ----
    bool isMultiSelection() const;
    QList<int> checkedIndexes() const;      ///< 按条目顺序返回已勾选索引
    QStringList checkedTexts() const;
    QVariantList checkedData() const;

    // ---- 可编辑与占位 ----
    bool isEditable() const;
    QString placeholderText() const;

    // ---- 样式 / 尺寸 / 委托 ----
    AgSelectStyle *style() const;           ///< 未单独设置时返回全局样式
    AgSelectStyle::SizeMode sizeMode() const;
    AgSelectItemDelegate *itemDelegate() const;

    // ---- 面板 ----
    PopupWidthMode popupWidthMode() const;
    int fixedPopupWidth() const;
    /// 面板内容区最大高度；<=0 时使用样式 panelMaxHeight()。
    int maxPopupHeight() const;
    bool isPopupVisible() const;

    // ---- 箭头 ----
    /// 当前箭头部件（未替换时返回内置箭头）。
    QWidget *arrowWidget() const;

public Q_SLOTS:
    void setCurrentIndex(int index);
    void setCurrentText(const QString &text);
    void setMultiSelection(bool multi);
    void setCheckedIndexes(const QList<int> &indexes);   ///< 仅多选模式生效
    void clearChecked();
    void setEditable(bool editable);
    void setPlaceholderText(const QString &text);
    /// 设置控件独立样式（所有权转移给控件）；传 nullptr 恢复使用全局样式。
    void setStyle(AgSelectStyle *style);
    /// 切换尺寸档位（样式实例的尺寸类字段随之重置）。
    void setSizeMode(AgSelectStyle::SizeMode mode);
    /// 替换下拉项委托（所有权转移给控件）。
    void setItemDelegate(AgSelectItemDelegate *delegate);
    void setPopupWidthMode(PopupWidthMode mode);
    void setFixedPopupWidth(int width);
    void setMaxPopupHeight(int height);
    void showPopup();
    void hidePopup();
    /// 替换箭头部件（所有权转移给控件）；传 nullptr 恢复内置箭头。
    void setArrowWidget(QWidget *widget);

Q_SIGNALS:
    void currentIndexChanged(int index);
    void currentTextChanged(const QString &text);
    /// 用户激活选项（单选点选 / 键盘回车；多选勾选或取消勾选）。
    void activated(int index);
    /// 可编辑模式下输入文本（用于外部联动）。
    void textEdited(const QString &text);
    /// 多选勾选集合变化。
    void checkedChanged(const QList<int> &indexes);
    /// 面板显示 / 隐藏回调（About = 动画开始前，完成 = 动画结束后）。
    void popupAboutToShow();
    void popupShown();
    void popupAboutToHide();
    void popupHidden();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    friend class AgSelectStyle;
    void onGlobalStyleChanged();            ///< 全局样式变化（跟随全局 / 派生样式的控件响应）

    // 构建 / 样式 / 布局
    void ensurePopup();
    void applyStyleToWidgets();
    void relayoutEditor();
    int editorHeight() const;
    int borderSpan() const;                 ///< 边框总宽（2 * width）
    QRect innerRect() const;
    int arrowSlotWidth() const;

    // 编辑文本 / 过滤
    void updateEditorText(const QString &text);
    void revertUnmatchedText();             ///< 失焦 / 面板隐藏后还原不匹配的输入
    void applyFilter(const QString &text);

    // 选择逻辑
    void activateSourceRow(int sourceRow);
    void handleItemClicked(const QModelIndex &proxyIndex);
    void syncCurrentRowToModel();           ///< 单选：把当前项同步为委托的选中态
    void setViewHighlightRow(int sourceRow);
    int acceptableRowFrom(int startRow, int step) const;   ///< 键盘导航：跳过组名与禁用项
    void moveHighlight(int step);
    void activateHighlight();
    void scrollToCurrentRow();

    // 面板
    int fitContentWidth() const;
    int fullContentHeight() const;
    int effectiveMaxPopupHeight() const;
    void rotateArrow(bool open);            ///< 内置箭头随面板展开 / 收起旋转（自定义箭头不处理）

    // chips
    int chipAt(const QPoint &pos) const;
    int chipRemoveAt(const QPoint &pos) const;

    QLineEdit *m_lineEdit;
    AgPopupPanel *m_panel;
    AgSelectListView *m_listView;
    AgSelectListModel *m_model;
    AgSelectFilterProxyModel *m_proxy;
    AgSelectItemDelegate *m_delegate;
    AgSelectStyle *m_ownStyle;
    bool m_styleFromSizeMode;               ///< own 样式由 setSizeMode 从全局样式派生（仍跟随全局主题）
    AgSelectStyle::SizeMode m_overrideSizeMode;

    QWidget *m_arrowWidget;                 ///< 内置箭头或用户替换的箭头
    QPropertyAnimation *m_arrowAnimation;

    bool m_multiSelection;
    bool m_editable;
    bool m_areaHovered;                     ///< 鼠标悬停在控件区域（不含行编辑框）
    bool m_lineHovered;                     ///< 鼠标悬停在行编辑框上
    int m_currentIndex;
    int m_activeGroupId;                    ///< addGroup() 后的当前分组

    PopupWidthMode m_popupWidthMode;
    int m_fixedPopupWidth;
    int m_maxPopupHeight;

    // chips 布局缓存
    QVector<QPair<int, QRect>> m_chipRects;     ///< 源行号 -> chip 矩形
    QVector<QRect> m_chipRemoveRects;
    int m_hoverChip;
    bool m_hoverRemove;
    int m_preferredHeight;                      ///< 当前尺寸档位与内容下的期望高度
};

Q_DECLARE_METATYPE(AgSelect::PopupWidthMode)

#endif // AGSELECT_H
