#ifndef AUCELLBUTTON_H
#define AUCELLBUTTON_H

#include "AuGrid_global.h"

#include <QColor>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QMetaType>

/**
 * @brief 单个状态下的按钮外观（AuCellButton 的 normal / hover / pressed 使用）。
 *
 * 三个颜色均为可选项（无效 QColor 表示未设置）：未设置的颜色由渲染器按
 * 派生规则补齐——hover 在默认样式基础上提亮，pressed 加深；Button 类型
 * 的文字色缺省取控件 textColor，背景缺省透明、边框取文字色半透明。
 */
struct AuCellButtonStyle
{
    QColor textColor;         ///< 文字色（无效 = 用派生默认）
    QColor backgroundColor;   ///< 背景色；Link 类型忽略
    QColor borderColor;       ///< 边框色；Link 类型忽略

    bool isDefault() const
    {
        return !textColor.isValid() && !backgroundColor.isValid() && !borderColor.isValid();
    }
};

/**
 * @brief 内置按钮渲染器（AuButtonCellRenderer）的按钮定义（值类型）。
 *
 * 最小用法只填 text；id 缺省取 text，作为 cellButtonClicked 信号里的
 * 识别键。hover / pressed 样式未设置时自动派生，常用场景可零样式配置，
 * 或直接使用 primary / danger 等语义预设。
 *
 * @code
 *   AuCellButton edit = AuCellButton::primary(QStringLiteral("编辑"));
 *   AuCellButton del  = AuCellButton::danger(QStringLiteral("删除"));
 *   renderer->setButtons({edit, del});
 * @endcode
 */
struct AUGRID_EXPORT AuCellButton
{
    /// 按钮形态：描边按钮 / 文字链接（链接 hover 显示下划线）。
    enum class Type {
        Button,   ///< 描边按钮（圆角矩形背景 + 边框）
        Link      ///< 文字链接（透明背景，文字色 + hover 下划线）
    };

    QString id;                 ///< 点击识别键（缺省取 text），随 cellButtonClicked 发出
    QString text;               ///< 按钮文字
    QString tooltip;            ///< 悬停提示（空 = 不显示）
    Type type = Type::Button;   ///< 按钮形态
    AuCellButtonStyle normal;   ///< 默认态（颜色缺省用渲染器默认色板）
    AuCellButtonStyle hover;    ///< 悬停态；未设置的颜色由 normal 提亮派生
    AuCellButtonStyle pressed;  ///< 按下态；未设置的颜色由 normal 加深派生
    int radius = -1;            ///< 圆角半径（-1 = 渲染器默认）；Link 类型忽略
    bool enabled = true;        ///< false 时灰显且不响应点击
    QVariant userData;          ///< 自定义数据，点击时随信号原样带回

    bool isValid() const { return !text.isEmpty(); }

    /// 序列化为 QVariantMap（数据驱动单元格值 / cellButtonClicked 信号载荷使用）。
    QVariantMap toMap() const;
    /// 从 QVariantMap 反序列化（键见 toMap()；颜色接受 "#RRGGBB" 等字符串）。
    static AuCellButton fromMap(const QVariantMap &map);

    // ---- 语义预设（预置配色，免配样式）----
    static AuCellButton primary(const QString &text);   ///< 主色（蓝）
    static AuCellButton danger(const QString &text);    ///< 危险操作（红，删除等）
    static AuCellButton success(const QString &text);   ///< 成功操作（绿，通过等）
    static AuCellButton warning(const QString &text);   ///< 警告操作（橙，驳回等）
    static AuCellButton plain(const QString &text);     ///< 中性（使用控件默认文字色）
};

Q_DECLARE_METATYPE(AuCellButton)

#endif // AUCELLBUTTON_H
