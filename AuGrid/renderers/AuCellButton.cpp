#include "AuCellButton.h"

namespace
{

// QVariant → QColor：支持 "#RRGGBB" / "#AARRGGBB" / 颜色名字符串与 0xRRGGBB 整数
QColor auColorFromVariant(const QVariant &value)
{
    if (!value.isValid())
        return QColor();
    const QString text = value.toString();
    if (!text.isEmpty()) {
        const QColor color(text);
        if (color.isValid())
            return color;
    }
    bool ok = false;
    const uint rgb = value.toUInt(&ok);
    if (ok)
        return QColor::fromRgb(rgb);
    return QColor();
}

QVariantMap auStyleToMap(const AuCellButtonStyle &style)
{
    QVariantMap map;
    if (style.textColor.isValid())
        map.insert(QStringLiteral("textColor"), style.textColor.name(QColor::HexArgb));
    if (style.backgroundColor.isValid())
        map.insert(QStringLiteral("backgroundColor"), style.backgroundColor.name(QColor::HexArgb));
    if (style.borderColor.isValid())
        map.insert(QStringLiteral("borderColor"), style.borderColor.name(QColor::HexArgb));
    return map;
}

AuCellButtonStyle auStyleFromMap(const QVariantMap &map)
{
    AuCellButtonStyle style;
    style.textColor = auColorFromVariant(map.value(QStringLiteral("textColor")));
    style.backgroundColor = auColorFromVariant(map.value(QStringLiteral("backgroundColor")));
    style.borderColor = auColorFromVariant(map.value(QStringLiteral("borderColor")));
    return style;
}

AuCellButton auPreset(const QString &text, const QColor &color)
{
    AuCellButton button;
    button.text = text;
    button.normal.textColor = color;
    return button;
}

} // namespace

QVariantMap AuCellButton::toMap() const
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), id.isEmpty() ? text : id);
    map.insert(QStringLiteral("text"), text);
    if (!tooltip.isEmpty())
        map.insert(QStringLiteral("tooltip"), tooltip);
    map.insert(QStringLiteral("type"), type == Type::Link ? QStringLiteral("link") : QStringLiteral("button"));
    if (!normal.isDefault())
        map.insert(QStringLiteral("normal"), auStyleToMap(normal));
    if (!hover.isDefault())
        map.insert(QStringLiteral("hover"), auStyleToMap(hover));
    if (!pressed.isDefault())
        map.insert(QStringLiteral("pressed"), auStyleToMap(pressed));
    if (radius >= 0)
        map.insert(QStringLiteral("radius"), radius);
    map.insert(QStringLiteral("enabled"), enabled);
    if (userData.isValid())
        map.insert(QStringLiteral("userData"), userData);
    return map;
}

AuCellButton AuCellButton::fromMap(const QVariantMap &map)
{
    AuCellButton button;
    button.text = map.value(QStringLiteral("text")).toString();
    button.id = map.value(QStringLiteral("id")).toString();
    button.tooltip = map.value(QStringLiteral("tooltip")).toString();
    button.type = map.value(QStringLiteral("type")).toString() == QStringLiteral("link")
                     ? Type::Link : Type::Button;
    button.normal = auStyleFromMap(map.value(QStringLiteral("normal")).toMap());
    button.hover = auStyleFromMap(map.value(QStringLiteral("hover")).toMap());
    button.pressed = auStyleFromMap(map.value(QStringLiteral("pressed")).toMap());
    button.radius = map.value(QStringLiteral("radius"), -1).toInt();
    button.enabled = map.value(QStringLiteral("enabled"), true).toBool();
    button.userData = map.value(QStringLiteral("userData"));
    return button;
}

AuCellButton AuCellButton::primary(const QString &text)
{
    return auPreset(text, QColor(0x16, 0x77, 0xFF));
}

AuCellButton AuCellButton::danger(const QString &text)
{
    return auPreset(text, QColor(0xFF, 0x4D, 0x4F));
}

AuCellButton AuCellButton::success(const QString &text)
{
    return auPreset(text, QColor(0x52, 0xC4, 0x1A));
}

AuCellButton AuCellButton::warning(const QString &text)
{
    return auPreset(text, QColor(0xFA, 0x8C, 0x16));
}

AuCellButton AuCellButton::plain(const QString &text)
{
    AuCellButton button;
    button.text = text;
    return button;
}
