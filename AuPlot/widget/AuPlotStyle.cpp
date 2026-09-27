#include "AuPlotStyle.h"

namespace {

/// 默认曲线调色板（亮色主题）。
QVector<QColor> defaultPalette()
{
    return {
        QColor(0x2f, 0x6f, 0xdb),   // 蓝
        QColor(0xe7, 0x4c, 0x3c),   // 红
        QColor(0x27, 0xae, 0x60),   // 绿
        QColor(0xf3, 0x9c, 0x12),   // 橙
        QColor(0x8e, 0x44, 0xad),   // 紫
        QColor(0x16, 0xa0, 0x85),   // 青绿
        QColor(0xd3, 0x54, 0x00),   // 棕橙
        QColor(0x60, 0x7d, 0x8b)    // 蓝灰
    };
}

AuPlotStyle *s_globalStyle = nullptr;

} // namespace

AuPlotStyle::AuPlotStyle()
    : m_backgroundColor(Qt::white)
    , m_axisColor(0x33, 0x33, 0x33)
    , m_tickLabelColor(0x33, 0x33, 0x33)
    , m_gridColor(0xd9, 0xd9, 0xd9)
    , m_curvePalette(defaultPalette())
    , m_trackerBackgroundColor(255, 255, 255, 235)
    , m_trackerBorderColor(0x33, 0x33, 0x33, 90)
    , m_trackerTextColor(0x33, 0x33, 0x33)
    , m_crosshairColor(0x99, 0x99, 0x99)
{
}

AuPlotStyle::~AuPlotStyle()
{
    if (s_globalStyle == this)
        s_globalStyle = nullptr;
}

AuPlotStyle *AuPlotStyle::createLightStyle()
{
    return new AuPlotStyle;
}

AuPlotStyle *AuPlotStyle::createDarkStyle()
{
    AuPlotStyle *style = new AuPlotStyle;
    style->setBackgroundColor(QColor(0x1e, 0x1f, 0x22));
    style->setAxisColor(QColor(0xa6, 0xa9, 0xae));
    style->setTickLabelColor(QColor(0xd0, 0xd3, 0xd8));
    style->setGridColor(QColor(0x3a, 0x3d, 0x42));
    style->setCurvePalette(defaultPalette());
    style->setTrackerBackgroundColor(QColor(0x2a, 0x2c, 0x30, 235));
    style->setTrackerBorderColor(QColor(0xd0, 0xd3, 0xd8, 70));
    style->setTrackerTextColor(QColor(0xd0, 0xd3, 0xd8));
    style->setCrosshairColor(QColor(0x77, 0x7a, 0x7e));
    return style;
}

AuPlotStyle *AuPlotStyle::globalStyle()
{
    return s_globalStyle;
}

void AuPlotStyle::setGlobalStyle(AuPlotStyle *style)
{
    delete s_globalStyle;
    s_globalStyle = style;
}

// ---- 整体背景 ----

QColor AuPlotStyle::backgroundColor() const
{
    return m_backgroundColor;
}

void AuPlotStyle::setBackgroundColor(const QColor &color)
{
    m_backgroundColor = color;
}

QColor AuPlotStyle::plotAreaBackground() const
{
    return m_plotAreaBackground;
}

void AuPlotStyle::setPlotAreaBackground(const QColor &color)
{
    m_plotAreaBackground = color;
}

// ---- 坐标轴 ----

QColor AuPlotStyle::axisColor() const
{
    return m_axisColor;
}

void AuPlotStyle::setAxisColor(const QColor &color)
{
    m_axisColor = color;
}

QColor AuPlotStyle::tickLabelColor() const
{
    return m_tickLabelColor;
}

void AuPlotStyle::setTickLabelColor(const QColor &color)
{
    m_tickLabelColor = color;
}

QFont AuPlotStyle::tickLabelFont() const
{
    return m_tickLabelFont;
}

void AuPlotStyle::setTickLabelFont(const QFont &font)
{
    m_tickLabelFont = font;
}

int AuPlotStyle::tickLength() const
{
    return m_tickLength;
}

void AuPlotStyle::setTickLength(int length)
{
    m_tickLength = qMax(0, length);
}

int AuPlotStyle::minorTickLength() const
{
    return m_minorTickLength;
}

void AuPlotStyle::setMinorTickLength(int length)
{
    m_minorTickLength = qMax(0, length);
}

bool AuPlotStyle::minorTicksVisible() const
{
    return m_minorTicksVisible;
}

void AuPlotStyle::setMinorTicksVisible(bool on)
{
    m_minorTicksVisible = on;
}

// ---- 背景网格 ----

QColor AuPlotStyle::gridColor() const
{
    return m_gridColor;
}

void AuPlotStyle::setGridColor(const QColor &color)
{
    m_gridColor = color;
}

Qt::PenStyle AuPlotStyle::gridStyle() const
{
    return m_gridStyle;
}

void AuPlotStyle::setGridStyle(Qt::PenStyle style)
{
    m_gridStyle = style;
}

qreal AuPlotStyle::gridWidth() const
{
    return m_gridWidth;
}

void AuPlotStyle::setGridWidth(qreal width)
{
    m_gridWidth = width;
}

// ---- 曲线默认调色板 ----

QVector<QColor> AuPlotStyle::curvePalette() const
{
    return m_curvePalette;
}

void AuPlotStyle::setCurvePalette(const QVector<QColor> &colors)
{
    m_curvePalette = colors;
    m_paletteIndex = 0;
}

QColor AuPlotStyle::nextCurveColor()
{
    if (m_curvePalette.isEmpty())
        return QColor(0x2f, 0x6f, 0xdb);
    const QColor color = m_curvePalette.at(m_paletteIndex % m_curvePalette.size());
    ++m_paletteIndex;
    return color;
}

// ---- 追踪器 tooltip / 十字线 ----

QColor AuPlotStyle::trackerBackgroundColor() const
{
    return m_trackerBackgroundColor;
}

void AuPlotStyle::setTrackerBackgroundColor(const QColor &color)
{
    m_trackerBackgroundColor = color;
}

QColor AuPlotStyle::trackerBorderColor() const
{
    return m_trackerBorderColor;
}

void AuPlotStyle::setTrackerBorderColor(const QColor &color)
{
    m_trackerBorderColor = color;
}

QColor AuPlotStyle::trackerTextColor() const
{
    return m_trackerTextColor;
}

void AuPlotStyle::setTrackerTextColor(const QColor &color)
{
    m_trackerTextColor = color;
}

QFont AuPlotStyle::trackerFont() const
{
    return m_trackerFont;
}

void AuPlotStyle::setTrackerFont(const QFont &font)
{
    m_trackerFont = font;
}

QColor AuPlotStyle::crosshairColor() const
{
    return m_crosshairColor;
}

void AuPlotStyle::setCrosshairColor(const QColor &color)
{
    m_crosshairColor = color;
}

Qt::PenStyle AuPlotStyle::crosshairStyle() const
{
    return m_crosshairStyle;
}

void AuPlotStyle::setCrosshairStyle(Qt::PenStyle style)
{
    m_crosshairStyle = style;
}
