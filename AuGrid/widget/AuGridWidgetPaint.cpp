// ============================================================================
// AuGridWidget 绘制实现：paintEvent 编排与全部绘制函数
// （背景 / 多级表头 / 行背景与内容 / 网格线 / 焦点框 / 区域分隔线 /
//   排序箭头 / 复选框图元 / 行号与拖拽手柄格 / 加载遮罩）。
// ============================================================================

#include "AuGridWidget.h"
#include "AuScrollBar.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>

void AuGridWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const int radius = cornerRadius();
    const QRect panel = contentPanelRect();

    // 1. 先填充整个 widget 背景（滚动条区域也会用到）
    drawBackground(&painter);

    // 2. 绘制圆角表格面板背景（需要反锯齿）
    if (radius > 0) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(panelBackgroundColor());
        painter.drawRoundedRect(panel, radius, radius);
        painter.restore();
    }

    // 3. 在面板区域内裁剪并绘制表格内容
    painter.save();
    if (radius > 0) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        // 内容裁剪区对齐边框内沿：边框占最外圈 1px（外径 radius、内径 radius-1），
        // 与网页 border-radius 的内圆角一致，避免表头/单元格边框超出圆角外边框。
        QPainterPath contentClip;
        const qreal innerRadius = qMax(0, radius - 1);
        contentClip.addRoundedRect(QRectF(panel).adjusted(1, 1, -1, -1), innerRadius, innerRadius);
        painter.setClipPath(contentClip, Qt::ReplaceClip);
    } else {
        painter.setClipRect(panel, Qt::ReplaceClip);
    }

    // 表格内部线条关闭反锯齿，保持 1px 清晰
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QPair<int, int> bodyColRange = visibleColumnRange();
    QPair<int, int> rowRange = visibleRowRange();
    // 排序 / 过滤位移动画：退场行的新可见索引在视口之外，把绘制范围扩展到这些行，
    // 使它们能按插值位置画出“移出视口”的过程（进场行本就在可视范围内）
    if (!m_dataAnimationEntries.isEmpty() && m_model) {
        for (const DataMoveAnimationEntry &entry : m_dataAnimationEntries) {
            rowRange.first = qMin(rowRange.first, entry.visibleRow);
            rowRange.second = qMax(rowRange.second, entry.visibleRow);
        }
        rowRange.first = qMax(0, rowRange.first);
        rowRange.second = qMin(m_model->rowCount() - 1, rowRange.second);
    }

    drawHeader(&painter);

    // 一个区域的行内容（背景之外的单元格 + 网格线）
    auto drawRegionRows = [&](int firstColumn, int lastColumn) {
        if (firstColumn > lastColumn)
            return;
        drawRows(&painter, rowRange.first, rowRange.second, firstColumn, lastColumn);
        drawGridLines(&painter, firstColumn, lastColumn, rowRange.first, rowRange.second);
    };
    auto drawPinnedRegions = [&]() {
        if (!m_leftPinnedColumns.isEmpty())
            drawRegionRows(m_leftPinnedColumns.first(), m_leftPinnedColumns.last());
        if (!m_rightPinnedColumns.isEmpty())
            drawRegionRows(m_rightPinnedColumns.first(), m_rightPinnedColumns.last());
    };

    // 树形展开 / 折叠动画期间分三层绘制：
    //   静态行 → 渐变子行（半透明）→ 平移兄弟行（不透明顶层），
    // 平移行盖在渐变子行之上，避免移动中单元格的边框 / 内容与子行互相穿透。
    const bool treeAnimActive = !m_treeAnimationEntries.isEmpty()
                              || !m_treeAnimationFadeRows.isEmpty();

    if (!treeAnimActive) {
        drawRowBackgrounds(&painter, rowRange.first, rowRange.second);
        drawRegionRows(bodyColRange.first, bodyColRange.second);
        drawPinnedRegions();
    } else {
        const int layers[3] = {3, 1, 2};   // 静态行 → 渐变子行 → 平移行
        for (const int layer : layers) {
            m_treeLayerFilter = layer;
            drawRowBackgrounds(&painter, rowRange.first, rowRange.second);
            drawRegionRows(bodyColRange.first, bodyColRange.second);
            drawPinnedRegions();
        }
        m_treeLayerFilter = 0;
    }

    // 绘制行号列单元格
    if (m_showRowNumberColumn) {
        const qreal columnFade = specialColumnOpacity(SpecialRowNumber);
        painter.save();
        painter.setClipRect(0, headerHeight(), rowNumberColumnWidth(), bodyHeight(), Qt::IntersectClip);
        for (int row = rowRange.first; row <= rowRange.second; ++row) {
            const qreal rowOpacity = rowAnimationOpacity(row) * columnFade;
            if (rowOpacity < 1.0) {
                painter.save();
                painter.setOpacity(rowOpacity);
            }
            drawRowNumberCell(&painter, row);
            if (rowOpacity < 1.0)
                painter.restore();
        }
        painter.restore();

        const QColor gridLineColor = m_style->color(AuGridStyle::GridLineColor, m_model);
        if (gridLineColor.isValid()) {
            painter.save();
            const int rnW = rowNumberColumnWidth();
            // 裁剪含右边界像素：drawRect 的右边线落在 x = rnW 处，
            // 数据列竖线隐藏时该边线由本循环独立绘制
            painter.setClipRect(0, headerHeight(), rnW + 1, bodyHeight(), Qt::IntersectClip);
            painter.setPen(gridLineColor);
            // 与 drawGridLines 一致：重设动画期间末行底边线不绘制（见 drawGridLines 内注释）
            const bool suppressLastRowBottom = m_dataResetAnimating || m_suppressBottomLineForCapture;
            const int lastDataRow = m_model->rowCount() - 1;
            for (int row = rowRange.first; row <= rowRange.second; ++row) {
                const int y = rowY(row);
                const int h = rowHeight(row);
                const qreal rowOpacity = rowAnimationOpacity(row) * columnFade;
                if (rowOpacity < 1.0) {
                    painter.save();
                    painter.setOpacity(rowOpacity);
                }
                // 行号列始终画完整边框，不受列边框开关影响；动画期间末行只画上 / 左 / 右
                if (suppressLastRowBottom && row == lastDataRow) {
                    painter.drawLine(0, y, rnW, y);
                    painter.drawLine(0, y, 0, y + h);
                    painter.drawLine(rnW, y, rnW, y + h);
                } else {
                    painter.drawRect(0, y, rnW, h);
                }
                if (rowOpacity < 1.0)
                    painter.restore();
            }
            painter.restore();
        }
    }

    // 绘制复选框列 + 行拖拽手柄列（合并为一个固定列单元格，中间不再画分隔边框）
    if (m_showCheckboxColumn || m_rowDragEnabled) {
        const int fixedX = rowNumberColumnWidth();
        const int fixedW = checkboxColumnWidth() + dragHandleColumnWidth();
        const qreal checkboxFade = specialColumnOpacity(SpecialCheckbox);
        const qreal dragFade = specialColumnOpacity(SpecialDragHandle);

        painter.save();
        painter.setClipRect(fixedX, headerHeight(), fixedW, bodyHeight(), Qt::IntersectClip);
        for (int row = rowRange.first; row <= rowRange.second; ++row) {
            const qreal rowOpacity = rowAnimationOpacity(row);
            if (m_showCheckboxColumn) {
                const qreal opacity = rowOpacity * checkboxFade;
                if (opacity < 1.0) {
                    painter.save();
                    painter.setOpacity(opacity);
                }
                drawCheckboxCell(&painter, row);
                if (opacity < 1.0)
                    painter.restore();
            }
            if (m_rowDragEnabled) {
                const qreal opacity = rowOpacity * dragFade;
                if (opacity < 1.0) {
                    painter.save();
                    painter.setOpacity(opacity);
                }
                drawDragHandleCell(&painter, row);
                if (opacity < 1.0)
                    painter.restore();
            }
        }
        painter.restore();

        // 合并列的边框：仅四周画一次，复选框与拖拽手柄之间不再有分隔线
        const QColor gridLineColor = m_style->color(AuGridStyle::GridLineColor, m_model);
        if (gridLineColor.isValid()) {
            painter.save();
            // 裁剪含右边界像素（同行号列：保证右边线在数据列竖线隐藏时仍由本循环绘制）
            painter.setClipRect(fixedX, headerHeight(), fixedW + 1, bodyHeight(), Qt::IntersectClip);
            painter.setPen(gridLineColor);
            // 与 drawGridLines 一致：重设动画期间末行底边线不绘制（见 drawGridLines 内注释）
            const bool suppressLastRowBottom = m_dataResetAnimating || m_suppressBottomLineForCapture;
            const int lastDataRow = m_model->rowCount() - 1;
            for (int row = rowRange.first; row <= rowRange.second; ++row) {
                const int y = rowY(row);
                const int h = rowHeight(row);
                const qreal rowOpacity = rowAnimationOpacity(row);
                if (rowOpacity < 1.0) {
                    painter.save();
                    painter.setOpacity(rowOpacity);
                }
                // 复选框 / 拖拽手柄列始终画完整边框，不受列边框开关影响；
                // 动画期间末行只画上 / 左 / 右
                if (suppressLastRowBottom && row == lastDataRow) {
                    painter.drawLine(fixedX, y, fixedX + fixedW, y);
                    painter.drawLine(fixedX, y, fixedX, y + h);
                    painter.drawLine(fixedX + fixedW, y, fixedX + fixedW, y + h);
                } else {
                    painter.drawRect(fixedX, y, fixedW, h);
                }
                if (rowOpacity < 1.0)
                    painter.restore();
            }
            painter.restore();
        }
    }

    // 绘制被拖拽行的半透明覆盖层，使拖动行与未拖动行区分
    if (m_isRowDragging) {
        painter.save();
        painter.setPen(Qt::NoPen);
        const QColor overlayColor = m_style->color(AuGridStyle::DraggedRowOverlayColor, m_model);
        painter.setBrush(overlayColor.isValid() ? overlayColor : QColor(255, 255, 255, 102));
        const int viewportW = viewportWidthForColumns();
        for (int row = rowRange.first; row <= rowRange.second; ++row) {
            if (isRowDragging(row)) {
                const int y = rowY(row);
                const int h = rowHeight(row);
                painter.drawRect(0, y, viewportW, h);
            }
        }
        painter.restore();
    }

    drawCellFocus(&painter);
    drawRegionSeparators(&painter);

    // 数据行未填满可视区时，在最后一条数据行底部画一条贯穿线，避免底部看起来缺边框。
    // 重设数据切换动画期间（及为其捕获快照时）不画：内容整体平移时，这条静止的
    // 贯穿线与内容自身的底部边框分离，会在空视口中单独晃动；动画结束后再补画。
    if (m_model && m_model->rowCount() > 0 && !m_vScrollBarVisible
        && !m_dataResetAnimating && !m_suppressBottomLineForCapture) {
        const QColor gridLineColor = m_style->color(AuGridStyle::GridLineColor, m_model);
        if (gridLineColor.isValid()) {
            const int contentBottom = headerHeight() + totalContentHeight() - m_yOffset;
            painter.save();
            painter.setPen(gridLineColor);
            painter.drawLine(0, contentBottom, viewportWidthForColumns(), contentBottom);
            painter.restore();
        }
    }

    // 重设数据切换动画（两段式）：前半程旧内容快照匀速整体上移离场（视口渐空），
    // 后半程快照已完全离场，新内容自视口上方下移涌入（见 rowY 的偏移逻辑）
    if (m_dataResetAnimating && !m_dataResetSnapshot.isNull()) {
        const qreal qe = (m_dataResetProgress < 0.5)
                             ? m_dataResetProgress * 2.0   // 离场匀速，全程可见
                             : 1.0;                        // 后半程快照已完全移出视口
        const int y = headerHeight() - qRound(m_dataResetSlideHeight * qe);
        if (y + m_dataResetSnapshot.height() > headerHeight()) {
            painter.save();
            painter.setClipRect(0, headerHeight(), width(), bodyHeight());
            painter.drawImage(QPoint(0, y), m_dataResetSnapshot);
            painter.restore();
        }
    }

    // 行删除消失动画：被删行的像素幽灵带原位淡出并轻微上漂，
    // 下方行经排序位移动画上滑补位（与排序动画共用进度与缓动，绘制在其之上）
    if (!m_removedRowGhosts.isEmpty()) {
        const qreal ghostOpacity = qBound(0.0, 1.0 - m_dataAnimationProgress, 1.0);
        if (ghostOpacity > 0.0) {
            painter.save();
            painter.setClipRect(0, headerHeight(), width(), bodyHeight(), Qt::IntersectClip);
            painter.setOpacity(ghostOpacity);
            for (const RemovedRowGhost &ghost : m_removedRowGhosts) {
                const int drift = qRound(ghost.height * 0.3 * m_dataAnimationProgress);
                painter.drawImage(QPoint(0, ghost.y - drift), ghost.image);
            }
            painter.restore();
        }
    }

    painter.restore();

    // 4. 绘制面板边框：用“外轮廓 - 内轮廓”的环形填充代替画笔描边。
    // 描边在开反锯齿时会把 1px 线摊到两侧（显得发虚、圆角处偏粗），
    // 关闭反锯齿则会让圆角变成锯齿台阶；环形填充的直边恰好落在整数
    // 像素上（实色 1px），圆角由反锯齿平滑过渡，观感与网页 border-radius 一致。
    const QColor borderColor = m_style->color(AuGridStyle::BorderColor, m_model);
    if (borderColor.isValid()) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(borderColor);
        const QRectF innerPanel = QRectF(panel).adjusted(1, 1, -1, -1);
        QPainterPath borderPath;
        borderPath.setFillRule(Qt::OddEvenFill);
        if (radius > 0) {
            borderPath.addRoundedRect(QRectF(panel), radius, radius);
            borderPath.addRoundedRect(innerPanel, radius - 1, radius - 1);
        } else {
            borderPath.addRect(QRectF(panel));
            borderPath.addRect(innerPanel);
        }
        painter.drawPath(borderPath);
        painter.restore();
    }

    // 5. 加载遮罩：最后绘制，盖住表格内容（编辑器由 setLoading 关闭；
    //    滚动条是子控件盖不住，随蒙层反向淡变，见 applyLoadingFadeToScrollBars）
    if (m_loadingOpacity > 0.0)
        drawLoadingOverlay(&painter);
}

void AuGridWidget::drawLoadingOverlay(QPainter *painter)
{
    if (m_loadingOpacity <= 0.0 || !m_style)
        return;

    const QRect panel = contentPanelRect();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setClipPath(roundedClipPath(), Qt::ReplaceClip);
    painter->setOpacity(m_loadingOpacity);

    // 半透明蒙层（覆盖表头与全部内容区）
    const QColor overlayColor = m_style->color(AuGridStyle::LoadingOverlayColor, m_model);
    if (overlayColor.isValid()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(overlayColor);
        painter->drawRect(panel);
    }

    // 圆环 + 文案整体在面板内居中
    QString text = m_loadingText;
    if (text.isNull())
        text = m_style->value(AuGridStyle::LoadingText, m_model).toString();

    const QColor spinnerColor = m_style->color(AuGridStyle::LoadingSpinnerColor, m_model);
    const QColor textColor = m_style->color(AuGridStyle::LoadingTextColor, m_model);
    const int spinnerSize = qMax(12, m_style->integer(AuGridStyle::LoadingSpinnerSize, m_model));
    const QFont textFont = m_style->font(m_model);
    const QFontMetrics fm(textFont);
    const int textGap = 12;

    qreal blockHeight = spinnerSize;
    if (!text.isEmpty())
        blockHeight += textGap + fm.height();
    const qreal blockTop = panel.center().y() - blockHeight / 2.0;

    if (spinnerColor.isValid()) {
        const qreal penWidth = qMax(2.0, spinnerSize / 7.0);
        QRectF spinnerRect(panel.center().x() - spinnerSize / 2.0, blockTop, spinnerSize, spinnerSize);
        // 内缩半个线宽，避免圆弧溢出圆环外接矩形
        spinnerRect.adjust(penWidth / 2.0, penWidth / 2.0, -penWidth / 2.0, -penWidth / 2.0);

        QPen pen(spinnerColor);
        pen.setWidthF(penWidth);
        pen.setCapStyle(Qt::RoundCap);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawArc(spinnerRect, m_loadingSpinnerAngle * 16, 100 * 16);
    }

    if (!text.isEmpty()) {
        painter->setFont(textFont);
        painter->setPen(textColor.isValid() ? textColor : QColor(80, 80, 80));
        const QRectF textRect(panel.left(), blockTop + spinnerSize + textGap,
                              panel.width(), fm.height());
        painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, text);
    }

    painter->restore();
}

QSharedPointer<AuCellRenderer> AuGridWidget::rendererForColumn(int column) const
{
    QSharedPointer<AuCellRenderer> renderer = m_defaultCellRenderer;
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return renderer;

    const AuColumnDef def = m_model->columnDefAt(column);
    if (def.isCheckbox())
        return m_checkboxCellRenderer;

    const QString rendererName = def.cellRenderer();
    if (!rendererName.isEmpty()) {
        auto it = m_cellRenderers.find(rendererName);
        if (it != m_cellRenderers.end())
            renderer = it.value();
    }
    return renderer;
}

QSharedPointer<AuHeaderRenderer> AuGridWidget::headerRendererForColumn(int column) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return QSharedPointer<AuHeaderRenderer>();
    const QString name = m_model->columnDefAt(column).headerRenderer();
    if (name.isEmpty())
        return QSharedPointer<AuHeaderRenderer>();
    return m_headerRenderers.value(name);
}

void AuGridWidget::drawRowNumberCell(QPainter *painter, int row)
{
    const QRect rect = rowNumberCellRect(row);
    if (rect.isEmpty() || !m_model)
        return;

    if (row < 0 || row >= m_model->rowCount())
        return;

    QFont font = m_style->font(m_model);
    if (m_style->value(AuGridStyle::RowNumberFontBold, m_model).toBool())
        font.setBold(true);

    painter->save();
    painter->setFont(font);
    const QColor fgColor = m_style->color(AuGridStyle::RowNumberForegroundColor, m_model);
    painter->setPen(fgColor.isValid() ? fgColor : palette().text().color());
    const int padding = specialColumnPadding();
    const QRect textRect = rect.adjusted(padding, 0, -padding, 0);
    // 行号 = 可见行号 + 1：排序 / 过滤 / 拖拽重排后跟随可见顺序重新编号
    painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, QString::number(row + 1));
    painter->restore();
}

void AuGridWidget::drawDragHandleCell(QPainter *painter, int row)
{
    const QRect rect = dragHandleCellRect(row);
    if (rect.isEmpty())
        return;

    const bool hovered = (m_hoverRow == row && m_hoverColumn == kDragHandleColumn);
    const QColor bgColor = hovered
        ? m_style->color(AuGridStyle::DragHandleHoveredBackgroundColor, m_model)
        : m_style->color(AuGridStyle::DragHandleBackgroundColor, m_model);
    if (bgColor.isValid())
        painter->fillRect(rect, bgColor);

    const int dotSize = m_style->integer(AuGridStyle::DragHandleDotSize, m_model);
    const QColor dotColor = hovered
        ? m_style->color(AuGridStyle::DragHandleHoveredIconColor, m_model)
        : m_style->color(AuGridStyle::DragHandleIconColor, m_model);
    if (!dotColor.isValid() || dotSize <= 0)
        return;

    constexpr int cols = 3;
    constexpr int rows = 2;
    const int spacing = dotSize;
    const int totalH = rows * dotSize + (rows - 1) * spacing;
    const int padding = specialColumnPadding();
    const int leftPadding = m_showCheckboxColumn ? padding / 2 : padding;
    const int startX = rect.left() + leftPadding;
    const int startY = rect.top() + (rect.height() - totalH) / 2;

    painter->setBrush(dotColor);
    painter->setPen(Qt::NoPen);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const int x = startX + c * (dotSize + spacing);
            const int y = startY + r * (dotSize + spacing);
            painter->drawEllipse(x, y, dotSize, dotSize);
        }
    }
}

void AuGridWidget::drawBackground(QPainter *painter)
{
    // 非圆角模式下，手动填充内容区背景
    if (cornerRadius() == 0)
        painter->fillRect(contentPanelRect(), panelBackgroundColor());
}

QColor AuGridWidget::panelBackgroundColor() const
{
    QColor color = m_style->color(AuGridStyle::RowBackgroundColor, m_model);
    if (!color.isValid())
        color = palette().base().color();
    return color;
}

void AuGridWidget::drawCheckbox(QPainter *painter, const QRect &box, bool checked, bool partial, bool enabled) const
{
    QColor checkedColor = m_style->color(AuGridStyle::CheckboxCheckedColor, m_model);
    QColor uncheckedColor = m_style->color(AuGridStyle::CheckboxUncheckedColor, m_model);
    QColor borderColor = m_style->color(AuGridStyle::CheckboxBorderColor, m_model);
    QColor checkmarkColor = m_style->color(AuGridStyle::CheckboxCheckmarkColor, m_model);

    if (!enabled) {
        checkedColor = QColor(200, 200, 200);
        uncheckedColor = QColor(230, 230, 230);
        borderColor = QColor(180, 180, 180);
        checkmarkColor = QColor(120, 120, 120);
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const int radius = 3;
    if (checked || partial) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(checkedColor.isValid() ? checkedColor : QColor(0, 120, 215));
        painter->drawRoundedRect(box, radius, radius);

        painter->setPen(QPen(checkmarkColor.isValid() ? checkmarkColor : Qt::white, 2));
        if (partial) {
            const int lineY = box.center().y();
            painter->drawLine(box.left() + 4, lineY, box.right() - 4, lineY);
        } else {
            QPainterPath path;
            path.moveTo(box.left() + 4, box.center().y());
            path.lineTo(box.center().x() - 1, box.bottom() - 4);
            path.lineTo(box.right() - 4, box.top() + 4);
            painter->drawPath(path);
        }
    } else {
        if (uncheckedColor.isValid()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(uncheckedColor);
            painter->drawRoundedRect(box, radius, radius);
        }
        if (borderColor.isValid()) {
            painter->setPen(QPen(borderColor, 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(box.adjusted(1, 1, -1, -1), radius, radius);
        }
    }

    painter->restore();
}

void AuGridWidget::drawCheckboxHeader(QPainter *painter)
{
    const QRect rect = checkboxHeaderRect();
    if (!rect.isValid())
        return;

    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_style->color(AuGridStyle::HeaderBackgroundColor, m_model));
    painter->drawRect(rect);
    painter->restore();

    const int size = m_style->integer(AuGridStyle::CheckboxSize, m_model);
    const int padding = specialColumnPadding();
    const int x = rect.left() + padding;
    const int y = rect.top() + (rect.height() - size) / 2;

    // 树形模式：表头三态按原始行全集聚合（折叠不可见的行也计入；
    // 聚合勾选可能包含不可勾选的祖先行，统计时需按可勾选行过滤）
    int checkableCount;
    int checkedCount;
    if (m_model && m_model->isTreeMode()) {
        checkableCount = checkableOriginalRowCount();
        checkedCount = checkedCheckableOriginalCount();
    } else {
        checkableCount = checkableRowCount();
        checkedCount = checkedRows().size();
    }
    const bool allChecked = checkableCount > 0 && checkedCount == checkableCount;
    const bool partial = !allChecked && checkedCount > 0;
    drawCheckbox(painter, QRect(x, y, size, size), allChecked, partial);
}

void AuGridWidget::drawCheckboxCell(QPainter *painter, int row)
{
    const QRect rect = checkboxCellRect(row);
    if (!rect.isValid())
        return;

    const int size = m_style->integer(AuGridStyle::CheckboxSize, m_model);
    const int padding = specialColumnPadding();
    const int x = rect.left() + padding;
    const int y = rect.top() + (rect.height() - size) / 2;

    const bool enabled = isRowCheckable(row);
    drawCheckbox(painter, QRect(x, y, size, size), isRowChecked(row), isRowPartiallyChecked(row), enabled);
}

void AuGridWidget::drawSortIndicator(QPainter *painter, const QRect &rect, Qt::SortOrder order) const
{
    const qreal size = 8;
    const qreal x = rect.right() - size - 6;
    const qreal y = rect.top() + (rect.height() - size) / 2.0;

    // 浮点坐标 + 反锯齿：底边落在整数像素行保持锐利，两条斜边平滑无锯齿
    QPolygonF triangle;
    if (order == Qt::AscendingOrder) {
        triangle << QPointF(x, y + size)
                 << QPointF(x + size, y + size)
                 << QPointF(x + size / 2.0, y);
    } else {
        triangle << QPointF(x, y)
                 << QPointF(x + size, y)
                 << QPointF(x + size / 2.0, y + size);
    }

    QColor color = m_style->color(AuGridStyle::SortIndicatorColor, m_model);
    if (!color.isValid())
        color = m_style->color(AuGridStyle::HeaderForegroundColor, m_model);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawPolygon(triangle);
    painter->restore();
}

void AuGridWidget::drawHeader(QPainter *painter)
{
    if (!m_model || m_model->topLevelColumnDefs().isEmpty())
        return;

    const int viewportW = viewportWidthForColumns();
    const int panelW = contentPanelRect().width();
    const int vBarWidth = m_vScrollBarVisible ? m_style->integer(AuGridStyle::ScrollBarExtent, m_model) : 0;

    // 先填充整个表头底色（包含滚动条占位列）
    painter->save();
    painter->setClipRect(0, 0, panelW, headerHeight(), Qt::IntersectClip);
    painter->fillRect(0, 0, panelW, headerHeight(),
                     m_style->color(AuGridStyle::HeaderBackgroundColor, m_model));
    painter->restore();

    const int fixedWidth = fixedColumnsWidth();

    // body 区表头：裁剪掉复选框列与拖拽手柄列区域，避免横向滚动时覆盖
    painter->save();
    painter->setClipRect(fixedWidth, 0, viewportW - fixedWidth, headerHeight(), Qt::IntersectClip);
    int leafIndex = 0;
    drawHeaderRecursive(painter, m_model->topLevelColumnDefs(), 0, leafIndex, ColumnRegion::Body);
    painter->restore();

    // 左侧停靠列表头
    if (!m_leftPinnedColumns.isEmpty()) {
        painter->save();
        painter->setClipRect(0, 0, viewportW, headerHeight(), Qt::IntersectClip);
        leafIndex = 0;
        drawHeaderRecursive(painter, m_model->topLevelColumnDefs(), 0, leafIndex, ColumnRegion::LeftPinned);
        painter->restore();
    }

    // 右侧停靠列表头
    if (!m_rightPinnedColumns.isEmpty()) {
        painter->save();
        painter->setClipRect(0, 0, viewportW, headerHeight(), Qt::IntersectClip);
        leafIndex = 0;
        drawHeaderRecursive(painter, m_model->topLevelColumnDefs(), 0, leafIndex, ColumnRegion::RightPinned);
        painter->restore();
    }

    // 复选框列表头：最后绘制，使其覆盖可能滚动过来的 body 列头
    if (m_showCheckboxColumn)
        drawCheckboxHeader(painter);

    // 右侧滚动条占位列：与滚动条等宽的无内容表头
    if (vBarWidth > 0) {
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_style->color(AuGridStyle::HeaderBackgroundColor, m_model));
        painter->drawRect(viewportW, 0, vBarWidth, headerHeight());
        painter->restore();
    }

    // 在表头最底部绘制一条贯穿的边框线，从表格左边缘到右边缘，
    // 覆盖复选框列和滚动条占位列（这些列没有叶子列头绘制底边）。
    // 与叶子列头“独占”底边对齐（y = headerHeight），与 body 首行顶边重叠仍为 1px。
    const QColor headerBorderColor = m_style->color(AuGridStyle::HeaderBorderColor, m_model);
    if (headerBorderColor.isValid()) {
        painter->save();
        painter->setPen(headerBorderColor);
        const int bottomY = headerHeight();
        painter->drawLine(0, bottomY, panelW, bottomY);
        painter->restore();
    }
}

void AuGridWidget::drawHeaderRecursive(QPainter *painter, const QVector<AuColumnDef> &defs, int depth, int &leafIndex, ColumnRegion region)
{
    for (const AuColumnDef &def : defs) {
        const int leafStart = leafIndex;

        if (def.isGroup()) {
            drawHeaderRecursive(painter, def.children(), depth + 1, leafIndex, region);
            // 分组内所有叶子都被隐藏时，该分组表头不再绘制
            bool anyVisible = false;
            for (int i = leafStart; i < leafIndex; ++i) {
                if (!isColumnHidden(i)) {
                    anyVisible = true;
                    break;
                }
            }
            if (anyVisible)
                drawHeaderNode(painter, def, depth, leafStart, leafIndex, region);
        } else {
            leafIndex++;
            if (!isColumnHidden(leafStart))
                drawHeaderNode(painter, def, depth, leafStart, leafIndex, region);
        }
    }
}

void AuGridWidget::drawHeaderNode(QPainter *painter, const AuColumnDef &def, int depth, int leafStart, int leafEnd, ColumnRegion region)
{
    const int levelHeight = headerLevelHeight();

    int firstInRegion = -1;
    int lastInRegion = -1;
    for (int i = leafStart; i < leafEnd; ++i) {
        if (isColumnHidden(i))
            continue;
        if (columnRegion(i) == region) {
            if (firstInRegion < 0)
                firstInRegion = i;
            lastInRegion = i;
        }
    }
    if (firstInRegion < 0)
        return;

    // 列拖拽在某个分组内部交换时，该分组及其所有祖先分组表头的边界应保持固定，不能跟随子列动画移动。
    const bool fixedGroupHeader = def.isGroup()
                                  && m_isColumnDragging
                                  && m_columnDragParentLeafStart >= 0
                                  && m_columnDragParentLeafEnd >= 0
                                  && leafStart <= m_columnDragParentLeafStart
                                  && leafEnd >= m_columnDragParentLeafEnd;

    const int x = fixedGroupHeader ? baseColumnX(firstInRegion)
                                   : columnX(firstInRegion);
    const int right = fixedGroupHeader ? (baseColumnX(lastInRegion) + columnWidth(lastInRegion))
                                       : (columnX(lastInRegion) + columnWidth(lastInRegion));
    const int width = right - x;
    const int y = depth * levelHeight;
    const int height = def.isGroup() ? levelHeight
                                      : (m_model->headerLevelCount() - depth) * levelHeight;

    const QRect rect(x, y, width, height);
    const QColor bgColor = m_style->color(AuGridStyle::HeaderBackgroundColor, m_model);
    const QColor borderColor = m_style->color(AuGridStyle::HeaderBorderColor, m_model);

    QColor textColor;
    if (def.hasHeaderForegroundColor())
        textColor = def.headerForegroundColor();
    else
        textColor = m_style->color(AuGridStyle::HeaderForegroundColor, m_model, -1, leafStart);

    Qt::Alignment alignment;
    if (def.hasHeaderAlignment())
        alignment = def.headerAlignment();
    else
        alignment = static_cast<Qt::Alignment>(m_style->value(AuGridStyle::HeaderAlignment, m_model, -1, leafStart).toInt());
    if (alignment == Qt::Alignment())
        alignment = def.isGroup() ? Qt::AlignCenter : def.alignment();
    alignment |= Qt::AlignVCenter;

    const QFont oldFont = painter->font();
    if (def.hasHeaderFont()) {
        painter->setFont(def.headerFont());
    } else {
        const QFont headerFont = m_style->value(AuGridStyle::HeaderFont, m_model, -1, leafStart).value<QFont>();
        if (headerFont != QFont())
            painter->setFont(headerFont);
    }

    const qreal opacity = columnOpacity(leafStart);
    const bool fade = opacity < 1.0;
    if (fade) {
        painter->save();
        painter->setOpacity(opacity);
    }

    painter->fillRect(rect, bgColor);
    painter->setPen(borderColor);
    // 每个列头绘制完整边框；相邻列头共享边界线，视觉上保持 1px，
    // 且列拖拽/排序动画中每个列头都自带完整边框，不会因共享边缺失而掉线。
    painter->drawRect(rect);

    const QSharedPointer<AuHeaderRenderer> headerRenderer =
        def.isGroup() ? QSharedPointer<AuHeaderRenderer>() : headerRendererForColumn(leafStart);
    if (headerRenderer) {
        AuHeaderRendererContext ctx;
        ctx.hovered = (leafStart == m_hoverHeaderColumn && (depth + 1) == m_hoverHeaderLevel);
        ctx.column = leafStart;
        ctx.level = depth + 1;
        ctx.isGroup = false;
        ctx.sorted = (leafStart == m_sortedColumn);
        ctx.sortOrder = m_sortOrder;
        ctx.headerPos = ctx.hovered ? m_hoverHeaderPos : QPoint(-1, -1);
        headerRenderer->paint(painter, rect, def, leafStart, depth + 1, m_model, m_style, textColor, ctx);
    } else {
        painter->setPen(textColor);
        QRect textRect = rect.adjusted(6, 0, -6, 0);
        if (!def.isGroup() && leafStart == m_sortedColumn)
            textRect.adjust(0, 0, -16, 0);
        painter->drawText(textRect, alignment, def.headerName());

        if (!def.isGroup() && leafStart == m_sortedColumn)
            drawSortIndicator(painter, rect, m_sortOrder);
    }

    painter->setFont(oldFont);
    if (fade)
        painter->restore();
}

void AuGridWidget::drawRowBackgrounds(QPainter *painter, int firstRow, int lastRow)
{
    if (!m_model || firstRow > lastRow)
        return;

    const int viewportW = viewportWidthForColumns();
    const int bodyH = bodyHeight();
    const QRect clipRect(0, headerHeight(), width(), bodyH);
    painter->save();
    painter->setClipRect(clipRect, Qt::IntersectClip);

    // 先铺满 body 区底色（包含滚动条占用的右侧区域），避免滚动条隐藏时露出旧像素。
    // 平移行图层不铺全局底色（会盖住下层渐变子行），改由平移行逐行铺不透明底。
    const QColor rowBackgroundColor = m_style->color(AuGridStyle::RowBackgroundColor, m_model);
    if (m_treeLayerFilter == 0 || m_treeLayerFilter == 3)
        painter->fillRect(clipRect, rowBackgroundColor.isValid() ? rowBackgroundColor : palette().base());

    const QColor selectedColor = m_style->color(AuGridStyle::SelectedRowBackgroundColor, m_model);
    const QColor alternateColor = m_style->color(AuGridStyle::AlternateRowBackgroundColor, m_model);
    const QColor hoverColor = m_style->color(AuGridStyle::HoverRowBackgroundColor, m_model);

    for (int row = firstRow; row <= lastRow; ++row) {
        if (!treeLayerAcceptRow(row))
            continue;
        const int y = rowY(row);
        const int h = rowHeight(row);
        // 树形展开 / 折叠动画：渐显 / 渐隐子行的行背景随进度变透明；
        // 平移行图层保持完全不透明（覆盖下层渐变子行，避免单元格边框穿透）
        const qreal rowOpacity = (m_treeLayerFilter == 2) ? 1.0 : rowAnimationOpacity(row);
        if (rowOpacity < 1.0) {
            painter->save();
            painter->setOpacity(rowOpacity);
        }
        if (m_treeLayerFilter == 2)
            painter->fillRect(0, y, viewportW, h,
                              rowBackgroundColor.isValid() ? rowBackgroundColor : palette().base());
        const bool rowSelected = isRowSelected(row);
        if (rowSelected && selectedColor.isValid())
            painter->fillRect(0, y, viewportW, h, selectedColor);
        else if (alternateColor.isValid() && row % 2 == 1)
            painter->fillRect(0, y, viewportW, h, alternateColor);

        if (row == m_hoverRow && !rowSelected && hoverColor.isValid())
            painter->fillRect(0, y, viewportW, h, hoverColor);
        if (rowOpacity < 1.0)
            painter->restore();
    }

    painter->restore();
}

void AuGridWidget::drawRows(QPainter *painter, int firstRow, int lastRow, int firstColumn, int lastColumn)
{
    if (!m_model || firstRow > lastRow || firstColumn > lastColumn)
        return;

    const int viewportW = viewportWidthForColumns();
    const int bodyH = bodyHeight();
    const ColumnRegion region = columnRegion(firstColumn);
    const int fixedWidth = fixedColumnsWidth();
    const QRect clipRect(region == ColumnRegion::Body ? fixedWidth : 0,
                         headerHeight(),
                         region == ColumnRegion::Body ? viewportW - fixedWidth : viewportW,
                         bodyH);
    painter->save();
    painter->setClipRect(clipRect, Qt::IntersectClip);

    const QColor defaultTextColor = palette().text().color();
    const QFont defaultFont = painter->font();

    const bool isPinnedRegion = (region == ColumnRegion::LeftPinned || region == ColumnRegion::RightPinned);
    const QColor selectedColor = m_style->color(AuGridStyle::SelectedRowBackgroundColor, m_model);
    const QColor alternateColor = m_style->color(AuGridStyle::AlternateRowBackgroundColor, m_model);
    const QColor hoverColor = m_style->color(AuGridStyle::HoverRowBackgroundColor, m_model);
    const int treeCol = isTreeMode() ? treeColumn() : -1;

    // 合并单元格锚点可能在可视范围外（区域被部分滚动出），把锚点并入绘制范围
    int paintFirstRow = firstRow;
    int paintFirstColumn = firstColumn;
    if (!m_spans.isEmpty()) {
        for (const CellSpan &s : m_spans) {
            const CellSpan e = effectiveSpan(s);
            if (!e.isValid() || columnRegion(e.anchorColumn) != region)
                continue;
            if (e.bottomRow() < firstRow || e.anchorRow > lastRow)
                continue;
            if (e.rightColumn() < firstColumn || e.anchorColumn > lastColumn)
                continue;
            paintFirstRow = qMin(paintFirstRow, e.anchorRow);
            paintFirstColumn = qMin(paintFirstColumn, e.anchorColumn);
        }
    }

    for (int row = paintFirstRow; row <= lastRow; ++row) {
        if (!treeLayerAcceptRow(row))
            continue;
        const int y = rowY(row);
        const int h = rowHeight(row);

        for (int col = paintFirstColumn; col <= lastColumn; ++col) {
            if (columnRegion(col) != region)
                continue;

            // 被合并覆盖的单元格不绘制任何内容
            const CellSpan span = spanAt(row, col);
            if (span.isValid() && (row != span.anchorRow || col != span.anchorColumn))
                continue;

            const AuColumnDef def = m_model->columnDefAt(col);
            const QRect rect = span.isValid() ? spanRect(span)
                : QRect(columnX(col), y, m_columnOffsets.at(col + 1) - m_columnOffsets.at(col), h);
            const QVariant value = m_model->cellValue(row, col);

            // 列显隐动画透明度 × 树形展开 / 折叠动画的行透明度
            const qreal opacity = columnOpacity(col) * rowAnimationOpacity(row);
            const bool fade = opacity < 1.0;
            if (fade) {
                painter->save();
                painter->setOpacity(opacity);
            }

            // 停靠列逐格铺背景；body 区合并单元格也需铺锚点行背景，避免露出斑马纹
            if (isPinnedRegion || span.isValid()) {
                const bool rowSelected = isRowSelected(row);
                QColor bg;
                if (rowSelected && selectedColor.isValid())
                    bg = selectedColor;
                else if (alternateColor.isValid() && row % 2 == 1)
                    bg = alternateColor;
                else {
                    const QColor rowBg = m_style->color(AuGridStyle::RowBackgroundColor, m_model);
                    bg = rowBg.isValid() ? rowBg : palette().base().color();
                }
                if (row == m_hoverRow && !rowSelected && hoverColor.isValid())
                    bg = hoverColor;
                painter->fillRect(rect, bg);
            }

            QSharedPointer<AuCellRenderer> renderer = rendererForColumn(col);

            painter->setPen(defaultTextColor);
            painter->setFont(defaultFont);

            AuCellRendererContext ctx;
            ctx.hovered = (row == m_hoverRow && col == m_hoverColumn);
            ctx.selected = (row == m_selectedRow && col == m_selectedColumn);
            ctx.alignment = def.alignment() | Qt::AlignVCenter;
            const QVariant alignVar = m_style->value(AuGridStyle::CellTextAlignment, m_model, row, col);
            if (alignVar.isValid() && alignVar.toInt() != 0)
                ctx.alignment = static_cast<Qt::Alignment>(alignVar.toInt()) | Qt::AlignVCenter;

            // 树形首列：层级缩进 + 展开箭头上下文
            if (col == treeCol) {
                const int originalRow = m_model->originalRowForVisibleRow(row);
                if (originalRow >= 0) {
                    ctx.treeRow = true;
                    ctx.treeDepth = m_model->rowDepth(originalRow);
                    ctx.treeHasChildren = m_model->rowHasChildren(originalRow);
                    ctx.treeExpanded = m_model->isRowExpanded(originalRow);
                    ctx.treeExpanderHovered = ctx.treeHasChildren
                        && row == m_hoverRow && col == m_hoverColumn
                        && treeExpanderRect(row).contains(m_hoverPos);
                }
            }

            renderer->paint(painter, rect, value, def, row, col, m_model, m_style, defaultTextColor, ctx);

            if (fade)
                painter->restore();
        }
    }

    painter->setFont(defaultFont);
    painter->restore();
}

void AuGridWidget::drawGridLines(QPainter *painter, int firstColumn, int lastColumn, int firstRow, int lastRow)
{
    if (!m_model || firstColumn > lastColumn || firstRow > lastRow)
        return;

    const ColumnRegion region = columnRegion(firstColumn);

    const int viewportW = viewportWidthForColumns();
    const int scrollBarExtent = m_style->integer(AuGridStyle::ScrollBarExtent, m_model);
    const QColor gridLineColor = m_style->color(AuGridStyle::GridLineColor, m_model);
    painter->setPen(gridLineColor);

    const int bodyTop = headerHeight();
    const int bodyBottom = qMin(bodyTop + totalContentHeight(), height() - (m_hScrollBarVisible ? scrollBarExtent : 0));

    const int fixedWidth = fixedColumnsWidth();
    const QRect clipRect(region == ColumnRegion::Body ? fixedWidth : 0,
                         bodyTop,
                         region == ColumnRegion::Body ? viewportW - fixedWidth : viewportW,
                         bodyBottom - bodyTop);
    painter->save();
    painter->setClipRect(clipRect, Qt::IntersectClip);

    if (m_rowOffsetsDirty)
        rebuildRowOffsets();

    // 合并单元格锚点可能在可视范围外（区域被部分滚动出），把锚点并入绘制范围
    int paintFirstRow = firstRow;
    int paintFirstColumn = firstColumn;
    if (!m_spans.isEmpty()) {
        for (const CellSpan &s : m_spans) {
            const CellSpan e = effectiveSpan(s);
            if (!e.isValid() || columnRegion(e.anchorColumn) != region)
                continue;
            if (e.bottomRow() < firstRow || e.anchorRow > lastRow)
                continue;
            if (e.rightColumn() < firstColumn || e.anchorColumn > lastColumn)
                continue;
            paintFirstRow = qMin(paintFirstRow, e.anchorRow);
            paintFirstColumn = qMin(paintFirstColumn, e.anchorColumn);
        }
    }

    // 每个单元格绘制完整边框；相邻单元格共享边界线，视觉上保持 1px，
    // 动画中每个单元格自带完整边框，避免掉线。
    // 隐藏左右（列方向）边框时只画上下两条横线（行分隔线保留）。
    const bool drawVertical = m_verticalGridLinesVisible;
    // 隐藏列方向边框且 body 内容未铺满区域宽度时，在内容右边缘补一条竖线，
    // 否则最右侧列的右侧缺边框观感不佳。x 取 body 区最后一列的右边缘（随滚动移动），
    // 始终描住“当前最右侧列”的右边界，而非停留在某个固定列上。
    int bodyContentRightX = -1;
    if (!drawVertical && region == ColumnRegion::Body && !m_bodyColumns.isEmpty()
        && m_columnOffsets.size() == m_model->columnCount() + 1) {
        const int lastBodyCol = m_bodyColumns.last();
        const int contentRight = fixedWidth + m_columnOffsets.at(lastBodyCol + 1) - m_xOffset;
        if (contentRight < viewportW)
            bodyContentRightX = contentRight;
    }
    // 重设数据切换动画期间（及为其捕获快照时），最后一行的底边线不绘制：
    // 内容整体平移时，这条线会在逐渐变空的视口中单独滑动，非常出戏；
    // 动画结束（或快照捕获完毕回到稳态）后按最终布局补画。
    const bool suppressLastRowBottom = m_dataResetAnimating || m_suppressBottomLineForCapture;
    const int lastDataRow = m_model->rowCount() - 1;
    for (int row = paintFirstRow; row <= lastRow; ++row) {
        if (!treeLayerAcceptRow(row))
            continue;
        const int y = rowY(row);
        const int h = rowHeight(row);
        // 渐变子行的边框随动画进度淡入 / 淡出（平移行图层保持不透明）
        const qreal rowOpacity = (m_treeLayerFilter == 2) ? 1.0 : rowAnimationOpacity(row);
        if (rowOpacity < 1.0) {
            painter->save();
            painter->setOpacity(rowOpacity);
        }
        // 末行且压制底边线时：竖线可见时画上 / 左 / 右三边，竖线隐藏时只画顶边
        const bool skipRowBottom = suppressLastRowBottom && row == lastDataRow;
        for (int col = paintFirstColumn; col <= lastColumn; ++col) {
            if (columnRegion(col) != region)
                continue;

            // 合并单元格：被覆盖格不画内框，锚点画整个合并区域的外框
            const CellSpan span = spanAt(row, col);
            if (span.isValid()) {
                if (row != span.anchorRow || col != span.anchorColumn)
                    continue;
                const QRect spanArea = spanRect(span);
                const qreal spanOpacity = columnOpacity(col);
                if (spanOpacity < 1.0) {
                    painter->save();
                    painter->setOpacity(spanOpacity);
                }
                if (drawVertical) {
                    if (skipRowBottom) {
                        painter->drawLine(spanArea.left(), spanArea.top(), spanArea.right(), spanArea.top());
                        painter->drawLine(spanArea.left(), spanArea.top(), spanArea.left(), spanArea.bottom());
                        painter->drawLine(spanArea.right(), spanArea.top(), spanArea.right(), spanArea.bottom());
                    } else {
                        painter->drawRect(spanArea);
                    }
                } else {
                    painter->drawLine(spanArea.left(), spanArea.top(), spanArea.right(), spanArea.top());
                    painter->drawLine(spanArea.left(), spanArea.bottom(), spanArea.right(), spanArea.bottom());
                }
                if (spanOpacity < 1.0)
                    painter->restore();
                continue;
            }

            const int x = columnX(col);
            const int w = columnWidth(col);
            const qreal opacity = columnOpacity(col);
            const bool fade = opacity < 1.0;
            if (fade) {
                painter->save();
                painter->setOpacity(opacity);
            }
            if (drawVertical) {
                if (skipRowBottom) {
                    painter->drawLine(x, y, x + w, y);
                    painter->drawLine(x, y, x, y + h);
                    painter->drawLine(x + w, y, x + w, y + h);
                } else {
                    painter->drawRect(x, y, w, h);
                }
            } else {
                painter->drawLine(x, y, x + w, y);
                if (!skipRowBottom)
                    painter->drawLine(x, y + h, x + w, y + h);
            }
            if (fade)
                painter->restore();
        }
        if (bodyContentRightX >= 0)
            painter->drawLine(bodyContentRightX, y, bodyContentRightX, y + h);
        if (rowOpacity < 1.0)
            painter->restore();
    }

    painter->restore();
}

void AuGridWidget::drawCellFocus(QPainter *painter)
{
    if (!m_model || !hasFocus())
        return;

    if (m_selectedRow < 0 || m_selectedColumn < 0)
        return;

    if (m_activeEditor && m_editRow == m_selectedRow && m_editColumn == m_selectedColumn)
        return;

    // 锚点落在不可聚焦格上（动态回调生效后可能出现）时不绘制焦点框；
    // 锚点会在下一次导航时自动跳到就近可聚焦列
    if (!isCellFocusable(m_selectedRow, m_selectedColumn))
        return;

    const QColor borderColor = m_style->color(AuGridStyle::FocusedCellBorderColor, m_model, m_selectedRow, m_selectedColumn);
    const int borderWidth = m_style->integer(AuGridStyle::FocusedCellBorderWidth, m_model, m_selectedRow, m_selectedColumn);
    if (!borderColor.isValid() || borderWidth <= 0)
        return;

    const QRect rect = cellRect(m_selectedRow, m_selectedColumn);
    if (rect.isEmpty())
        return;

    const int viewportW = viewportWidthForColumns();
    const int scrollBarExtent = m_style->integer(AuGridStyle::ScrollBarExtent, m_model);
    const int bodyTop = headerHeight();
    const int bodyBottom = qMin(bodyTop + totalContentHeight(), height() - (m_hScrollBarVisible ? scrollBarExtent : 0));
    const QRect clipRect(0, bodyTop, viewportW, bodyBottom - bodyTop);
    painter->save();
    painter->setClipRect(clipRect, Qt::IntersectClip);

    // body 单元格（含合并区域）滚动到停靠列下方时，边框不能画到固定列/停靠列上；
    // 边界与 drawRegionSeparators 的分隔线一致（仅统计实际发生 sticky 位移的停靠列）。
    // 停靠列单元格位置固定，不会滑入其他区域，无需收缩。
    if (columnRegion(m_selectedColumn) == ColumnRegion::Body) {
        QRect drawClip = clipRect;
        drawClip.setLeft(qMax(drawClip.left(), stickyLeftBoundaryX()));
        drawClip.setRight(qMin(drawClip.right(), stickyRightBoundaryX() - 1));
        painter->setClipRect(drawClip, Qt::IntersectClip);
    }

    QPen pen(borderColor);
    pen.setWidth(borderWidth);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect.adjusted(borderWidth / 2, borderWidth / 2, -borderWidth / 2, -borderWidth / 2));

    painter->restore();
}

int AuGridWidget::stickyLeftBoundaryX() const
{
    // 左侧 sticky 区域的右边界：固定列与实际发生 sticky 位移的左停靠列中最大右边缘
    int boundary = fixedColumnsWidth();
    for (int col : m_leftPinnedColumns) {
        const int stickyX = columnX(col);
        const int naturalX = m_columnOffsets.at(col) - m_xOffset;
        if (stickyX > naturalX)
            boundary = qMax(boundary, stickyX + columnWidth(col));
    }
    return boundary;
}

int AuGridWidget::stickyRightBoundaryX() const
{
    // 右侧 sticky 区域的左边界：实际发生 sticky 位移的右停靠列中最小左边缘
    const int viewportW = viewportWidthForColumns();
    int boundary = viewportW;
    for (int col : m_rightPinnedColumns) {
        const int stickyX = columnX(col);
        const int naturalX = m_columnOffsets.at(col) - m_xOffset;
        if (stickyX < naturalX)
            boundary = qMin(boundary, stickyX);
    }
    return boundary;
}

void AuGridWidget::drawRegionSeparators(QPainter *painter)
{
    if (!m_model)
        return;

    const int viewportW = viewportWidthForColumns();
    const int scrollBarExtent = m_style->integer(AuGridStyle::ScrollBarExtent, m_model);
    const int bodyTop = headerHeight();
    const int bodyBottom = qMin(bodyTop + totalContentHeight(), height() - (m_hScrollBarVisible ? scrollBarExtent : 0));

    const QColor separatorColor = m_style->color(AuGridStyle::HeaderBorderColor, m_model);
    if (!separatorColor.isValid())
        return;

    QPen pen(separatorColor);
    pen.setWidth(1);
    painter->setPen(pen);

    // 停靠区与 body 区的交界竖线始终绘制（无论停靠列是否发生 sticky 位移，
    // 也不受列边框开关影响）；同侧多列停靠之间的分隔随列边框开关。
    int leftSeparatorX = -1;
    if (fixedColumnsWidth() > 0)
        leftSeparatorX = stickyLeftBoundaryX();
    for (int i = m_leftPinnedColumns.size() - 1; i >= 0; --i) {
        const int col = m_leftPinnedColumns.at(i);
        if (!isColumnHidden(col)) {
            leftSeparatorX = qMax(leftSeparatorX, columnX(col) + columnWidth(col));
            break;
        }
    }
    if (leftSeparatorX > 0)
        painter->drawLine(leftSeparatorX, 0, leftSeparatorX, bodyBottom);

    int rightSeparatorX = stickyRightBoundaryX();
    for (int col : m_rightPinnedColumns) {
        if (!isColumnHidden(col)) {
            rightSeparatorX = qMin(rightSeparatorX, columnX(col));
            break;
        }
    }
    if (rightSeparatorX < viewportW)
        painter->drawLine(rightSeparatorX, 0, rightSeparatorX, bodyBottom);
}

void AuGridWidget::setVerticalGridLinesVisible(bool visible)
{
    if (m_verticalGridLinesVisible == visible)
        return;
    m_verticalGridLinesVisible = visible;
    update();
}

bool AuGridWidget::isVerticalGridLinesVisible() const
{
    return m_verticalGridLinesVisible;
}

