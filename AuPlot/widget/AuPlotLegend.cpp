#include "AuPlotLegend.h"

#include "AuPlot.h"

AuPlotLegend::AuPlotLegend() = default;

bool AuPlotLegend::isVisible() const
{
    return m_visible;
}

void AuPlotLegend::setVisible(bool on)
{
    if (m_visible == on)
        return;
    m_visible = on;
    if (m_plot)
        m_plot->update();
}

AuPlotLegendPos AuPlotLegend::position() const
{
    return m_position;
}

void AuPlotLegend::setPosition(AuPlotLegendPos pos)
{
    if (m_position == pos)
        return;
    m_position = pos;
    if (m_plot)
        m_plot->update();
}

bool AuPlotLegend::clickToToggleEnabled() const
{
    return m_clickToToggle;
}

void AuPlotLegend::setClickToToggleEnabled(bool on)
{
    m_clickToToggle = on;   // 纯行为开关，不改外观，无需重绘
}

int AuPlotLegend::iconSize() const
{
    return m_iconSize;
}

void AuPlotLegend::setIconSize(int size)
{
    size = qMax(6, size);
    if (m_iconSize == size)
        return;
    m_iconSize = size;
    if (m_plot)
        m_plot->update();
}

int AuPlotLegend::maxRows() const
{
    return m_maxRows;
}

void AuPlotLegend::setMaxRows(int rows)
{
    rows = qMax(0, rows);
    if (m_maxRows == rows)
        return;
    m_maxRows = rows;
    if (m_plot)
        m_plot->update();
}
