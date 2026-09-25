#include "AgPlotStyle.h"

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

AgPlotStyle *s_globalStyle = nullptr;

} // namespace

AgPlotStyle::AgPlotStyle()
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

AgPlotStyle::~AgPlotStyle()
{
    if (s_globalStyle == this)
        s_globalStyle = nullptr;
}

AgPlotStyle *AgPlotStyle::createLightStyle()
{
    return new AgPlotStyle;
}

AgPlotStyle *AgPlotStyle::createDarkStyle()
{
    AgPlotStyle *style = new AgPlotStyle;
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

AgPlotStyle *AgPlotStyle::globalStyle()
{
    return s_globalStyle;
}

void AgPlotStyle::setGlobalStyle(AgPlotStyle *style)
{
    delete s_globalStyle;
    s_globalStyle = style;
}

// ---- 整体背景 ----

QColor AgPlotStyle::backgroundColor() const
{
    return m_backgroundColor;
}

void AgPlotStyle::setBackgroundColor(const QColor &color)
{
    m_backgroundColor = color;
}

QColor AgPlotStyle::plotAreaBackground() const
{
    return m_plotAreaBackground;
}

void AgPlotStyle::setPlotAreaBackground(const QColor &color)
{
    m_plotAreaBackground = color;
}

// ---- 坐标轴 ----

QColor AgPlotStyle::axisColor() const
{
    return m_axisColor;
}

void AgPlotStyle::setAxisColor(const QColor &color)
{
    m_axisColor = color;
}

QColor AgPlotStyle::tickLabelColor() const
{
    return m_tickLabelColor;
}

void AgPlotStyle::setTickLabelColor(const QColor &color)
{
    m_tickLabelColor = color;
}

QFont AgPlotStyle::tickLabelFont() const
{
    return m_tickLabelFont;
}

void AgPlotStyle::setTickLabelFont(const QFont &font)
{
    m_tickLabelFont = font;
}

int AgPlotStyle::tickLength() const
{
    return m_tickLength;
}

void AgPlotStyle::setTickLength(int length)
{
    m_tickLength = qMax(0, length);
}

int AgPlotStyle::minorTickLength() const
{
    return m_minorTickLength;
}

void AgPlotStyle::setMinorTickLength(int length)
{
    m_minorTickLength = qMax(0, length);
}

bool AgPlotStyle::minorTicksVisible() const
{
    return m_minorTicksVisible;
}

void AgPlotStyle::setMinorTicksVisible(bool on)
{
    m_minorTicksVisible = on;
}

// ---- 背景网格 ----

QColor AgPlotStyle::gridColor() const
{
    return m_gridColor;
}

void AgPlotStyle::setGridColor(const QColor &color)
{
    m_gridColor = color;
}

Qt::PenStyle AgPlotStyle::gridStyle() const
{
    return m_gridStyle;
}

void AgPlotStyle::setGridStyle(Qt::PenStyle style)
{
    m_gridStyle = style;
}

qreal AgPlotStyle::gridWidth() const
{
    return m_gridWidth;
}

void AgPlotStyle::setGridWidth(qreal width)
{
    m_gridWidth = width;
}

// ---- 曲线默认调色板 ----

QVector<QColor> AgPlotStyle::curvePalette() const
{
    return m_curvePalette;
}

void AgPlotStyle::setCurvePalette(const QVector<QColor> &colors)
{
    m_curvePalette = colors;
    m_paletteIndex = 0;
}

QColor AgPlotStyle::nextCurveColor()
{
    if (m_curvePalette.isEmpty())
        return QColor(0x2f, 0x6f, 0xdb);
    const QColor color = m_curvePalette.at(m_paletteIndex % m_curvePalette.size());
    ++m_paletteIndex;
    return color;
}

// ---- 追踪器 tooltip / 十字线 ----

QColor AgPlotStyle::trackerBackgroundColor() const
{
    return m_trackerBackgroundColor;
}

void AgPlotStyle::setTrackerBackgroundColor(const QColor &color)
{
    m_trackerBackgroundColor = color;
}

QColor AgPlotStyle::trackerBorderColor() const
{
    return m_trackerBorderColor;
}

void AgPlotStyle::setTrackerBorderColor(const QColor &color)
{
    m_trackerBorderColor = color;
}

QColor AgPlotStyle::trackerTextColor() const
{
    return m_trackerTextColor;
}

void AgPlotStyle::setTrackerTextColor(const QColor &color)
{
    m_trackerTextColor = color;
}

QFont AgPlotStyle::trackerFont() const
{
    return m_trackerFont;
}

void AgPlotStyle::setTrackerFont(const QFont &font)
{
    m_trackerFont = font;
}

QColor AgPlotStyle::crosshairColor() const
{
    return m_crosshairColor;
}

void AgPlotStyle::setCrosshairColor(const QColor &color)
{
    m_crosshairColor = color;
}

Qt::PenStyle AgPlotStyle::crosshairStyle() const
{
    return m_crosshairStyle;
}

void AgPlotStyle::setCrosshairStyle(Qt::PenStyle style)
{
    m_crosshairStyle = style;
}
