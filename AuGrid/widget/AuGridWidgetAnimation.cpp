// ============================================================================
// AuGridWidget 数据变化动画与加载遮罩：排序 FLIP 位移动画、删除行幽灵
// 动画、重设数据切换动画、加载遮罩淡入淡出与文案。
// ============================================================================

#include "AuGridWidget.h"
#include "AuScrollBar.h"

#include <QPixmap>
#include <QTimer>
#include <QVariantAnimation>

void AuGridWidget::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;

    if (loading) {
        // 蒙层盖不住子控件，先收掉：编辑器（数据即将刷新，放弃未提交修改）、
        // 进行中的拖拽、展开动画与滚动条
        if (isEditing())
            closeEditor(false);
        cancelRowDrag();
        cancelColumnDrag();
        finishTreeAnimation();
        clearHoverCell();
        clearHeaderHover();
        // 淡入过渡中滚动条仍短暂显示（随蒙层淡出），禁用交互保证遮罩屏蔽行为不变
        m_hScrollBar->setEnabled(false);
        m_vScrollBar->setEnabled(false);
    } else {
        m_hScrollBar->setEnabled(true);
        m_vScrollBar->setEnabled(true);
    }

    // 滚动条显隐与列布局在过渡开始时就位（此时蒙层几乎不透明，布局变化不可见），
    // 之后透明度随蒙层淡变，避免淡出结束时滚动条 / 列宽突然变化造成闪烁
    updateScrollBars();

    m_loadingAnimation->stop();
    m_loadingAnimation->setDuration(loading ? 200 : 160);
    m_loadingAnimation->setStartValue(m_loadingOpacity);
    m_loadingAnimation->setEndValue(loading ? 1.0 : 0.0);
    m_loadingAnimation->start();
    update();
}

void AuGridWidget::applyLoadingFadeToScrollBars(qreal maskOpacity)
{
    // 滚动条是子控件，蒙层盖不住；淡入 / 淡出过渡中让其透明度与蒙层反向同步淡变，
    // 稳态（蒙层全显 / 全隐）时交回 updateScrollBars 按内容与加载状态决定真实显隐
    const qreal barOpacity = 1.0 - maskOpacity;
    m_hScrollBar->setContentOpacity(barOpacity);
    m_vScrollBar->setContentOpacity(barOpacity);
    if (maskOpacity <= 0.0 || maskOpacity >= 1.0)
        updateScrollBars();
}

bool AuGridWidget::isLoading() const
{
    return m_loading;
}

void AuGridWidget::setLoadingText(const QString &text)
{
    if (m_loadingText == text)
        return;
    m_loadingText = text;
    if (m_loadingOpacity > 0.0)
        update();
}

QString AuGridWidget::loadingText() const
{
    return m_loadingText;
}

void AuGridWidget::setDataAnimationEnabled(bool enabled)
{
    if (m_dataAnimationEnabled == enabled)
        return;
    m_dataAnimationEnabled = enabled;
    if (!enabled) {
        finishSortMoveAnimation();
        finishDataResetAnimation();
        m_dataResetPending = false;
        m_dataResetSnapshot = QImage();
    }
}

bool AuGridWidget::isDataAnimationEnabled() const
{
    return m_dataAnimationEnabled;
}

void AuGridWidget::setSortAnimationDuration(int ms)
{
    m_sortAnimationDuration = qMax(0, ms);
    if (m_sortAnimationDuration <= 0)
        finishSortMoveAnimation();
}

int AuGridWidget::sortAnimationDuration() const
{
    return m_sortAnimationDuration;
}

void AuGridWidget::setDataResetAnimationDuration(int ms)
{
    m_resetAnimationDuration = qMax(0, ms);
    if (m_resetAnimationDuration <= 0) {
        finishDataResetAnimation();
        m_dataResetPending = false;
        m_dataResetSnapshot = QImage();
    }
}

int AuGridWidget::dataResetAnimationDuration() const
{
    return m_resetAnimationDuration;
}

void AuGridWidget::captureLayoutSnapshot()
{
    if (!m_model) {
        m_animLastOrder.clear();
        m_animLastOffsets.clear();
        return;
    }

    if (m_rowOffsetsDirty)
        rebuildRowOffsets();

    const int count = m_model->rowCount();
    m_animLastOrder.resize(count);
    for (int i = 0; i < count; ++i)
        m_animLastOrder[i] = m_model->originalRowForVisibleRow(i);
    m_animLastOffsets = m_rowOffsets;
}

void AuGridWidget::captureRemovedRowGhosts(const QVector<int> &originalRows)
{
    m_removedRowGhosts.clear();
    m_removedOldVis.clear();
    m_rowRemovePending = false;
    if (!m_dataAnimationEnabled || m_loading || !isVisible() || bodyHeight() <= 0)
        return;
    if (isTreeMode())
        return;   // 树形模式不支持按行删除（模型层同样忽略），无消失动画
    if (m_animLastOrder.isEmpty() || m_animLastOffsets.size() != m_animLastOrder.size() + 1)
        return;

    // 与进行中的排序 / 位移动画互斥：先落地为终态，再捕获本次删除的幽灵带
    finishSortMoveAnimation();

    const int bodyTop = headerHeight();
    const int bodyHeightPx = bodyHeight();
    // 行偏移表为 body 相对坐标：视口可见带 = [m_yOffset, m_yOffset + bodyHeight)
    const int viewTop = m_yOffset;
    const int viewBottom = m_yOffset + bodyHeightPx;

    // 旧可见行号的完整被删集合（含视口外）：新→旧位置映射必须跳过全部被删行
    QVector<RemovedRowGhost> ghosts;
    for (int original : originalRows) {
        const int oldVis = m_animLastOrder.indexOf(original);
        if (oldVis < 0)
            continue;   // 行不可见（被过滤 / 折叠）：无外观可捕获，映射同样跳过
        m_removedOldVis.append(oldVis);
        const int y = m_animLastOffsets.at(oldVis);
        const int h = m_animLastOffsets.at(oldVis + 1) - y;
        if (y + h <= viewTop || y >= viewBottom)
            continue;   // 完全在视口外：无需幽灵，但参与位置映射
        RemovedRowGhost ghost;
        ghost.y = y;
        ghost.height = h;
        ghosts.append(ghost);
    }
    if (m_removedOldVis.isEmpty())
        return;

    std::sort(m_removedOldVis.begin(), m_removedOldVis.end());
    if (!ghosts.isEmpty()) {
        // 只截内容区宽度：grab 会把子控件（右侧竖向滚动条）一并渲染进来，
        // 若按整宽快照，每条幽灵带右缘都会带上一段滚动条像素，多行删除时
        // 叠在实时滚动条上呈现为阶梯状的粗竖线残影
        const int contentW = viewportWidthForColumns();
        if (contentW <= 0) {
            m_removedRowGhosts.clear();
            return;
        }
        // 整个 body 一次快照再裁出行带（此刻模型旧数据仍完整，grab 即旧行内容）
        const QImage bodySnapshot = grab(QRect(0, bodyTop, contentW, bodyHeightPx)).toImage();
        if (bodySnapshot.isNull()) {
            m_removedRowGhosts.clear();
            return;
        }
        const qreal dpr = bodySnapshot.devicePixelRatio() > 0 ? bodySnapshot.devicePixelRatio() : 1.0;
        for (RemovedRowGhost &ghost : ghosts) {
            // 快照第 p 像素行对应 body 坐标 viewTop + p；裁出可见带并记录其视口绘制 y
            const int bandTop = qMax(viewTop, ghost.y);
            const int cropTop = bandTop - viewTop;
            const int cropH = qMin(viewBottom, ghost.y + ghost.height) - bandTop;
            // copy(x, y, w, h)：行带是横向整条、纵向裁剪，勿将纵向偏移误传给 x
            ghost.image = bodySnapshot.copy(0, qRound(cropTop * dpr),
                                            qRound(contentW * dpr), qRound(cropH * dpr));
            ghost.y = bodyTop + cropTop;   // 视口坐标（裁剪过的带从可见处起画）
        }
        m_removedRowGhosts = ghosts;
    }
    m_rowRemovePending = true;
}

void AuGridWidget::startRowRemoveAnimation()
{
    m_rowRemovePending = false;
    if (!m_dataAnimationEnabled || m_loading || !isVisible())
        return;
    if (m_rowOffsetsDirty)
        rebuildRowOffsets();   // 删除后的新布局行偏移

    // 新可见行序列 = 旧可见行序列剔除被删行：由此得到每行旧 y → 新 y 的上滑映射。
    // 不能走排序 FLIP：删除使原始行号前移，按身份映射会把不同数据行错配到一起。
    const int oldCount = m_animLastOrder.size();
    const int newCount = m_model ? m_model->rowCount() : 0;
    if (oldCount == 0 || m_animLastOffsets.size() != oldCount + 1)
        return;

    m_dataAnimationEntries.clear();
    const int removedCount = m_removedOldVis.size();
    // 行偏移为 body 相对坐标：视口带 = [m_yOffset, m_yOffset + bodyHeight)
    const int viewTop = m_yOffset;
    const int viewBottom = m_yOffset + bodyHeight();
    const int bandMargin = rowHeight() * 4;   // 只动画视口邻域的行，远处行直接就位
    int removedIdx = 0;
    int newVis = 0;
    for (int oldVis = 0; oldVis < oldCount && newVis < newCount; ++oldVis) {
        if (removedIdx < removedCount && m_removedOldVis.at(removedIdx) == oldVis) {
            ++removedIdx;
            continue;   // 该旧行被删除：无新载体，由幽灵带负责淡出
        }
        const int oldY = m_animLastOffsets.at(oldVis);
        const int newY = m_rowOffsets.at(newVis);
        const bool oldNear = (oldY >= viewTop - bandMargin && oldY <= viewBottom + bandMargin);
        const bool newNear = (newY >= viewTop - bandMargin && newY <= viewBottom + bandMargin);
        if (oldY != newY && (oldNear || newNear)) {
            DataMoveAnimationEntry e;
            e.visibleRow = newVis;
            e.startOffset = oldY;
            e.endOffset = newY;
            m_dataAnimationEntries.append(e);
        }
        ++newVis;
    }

    if (m_dataAnimationEntries.isEmpty() && m_removedRowGhosts.isEmpty())
        return;
    if (m_sortAnimationDuration <= 0) {
        finishSortMoveAnimation();   // 时长为 0：不播动画，直接呈现终态
        return;
    }

    m_dataAnimationProgress = 0.0;
    m_dataAnimation = new QVariantAnimation(this);
    m_dataAnimation->setStartValue(0.0);
    m_dataAnimation->setEndValue(1.0);
    m_dataAnimation->setDuration(m_sortAnimationDuration);
    m_dataAnimation->setEasingCurve(QEasingCurve::OutCubic);   // 与排序动画一致的快出缓停手感
    connect(m_dataAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_dataAnimationProgress = qBound(0.0, value.toReal(), 1.0);
        update();
    });
    connect(m_dataAnimation, &QVariantAnimation::finished, this, [this]() {
        finishSortMoveAnimation();
    });
    m_dataAnimation->start();
}

void AuGridWidget::startSortMoveAnimation()
{
    finishSortMoveAnimation();
    if (!m_dataAnimationEnabled || !m_model || m_loading || !isVisible())
        return;
    if (isTreeMode())
        return;   // 树形结构变化由展开 / 折叠动画负责，避免两套动画叠加

    const int oldCount = m_animLastOrder.size();
    const int newCount = m_model->rowCount();
    if (oldCount == 0 || newCount == 0 || m_animLastOffsets.size() != oldCount + 1)
        return;

    // 行序未变化则无位移可言
    bool orderChanged = (newCount != oldCount);
    for (int i = 0; !orderChanged && i < newCount; ++i) {
        if (m_animLastOrder.at(i) != m_model->originalRowForVisibleRow(i))
            orderChanged = true;
    }
    if (!orderChanged)
        return;

    if (m_rowOffsetsDirty)
        rebuildRowOffsets();   // 新布局的行高 / 偏移（WordWrap 行高随数据位置变化）

    QHash<int, int> oldVisibleOf;
    oldVisibleOf.reserve(oldCount * 2);
    for (int i = 0; i < oldCount; ++i)
        oldVisibleOf.insert(m_animLastOrder.at(i), i);

    const int bodyTop = headerHeight();
    const int bodyBottom = bodyTop + bodyHeight();
    const int outsideTop = bodyTop - rowHeight();   // 视口上沿外一行（完全不可见处）
    const int outsideBottom = bodyBottom;
    const int rowH = rowHeight();

    // 待入队的退场 / 入场行：visibleRow = 新可见行号，inBandY = 视口带内的真实位置，
    // outY = 视口带外的真实位置（用于排序定队列次序）
    struct TrainItem { int visibleRow; int inBandY; int outY; };
    QVector<TrainItem> exitUp, exitDown, enterFromTop, enterFromBottom;
    m_dataAnimationEntries.clear();

    const int totalOriginals = qMax(oldCount, m_model->originalRowCount());
    for (int original = 0; original < totalOriginals; ++original) {
        const int newVis = (original < m_model->originalRowCount())
                               ? m_model->visibleRowForOriginalRow(original)
                               : -1;
        if (newVis < 0)
            continue;   // 移出可见集合（被过滤等）：无绘制载体，瞬时消失

        const int newY = m_rowOffsets.at(newVis);
        const int oldVis = oldVisibleOf.value(original, -1);
        const int oldY = (oldVis >= 0) ? m_animLastOffsets.at(oldVis) : -1;

        const bool oldInBand = (oldVis >= 0 && oldY >= outsideTop && oldY <= outsideBottom);
        const bool newInBand = (newY >= outsideTop && newY <= outsideBottom);

        if (oldInBand && newInBand) {
            // 新旧位置都在视口带内：直接从旧位置滑到新位置
            if (oldY == newY)
                continue;
            DataMoveAnimationEntry e;
            e.visibleRow = newVis;
            e.startOffset = oldY;
            e.endOffset = newY;
            m_dataAnimationEntries.append(e);
        } else if (oldInBand) {
            // 可视行移出视口：向新位置方向退场（队列在下方统一排定）
            if (newY < bodyTop)
                exitUp.append({ newVis, oldY, newY });
            else
                exitDown.append({ newVis, oldY, newY });
        } else if (newInBand) {
            // 视口外的行变为可见：从旧位置所在方向入场（oldY < 0 = 新增行，自下方进入）
            if (oldVis >= 0 && oldY < bodyTop)
                enterFromTop.append({ newVis, newY, oldY });
            else
                enterFromBottom.append({ newVis, newY, oldVis >= 0 ? oldY : outsideBottom });
        }
        // 起止都在视口带外（跨视口移动 / 始终不可见）：不参与动画，保持不可见
    }

    // 退场 / 入场按“距视口边缘的远近”排成间距为一行的队列：
    // 离边缘最近的行贴边出入，其余依次向视口外排开——保持行间相对间距，
    // 避免多行同时叠在同一个边缘点上形成色块。
    const auto byOutDesc = [](const TrainItem &a, const TrainItem &b) { return a.outY > b.outY; };
    const auto byOutAsc = [](const TrainItem &a, const TrainItem &b) { return a.outY < b.outY; };

    std::sort(exitUp.begin(), exitUp.end(), byOutDesc);
    for (int k = 0; k < exitUp.size(); ++k) {
        DataMoveAnimationEntry e;
        e.visibleRow = exitUp.at(k).visibleRow;
        e.startOffset = exitUp.at(k).inBandY;
        e.endOffset = outsideTop - k * rowH;
        m_dataAnimationEntries.append(e);
    }
    std::sort(exitDown.begin(), exitDown.end(), byOutAsc);
    for (int k = 0; k < exitDown.size(); ++k) {
        DataMoveAnimationEntry e;
        e.visibleRow = exitDown.at(k).visibleRow;
        e.startOffset = exitDown.at(k).inBandY;
        e.endOffset = outsideBottom + k * rowH;
        m_dataAnimationEntries.append(e);
    }
    std::sort(enterFromTop.begin(), enterFromTop.end(), byOutDesc);
    for (int k = 0; k < enterFromTop.size(); ++k) {
        DataMoveAnimationEntry e;
        e.visibleRow = enterFromTop.at(k).visibleRow;
        e.startOffset = outsideTop - k * rowH;
        e.endOffset = enterFromTop.at(k).inBandY;
        m_dataAnimationEntries.append(e);
    }
    std::sort(enterFromBottom.begin(), enterFromBottom.end(), byOutAsc);
    for (int k = 0; k < enterFromBottom.size(); ++k) {
        DataMoveAnimationEntry e;
        e.visibleRow = enterFromBottom.at(k).visibleRow;
        e.startOffset = outsideBottom + k * rowH;
        e.endOffset = enterFromBottom.at(k).inBandY;
        m_dataAnimationEntries.append(e);
    }

    if (m_dataAnimationEntries.isEmpty() && m_removedRowGhosts.isEmpty())
        return;
    if (m_sortAnimationDuration <= 0) {
        finishSortMoveAnimation();   // 时长为 0：不播动画，清理条目直接呈现终态
        return;
    }

    m_dataAnimationProgress = 0.0;
    m_dataAnimation = new QVariantAnimation(this);
    m_dataAnimation->setStartValue(0.0);
    m_dataAnimation->setEndValue(1.0);
    m_dataAnimation->setDuration(m_sortAnimationDuration);
    m_dataAnimation->setEasingCurve(QEasingCurve::OutCubic);   // 快出缓停，接近 web 端手感
    connect(m_dataAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_dataAnimationProgress = qBound(0.0, value.toReal(), 1.0);
        update();
    });
    connect(m_dataAnimation, &QVariantAnimation::finished, this, [this]() {
        finishSortMoveAnimation();
    });
    m_dataAnimation->start();
}

void AuGridWidget::finishSortMoveAnimation()
{
    if (m_dataAnimation) {
        m_dataAnimation->stop();
        delete m_dataAnimation;
        m_dataAnimation = nullptr;
    }
    if (m_dataAnimationEntries.isEmpty() && m_removedRowGhosts.isEmpty()
        && m_dataAnimationProgress == 0.0)
        return;
    m_dataAnimationEntries.clear();
    m_removedRowGhosts.clear();
    m_removedOldVis.clear();
    m_dataAnimationProgress = 0.0;
    update();
}

void AuGridWidget::startDataResetAnimation()
{
    m_dataResetPending = false;
    if (!m_dataAnimationEnabled || m_loading || !isVisible() || m_dataResetSnapshot.isNull())
        return;
    if (!m_model || m_model->rowCount() <= 0)
        return;
    if (m_resetAnimationDuration <= 0) {
        m_dataResetSnapshot = QImage();
        return;   // 时长为 0：不播动画，直接呈现新数据
    }

    finishSortMoveAnimation();
    finishDataResetAnimation();

    m_dataResetSlideHeight = bodyHeight();
    m_dataResetProgress = 0.0;
    m_dataResetAnimating = true;
    m_dataResetAnimation = new QVariantAnimation(this);
    m_dataResetAnimation->setStartValue(0.0);
    m_dataResetAnimation->setEndValue(1.0);
    m_dataResetAnimation->setDuration(m_resetAnimationDuration);
    // 总时长前半为离场、后半为入场，两段各自在 rowY / 绘制处施加 OutCubic 缓动，
    // 动画驱动本身保持线性
    m_dataResetAnimation->setEasingCurve(QEasingCurve::Linear);
    connect(m_dataResetAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_dataResetProgress = qBound(0.0, value.toReal(), 1.0);
        update();
    });
    connect(m_dataResetAnimation, &QVariantAnimation::finished, this, [this]() {
        finishDataResetAnimation();
    });
    m_dataResetAnimation->start();
}

void AuGridWidget::finishDataResetAnimation()
{
    if (m_dataResetAnimation) {
        m_dataResetAnimation->stop();
        delete m_dataResetAnimation;
        m_dataResetAnimation = nullptr;
    }
    if (!m_dataResetAnimating)
        return;
    m_dataResetAnimating = false;
    m_dataResetProgress = 0.0;
    m_dataResetSlideHeight = 0;
    m_dataResetSnapshot = QImage();
    update();
}

