#include "AuPlotAxis.h"

#include "AuPlot.h"

#include <QtMath>
#include <utility>

namespace {

/// 把 rough 步长向上取整到 "好数"（1 / 2 / 5 x 10^n），让刻度值可读。
double niceStep(double rough)
{
    if (!(rough > 0.0))
        return 1.0;
    const double exponent = std::floor(std::log10(rough));
    const double base = std::pow(10.0, exponent);
    const double fraction = rough / base;   // 1.0 ~ 10.0
    if (fraction < 1.5)
        return 1.0 * base;
    if (fraction < 3.5)
        return 2.0 * base;
    if (fraction < 7.5)
        return 5.0 * base;
    return 10.0 * base;
}

/// 求步长的小数位数（step=1 -> 0 位，2.5 -> 1 位，0.05 -> 2 位，上限 6 位）。
int stepDecimals(double step)
{
    if (!(step > 0.0))
        return 0;
    for (int decimals = 0; decimals <= 6; ++decimals) {
        const double scaled = step * std::pow(10.0, decimals);
        if (std::fabs(scaled - std::round(scaled)) < 1e-9)
            return decimals;
    }
    return 6;
}

bool closeEnough(double a, double b)
{
    return std::fabs(a - b) <= 1e-9 * qMax(1.0, qMax(std::fabs(a), std::fabs(b)));
}

} // namespace

AuPlotAxis::AuPlotAxis(QObject *parent)
    : QObject(parent)
{
}

AuPlotAxis::~AuPlotAxis() = default;

// ---- 可见性 ----

bool AuPlotAxis::isVisible() const
{
    return m_visible;
}

void AuPlotAxis::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

// ---- 数值范围 ----

double AuPlotAxis::minimum() const
{
    return m_min;
}

double AuPlotAxis::maximum() const
{
    return m_max;
}

void AuPlotAxis::setRange(double min, double max)
{
    if (min > max)
        std::swap(min, max);
    m_autoRange = false;
    m_fixedRange = true;
    m_boundMin = min;
    m_boundMax = max;
    setCurrentRange(min, max);
}

bool AuPlotAxis::autoRange() const
{
    return m_autoRange;
}

void AuPlotAxis::setAutoRange(bool on)
{
    if (m_autoRange == on)
        return;
    m_autoRange = on;
    m_fixedRange = false;   // 清除 setRange 的固定边界；setLimits 保留
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

bool AuPlotAxis::hasLimits() const
{
    return m_hasLimits;
}

double AuPlotAxis::limitMinimum() const
{
    return m_limitMin;
}

double AuPlotAxis::limitMaximum() const
{
    return m_limitMax;
}

void AuPlotAxis::setLimits(double min, double max)
{
    if (min > max)
        std::swap(min, max);
    m_hasLimits = true;
    m_limitMin = min;
    m_limitMax = max;
    // 已有范围越界时立即夹回
    if (m_min < m_limitMin || m_max > m_limitMax) {
        clampToBounds(m_min, m_max);
        setCurrentRange(m_min, m_max);
    }
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

void AuPlotAxis::clearLimits()
{
    m_hasLimits = false;
}

qreal AuPlotAxis::rangePadding() const
{
    return m_padding;
}

void AuPlotAxis::setRangePadding(qreal padding)
{
    m_padding = qBound<qreal>(0.0, padding, 0.45);
}

// ---- 刻度 ----

int AuPlotAxis::tickCount() const
{
    return m_tickCount;
}

void AuPlotAxis::setTickCount(int count)
{
    if (m_tickCount == count)
        return;
    m_tickCount = count;
    m_rangeDirty = true;
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

int AuPlotAxis::minorTickCount() const
{
    return m_minorCount;
}

void AuPlotAxis::setMinorTickCount(int count)
{
    count = qBound(0, count, 50);
    if (m_minorCount == count)
        return;
    m_minorCount = count;
    m_rangeDirty = true;
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

bool AuPlotAxis::tickLabelsVisible() const
{
    return m_tickLabelsVisible;
}

void AuPlotAxis::setTickLabelsVisible(bool on)
{
    if (m_tickLabelsVisible == on)
        return;
    m_tickLabelsVisible = on;
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

void AuPlotAxis::setLabelFormatter(std::function<QString(double)> formatter)
{
    m_formatter = std::move(formatter);
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

bool AuPlotAxis::hasLabelFormatter() const
{
    return static_cast<bool>(m_formatter);
}

QString AuPlotAxis::formatTickLabel(double value) const
{
    if (m_formatter)
        return m_formatter(value);

    int decimals = m_labelPrecision;
    if (decimals < 0)
        decimals = stepDecimals(m_lastStep);
    return QString::number(value, 'f', decimals);
}

int AuPlotAxis::labelPrecision() const
{
    return m_labelPrecision;
}

void AuPlotAxis::setLabelPrecision(int precision)
{
    if (m_labelPrecision == precision)
        return;
    m_labelPrecision = precision;
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

// ---- 背景网格 ----

bool AuPlotAxis::gridVisible() const
{
    return m_gridVisible;
}

void AuPlotAxis::setGridVisible(bool on)
{
    if (m_gridVisible == on)
        return;
    m_gridVisible = on;
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

Qt::PenStyle AuPlotAxis::gridStyle() const
{
    return m_gridStyle;
}

void AuPlotAxis::setGridStyle(Qt::PenStyle style)
{
    if (m_gridStyle == style)
        return;
    m_gridStyle = style;
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

QColor AuPlotAxis::gridColor() const
{
    return m_gridColor;
}

void AuPlotAxis::setGridColor(const QColor &color)
{
    if (m_gridColor == color)
        return;
    m_gridColor = color;
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

qreal AuPlotAxis::gridWidth() const
{
    return m_gridWidth;
}

void AuPlotAxis::setGridWidth(qreal width)
{
    if (qFuzzyCompare(m_gridWidth, width))
        return;
    m_gridWidth = width;
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

bool AuPlotAxis::minorGridVisible() const
{
    return m_minorGridVisible;
}

void AuPlotAxis::setMinorGridVisible(bool on)
{
    if (m_minorGridVisible == on)
        return;
    m_minorGridVisible = on;
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

// ---- 刻度取值 ----

QVector<double> AuPlotAxis::tickValues(qreal pixelLength) const
{
    generateTicks(pixelLength);
    return m_majorTicks;
}

QVector<double> AuPlotAxis::minorTickValues(qreal pixelLength) const
{
    generateTicks(pixelLength);
    return m_minorTicks;
}

QColor AuPlotAxis::axisColor() const
{
    return m_axisColor;
}

void AuPlotAxis::setAxisColor(const QColor &color)
{
    if (m_axisColor == color)
        return;
    m_axisColor = color;
    if (AuPlot *plot = qobject_cast<AuPlot *>(parent()))
        plot->update();
}

// ---- 内部 ----

void AuPlotAxis::setCurrentRange(double min, double max)
{
    if (min > max)
        std::swap(min, max);
    if (closeEnough(m_min, min) && closeEnough(m_max, max))
        return;
    m_min = min;
    m_max = max;
    m_rangeDirty = true;
    emit rangeChanged(m_min, m_max);
}

bool AuPlotAxis::clampToBounds(double &mn, double &mx) const
{
    double boundMin = 0.0;
    double boundMax = 0.0;
    if (!hasInteractionBounds(boundMin, boundMax))
        return false;

    const double span = mx - mn;
    const double boundSpan = boundMax - boundMin;
    if (span >= boundSpan) {
        mn = boundMin;
        mx = boundMax;
        return true;
    }
    if (mn < boundMin) {
        mn = boundMin;
        mx = boundMin + span;
    }
    if (mx > boundMax) {
        mx = boundMax;
        mn = boundMax - span;
    }
    return true;
}

bool AuPlotAxis::hasInteractionBounds(double &boundMin, double &boundMax) const
{
    if (m_hasLimits) {
        boundMin = m_limitMin;
        boundMax = m_limitMax;
        return true;
    }
    if (m_fixedRange) {
        boundMin = m_boundMin;
        boundMax = m_boundMax;
        return true;
    }
    return false;
}

void AuPlotAxis::generateTicks(qreal pixelLength) const
{
    if (!m_rangeDirty && closeEnough(pixelLength, m_cachedPixelLength))
        return;
    m_cachedPixelLength = pixelLength;
    m_rangeDirty = false;

    m_majorTicks.clear();
    m_minorTicks.clear();

    const double span = m_max - m_min;
    if (!(span > 0.0) || !(pixelLength > 0.0)) {
        m_majorTicks << m_min << m_max;
        return;
    }

    double step = 0.0;
    if (m_tickCount >= 2) {
        // 手动固定数量：首尾各一个，范围内均分
        step = span / (m_tickCount - 1);
        for (int i = 0; i < m_tickCount; ++i)
            m_majorTicks << m_min + i * step;
    } else {
        // 自动：按像素长度推目标数量（约 70px 一个大刻度），步长取好数
        const int targetCount = qMax(2, int(pixelLength / 70.0));
        step = niceStep(span / targetCount);
        const double first = std::ceil(m_min / step - 1e-9) * step;
        for (double value = first; value <= m_max + step * 1e-6; value += step) {
            if (closeEnough(value, 0.0))
                value = 0.0;   // 消除 -0.0 与浮点累积误差
            m_majorTicks << value;
        }
        // 范围太窄导致不足两个刻度时，退化为首尾两个
        if (m_majorTicks.size() < 2)
            m_majorTicks = { m_min, m_max };
    }

    m_lastStep = step;

    // 小刻度：相邻大刻度之间均分 m_minorCount 个（不含大刻度本身）
    const int minorCount = m_minorCount;
    if (minorCount <= 0 || m_majorTicks.size() < 2)
        return;
    const double minorStep = step / (minorCount + 1);
    // 首个大刻度之前（到范围下限）与最后一个大刻度之后（到范围上限）也补齐
    const double firstMajor = m_majorTicks.first();
    const double lastMajor = m_majorTicks.last();
    for (double value = firstMajor - minorStep; value >= m_min + minorStep * 1e-6; value -= minorStep)
        m_minorTicks.prepend(value);
    for (int i = 0; i + 1 < m_majorTicks.size(); ++i) {
        const double from = m_majorTicks.at(i);
        for (int j = 1; j <= minorCount; ++j)
            m_minorTicks << from + j * minorStep;
    }
    for (double value = lastMajor + minorStep; value <= m_max - minorStep * 1e-6; value += minorStep)
        m_minorTicks << value;
}
