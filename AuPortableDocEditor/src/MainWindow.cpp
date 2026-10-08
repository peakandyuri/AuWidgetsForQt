#include "MainWindow.h"
#include "PagePickerDialog.h"
#include "PdfPreviewRenderer.h"
#include "pdf/PdfPageEditor.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QRectF>
#include <QSet>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTransform>
#include <QUrl>
#include <QtMath>

#include <cmath>

namespace {

// 顺时针 3/4 圆弧 + 箭头的旋转图标；mirrored 为 true 时水平镜像（逆时针）
QIcon makeCircularArrowIcon(bool mirrored, const QColor &color)
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 2.6);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawArc(QRectF(7.5, 7.5, 17, 17), 100 * 16, -270 * 16);

    const qreal cx = 16, cy = 16, r = 8.5;
    const double rad = qDegreesToRadians(100.0 - 270.0); // 弧终点角
    const QPointF point(cx + r * std::cos(rad), cy - r * std::sin(rad));
    const QPointF tangent(std::sin(rad), std::cos(rad)); // 顺时针运动切线（Qt 坐标）
    const QPointF normal(-tangent.y(), tangent.x());
    QPolygonF head;
    head << point + tangent * 6.0 << point + normal * 3.2 << point - normal * 3.2;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(color));
    painter.drawPolygon(head);
    painter.end();
    if (mirrored)
        return QIcon(QPixmap::fromImage(pixmap.toImage().mirrored(true, false)));
    return QIcon(pixmap);
}

// 旋转 180°：上下两段弧各带一个顺时针箭头，表示转一整圈
QIcon makeRotate180Icon(const QColor &color)
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 2.6);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QRectF arc(7.5, 7.5, 17, 17);
    painter.drawArc(arc, 10 * 16, 160 * 16);
    painter.drawArc(arc, 190 * 16, 160 * 16);

    const qreal cx = 16, cy = 16, r = 8.5;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(color));
    const double endAngles[2] = { 10.0, 190.0 };
    for (int i = 0; i < 2; ++i) {
        const double rad = qDegreesToRadians(endAngles[i]);
        const QPointF point(cx + r * std::cos(rad), cy - r * std::sin(rad));
        const QPointF tangent(std::sin(rad), std::cos(rad));
        const QPointF normal(-tangent.y(), tangent.x());
        QPolygonF head;
        head << point + tangent * 6.0 << point + normal * 3.2 << point - normal * 3.2;
        painter.drawPolygon(head);
    }
    painter.end();
    return QIcon(pixmap);
}

} // namespace

// ---------------------------------------------------------------- PageListWidget

PageListWidget::PageListWidget(QWidget *parent)
    : QListWidget(parent)
    , m_pressedRow(-1)
    , m_dragging(false)
    , m_indicatorRow(-1)
{
    // 页面顺序的调整使用自定义鼠标拖动（见 mousePress/Move/Release），
    // 不启用视图自带的拖放机制（NoDragDrop），避免平台拖放行为差异
    setViewMode(QListView::IconMode);
    setFlow(QListView::LeftToRight);
    setWrapping(true);
    setResizeMode(QListView::Adjust); // 窗口缩放时重新平铺
    setMovement(QListView::Static);
    setSelectionMode(QAbstractItemView::SingleSelection); // 页面列表不支持多选
    setDragDropMode(QAbstractItemView::NoDragDrop);
    setIconSize(QSize(140, 180));
    setGridSize(QSize(170, 230));
    setSpacing(8);
    setUniformItemSizes(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
}

int PageListWidget::insertionRowAt(const QPoint &pos) const
{
    const int n = count();
    if (n == 0)
        return 0;

    // 先确定光标所在行带（同一行的条目），再按条目水平中线决定插到哪个条目
    // 之前/之后。光标经过条目间隙或行间空隙时落到最近行的对应位置，
    // 只有第一行上方 → 最前、最后一行下方 → 末尾。避免间隙被误判为“插到最后”。
    int bandFirst = -1;
    int bandLast = -1;
    for (int i = 0; i < n; ++i) {
        QRect r = visualItemRect(item(i));
        if (!r.isNull() && pos.y() >= r.top() && pos.y() <= r.bottom()) {
            if (bandFirst < 0)
                bandFirst = i;
            bandLast = i;
        }
    }
    if (bandFirst < 0) {
        QRect first = visualItemRect(item(0));
        QRect last = visualItemRect(item(n - 1));
        if (pos.y() < first.top())
            return 0;
        if (pos.y() > last.bottom())
            return n;
        // 行与行之间的空隙：就近取一行带
        int nearestRow = 0;
        int bestDy = INT_MAX;
        for (int i = 0; i < n; ++i) {
            QRect r = visualItemRect(item(i));
            const int dy = qAbs(pos.y() - r.center().y());
            if (dy < bestDy) {
                bestDy = dy;
                nearestRow = i;
            }
        }
        bandFirst = nearestRow;
        bandLast = nearestRow;
    }
    for (int i = bandFirst; i <= bandLast; ++i) {
        if (pos.x() < visualItemRect(item(i)).center().x())
            return i;
    }
    return bandLast + 1;
}

bool PageListWidget::isNoOpSlot(int row) const
{
    if (!m_dragging || m_dragRows.isEmpty())
        return false;
    // 自身页之前与之后都不会改变顺序，视为无效槽位
    return row >= m_dragRows.first() && row <= m_dragRows.last() + 1;
}

QRect PageListWidget::indicatorRect(int row) const
{
    const int n = count();
    if (n == 0)
        return QRect();
    QRect cell;
    if (row < n)
        cell = visualItemRect(item(row));
    else
        cell = visualItemRect(item(n - 1));
    if (cell.isNull())
        return QRect();
    const int gap = qMax(12, gridSize().width() - cell.width());
    const int x = (row < n) ? cell.left() - gap / 2 : cell.right() + gap / 2;
    return QRect(x, cell.top(), 0, cell.height());
}

void PageListWidget::mousePressEvent(QMouseEvent *event)
{
    m_pressPos = event->pos();
    m_pressedRow = indexAt(event->pos()).row();
    m_dragging = false;
    m_indicatorRow = -1;
    QListWidget::mousePressEvent(event);
}

void PageListWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) {
        if (m_pressedRow >= 0 && (event->buttons() & Qt::LeftButton)
                && (event->pos() - m_pressPos).manhattanLength()
                   > QApplication::startDragDistance()) {
            startDrag();
            viewport()->update();
        }
        if (!m_dragging)
            QListWidget::mouseMoveEvent(event);
        return;
    }
    // 拖动中：半透明快照跟随光标，指示线标示有效插入位置
    int row = insertionRowAt(event->pos());
    m_ghostPos = event->pos() - m_ghostOffset;
    m_indicatorRow = isNoOpSlot(row) ? -1 : row;
    viewport()->update();
}

void PageListWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragging) {
        finishDrag(true); // 松手才真正调整顺序
        return;
    }
    m_pressedRow = -1;
    QListWidget::mouseReleaseEvent(event);
}

void PageListWidget::keyPressEvent(QKeyEvent *event)
{
    if (m_dragging && event->key() == Qt::Key_Escape) {
        finishDrag(false); // Esc 取消拖动：不调整顺序，释放快照
        event->accept();
        return;
    }
    QListWidget::keyPressEvent(event);
}

void PageListWidget::startDrag()
{
    // 被拖行：按下的行在选中集合内则拖全部选中行，否则仅该行
    QVector<int> selectedRows;
    for (int i = 0; i < count(); ++i) {
        if (item(i)->isSelected())
            selectedRows.append(i);
    }
    if (selectedRows.contains(m_pressedRow))
        m_dragRows = selectedRows;
    else
        m_dragRows = QVector<int>() << m_pressedRow;

    m_dragging = true;
    buildGhost();
    viewport()->update();
}

// 半透明快照：用条目的缩略图与文本手工合成页面的拷贝
// （不使用 QWidget::grab/setCursor——它们会干扰后续鼠标移动事件的投递）
void PageListWidget::buildGhost()
{
    QListWidgetItem *draggedItem = item(m_pressedRow);
    if (!draggedItem)
        return;
    const QSize isz = iconSize();
    const int w = gridSize().width() - 12;
    const int h = gridSize().height() - 12;
    QPixmap source(w, h);
    source.fill(Qt::transparent);
    QPainter painter(&source);
    const QPixmap page = draggedItem->icon().pixmap(isz);
    painter.drawPixmap((w - page.width()) / 2, 0, page);
    painter.setPen(palette().color(QPalette::WindowText));
    painter.setFont(font());
    painter.drawText(QRect(0, isz.height() + 4, w, h - isz.height() - 4),
                     Qt::AlignHCenter | Qt::AlignTop, draggedItem->text());
    painter.end();

    m_ghost = QPixmap(source.size());
    m_ghost.fill(Qt::transparent);
    QPainter ghostPainter(&m_ghost);
    ghostPainter.setOpacity(0.65);
    ghostPainter.drawPixmap(0, 0, source);
    ghostPainter.end();

    // 光标按下的位置对应快照上的相对位置（约束在快照范围内）
    QRect itemRect = visualItemRect(draggedItem);
    m_ghostOffset = m_pressPos - itemRect.topLeft();
    m_ghostOffset.setX(qBound(0, m_ghostOffset.x(), w));
    m_ghostOffset.setY(qBound(0, m_ghostOffset.y(), h));
    m_ghostPos = m_pressPos - m_ghostOffset;
}

void PageListWidget::finishDrag(bool commit)
{
    QVector<int> rows = m_dragRows;
    int indicator = m_indicatorRow;
    m_dragging = false;
    m_indicatorRow = -1;
    m_pressedRow = -1;
    m_dragRows.clear();
    m_ghost = QPixmap();
    viewport()->update();
    if (!commit || rows.isEmpty() || indicator < 0 || isNoOpSlot(indicator))
        return;

    // 原位调整：取出被拖行，校正移除偏移后插入到目标位置
    int removedBefore = 0;
    for (int i = 0; i < rows.size(); ++i) {
        if (rows.at(i) < indicator)
            ++removedBefore;
    }
    const int insertAt = indicator - removedBefore;
    QList<QListWidgetItem *> taken;
    for (int i = rows.size() - 1; i >= 0; --i)
        taken.prepend(takeItem(rows.at(i)));
    for (int i = 0; i < taken.size(); ++i)
        insertItem(insertAt + i, taken.at(i));
    emit pagesReordered();
}

void PageListWidget::paintEvent(QPaintEvent *event)
{
    QListWidget::paintEvent(event);
    QPainter painter(viewport());
    if (m_dragging && m_indicatorRow >= 0) {
        QRect line = indicatorRect(m_indicatorRow);
        if (!line.isNull()) {
            // 与 QAbstractItemView 绘制内置指示线相同的技巧：在视口上作画
            painter.setRenderHint(QPainter::Antialiasing);
            QPen pen(palette().color(QPalette::Highlight), 3);
            pen.setCapStyle(Qt::RoundCap);
            painter.setPen(pen);
            const int top = line.top() + 2;
            const int bottom = line.bottom() - 2;
            painter.drawLine(line.left(), top, line.left(), bottom);
            painter.drawLine(line.left() - 6, top, line.left() + 6, top);
            painter.drawLine(line.left() - 6, bottom, line.left() + 6, bottom);
        }
    }
    if (m_dragging && !m_ghost.isNull())
        painter.drawPixmap(m_ghostPos, m_ghost);
}

// ---------------------------------------------------------------- MainWindow

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_doc(new PdfPageEditor)
    , m_preview(PdfPreviewRenderer::create())
    , m_previewAvailable(false)
    , m_modified(false)
    , m_pageList(nullptr)
    , m_statusLabel(nullptr)
    , m_saveAction(nullptr)
    , m_saveAsAction(nullptr)
    , m_insertBeforeAction(nullptr)
    , m_insertAfterAction(nullptr)
    , m_deleteAction(nullptr)
    , m_replaceAction(nullptr)
{
    setAcceptDrops(true);
    resize(1100, 720);

    // ---- 页面列表：平铺排布，宽度自适应换行，支持拖动排序（配置见 PageListWidget）----
    m_pageList = new PageListWidget(this);
    connect(m_pageList, &PageListWidget::pagesReordered,
            this, &MainWindow::syncPagesFromList);
    connect(m_pageList, &QListWidget::itemSelectionChanged,
            this, &MainWindow::onSelectionChanged);
    setCentralWidget(m_pageList);

    // ---- 动作 ----
    QStyle *style = this->style();
    const QColor iconColor = palette().color(QPalette::WindowText);
    QAction *openAction = new QAction(style->standardIcon(QStyle::SP_DialogOpenButton),
                                      tr("打开 PDF…"), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    m_saveAction = new QAction(style->standardIcon(QStyle::SP_DialogSaveButton),
                               tr("保存"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveFile);

    m_saveAsAction = new QAction(style->standardIcon(QStyle::SP_DialogSaveButton),
                                 tr("另存为…"), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);

    m_insertBeforeAction = new QAction(tr("在所选页之前"), this);
    m_insertBeforeAction->setShortcut(QKeySequence(QStringLiteral("Alt+1")));
    connect(m_insertBeforeAction, &QAction::triggered, this, &MainWindow::insertBlankBefore);

    m_insertAfterAction = new QAction(tr("在所选页之后"), this);
    m_insertAfterAction->setShortcut(QKeySequence(QStringLiteral("Alt+2")));
    connect(m_insertAfterAction, &QAction::triggered, this, &MainWindow::insertBlankAfter);

    m_deleteAction = new QAction(style->standardIcon(QStyle::SP_TrashIcon),
                                 tr("删除所选页"), this);
    // Alt+Delete 与 Delete 均可删除
    m_deleteAction->setShortcuts(QList<QKeySequence>()
                                 << QKeySequence(QStringLiteral("Alt+Delete"))
                                 << QKeySequence::Delete);
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::deleteSelected);

    m_replaceAction = new QAction(style->standardIcon(QStyle::SP_BrowserReload),
                                  tr("替换所选页…"), this);
    connect(m_replaceAction, &QAction::triggered, this, &MainWindow::replaceSelected);

    QAction *rotateLeftAction = new QAction(makeCircularArrowIcon(true, iconColor),
                                            tr("左转 90°"), this);
    connect(rotateLeftAction, &QAction::triggered, this, [this]() { rotateSelected(-90); });
    QAction *rotateRightAction = new QAction(makeCircularArrowIcon(false, iconColor),
                                             tr("右转 90°"), this);
    connect(rotateRightAction, &QAction::triggered, this, [this]() { rotateSelected(90); });
    QAction *rotate180Action = new QAction(makeRotate180Icon(iconColor),
                                           tr("旋转 180°"), this);
    connect(rotate180Action, &QAction::triggered, this, [this]() { rotateSelected(180); });

    // ---- 菜单栏 ----
    QMenu *fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    fileMenu->addAction(openAction);
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("退出(&X)"), this, &MainWindow::close);

    QMenu *pageMenu = menuBar()->addMenu(tr("页面(&P)"));
    pageMenu->addAction(m_insertBeforeAction);
    pageMenu->addAction(m_insertAfterAction);
    pageMenu->addSeparator();
    pageMenu->addAction(m_deleteAction);
    pageMenu->addAction(m_replaceAction);
    pageMenu->addSeparator();
    pageMenu->addAction(rotateLeftAction);
    pageMenu->addAction(rotateRightAction);
    pageMenu->addAction(rotate180Action);
    pageMenu->addSeparator();
    pageMenu->addAction(tr("提示：直接拖动缩略图可调整页面顺序"));

    // ---- 工具栏 ----
    QToolBar *toolbar = addToolBar(tr("主工具栏"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(20, 20));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar->addAction(openAction);
    toolbar->addAction(m_saveAction);
    toolbar->addAction(m_saveAsAction);
    toolbar->addSeparator();

    QToolButton *insertButton = new QToolButton(this);
    insertButton->setText(tr("插入空白页"));
    insertButton->setIcon(style->standardIcon(QStyle::SP_FileIcon));
    insertButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    insertButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *insertMenu = new QMenu(insertButton);
    insertMenu->addAction(m_insertBeforeAction);
    insertMenu->addAction(m_insertAfterAction);
    insertButton->setMenu(insertMenu);
    toolbar->addWidget(insertButton);

    toolbar->addAction(m_deleteAction);
    toolbar->addAction(m_replaceAction);
    toolbar->addSeparator();
    toolbar->addAction(rotateLeftAction);
    toolbar->addAction(rotateRightAction);
    toolbar->addAction(rotate180Action);

    // ---- 状态栏 ----
    m_statusLabel = new QLabel(this);
    statusBar()->addWidget(m_statusLabel, 1);

    m_thumbTimer = new QTimer(this);
    m_thumbTimer->setSingleShot(true);
    m_thumbTimer->setInterval(0);
    connect(m_thumbTimer, &QTimer::timeout, this, &MainWindow::renderNextThumbnail);

    updateActions();
    updateStatus();
    setWindowTitle(tr("AuPDF页面编辑器"));
}

MainWindow::~MainWindow()
{
    delete m_preview;
    delete m_doc;
}

void MainWindow::openPath(const QString &path)
{
    if (m_doc->isLoaded() && m_modified && !maybeSave())
        return;
    loadDocument(path);
}

// ---------------------------------------------------------------- 打开

void MainWindow::openFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("打开 PDF 文件"),
                                                QString(), tr("PDF 文件 (*.pdf)"));
    if (path.isEmpty())
        return;
    openPath(path);
}

bool MainWindow::loadDocument(const QString &path)
{
    PdfPageEditor *doc = new PdfPageEditor;
    QString error;
    if (!doc->load(path, &error)) {
        delete doc;
        QMessageBox::warning(this, tr("打开 PDF 文件"), error);
        return false;
    }
    delete m_doc;
    m_doc = doc;
    m_filePath = path;
    setModified(false);
    refreshPreviewFromDisk();
    rebuildPageList();
    updateActions();
    updateStatus();
    updateWindowTitle();
    return true;
}

// ---------------------------------------------------------------- 保存

void MainWindow::saveFile()
{
    if (!m_doc->isLoaded())
        return;
    if (m_filePath.isEmpty()) {
        saveFileAs();
        return;
    }
    doSave(m_filePath);
}

void MainWindow::saveFileAs()
{
    if (!m_doc->isLoaded())
        return;
    QString path = QFileDialog::getSaveFileName(this, tr("另存为"),
                                                m_filePath, tr("PDF 文件 (*.pdf)"));
    if (path.isEmpty())
        return;
    // 先更新路径，保证 doSave 内的预览刷新读取的是新文件
    QString oldPath = m_filePath;
    m_filePath = path;
    if (!doSave(path)) {
        m_filePath = oldPath;
        return;
    }
    updateStatus();
    updateWindowTitle();
}

bool MainWindow::doSave(const QString &path)
{
    QString error;
    if (!m_doc->saveAs(path, &error)) {
        QMessageBox::critical(this, tr("保存"), error);
        return false;
    }
    setModified(false);
    statusBar()->showMessage(tr("已保存：%1").arg(QFileInfo(path).fileName()), 4000);
    // 保存后预览内容可能已过期（被换/被改的页），从磁盘重新打开预览
    refreshPreviewFromDisk();
    rebuildPageList();
    return true;
}

bool MainWindow::maybeSave()
{
    if (!m_modified)
        return true;
    QMessageBox::StandardButton ret = QMessageBox::warning(this, tr("未保存的修改"),
            tr("文档已修改，是否保存？"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Save) {
        saveFile();
        return !m_modified;
    }
    return ret == QMessageBox::Discard;
}

// ---------------------------------------------------------------- 页操作

void MainWindow::insertBlankBefore()
{
    if (!m_doc->isLoaded())
        return;
    int current = m_pageList->currentRow();
    doInsertBlank(current >= 0 ? current : 0);
}

void MainWindow::insertBlankAfter()
{
    if (!m_doc->isLoaded())
        return;
    int current = m_pageList->currentRow();
    doInsertBlank(current >= 0 ? current + 1 : m_doc->pageCount());
}

void MainWindow::doInsertBlank(int newIndex)
{
    QString error;
    QSizeF size = chooseBlankPageSize(newIndex);
    if (!m_doc->insertBlankPage(newIndex, size, &error)) {
        QMessageBox::warning(this, tr("插入空白页"), error);
        return;
    }
    setModified(true);
    remapPreviewAfterInsert(newIndex);
    rebuildPageList();
    m_pageList->setCurrentRow(newIndex);
    updateStatus();
}

void MainWindow::deleteSelected()
{
    if (!m_doc->isLoaded())
        return;
    QList<int> rows = selectedPageRows();
    if (rows.isEmpty())
        return;
    if (QMessageBox::question(this, tr("删除页面"),
                              tr("确定删除选中的 %1 页吗？").arg(rows.size()))
            != QMessageBox::Yes)
        return;
    QString error;
    if (!m_doc->deletePages(rows, &error)) {
        QMessageBox::warning(this, tr("删除页面"), error);
        return;
    }
    setModified(true);
    remapPreviewAfterDelete(rows.toVector());
    rebuildPageList();
    int target = qMin(rows.first(), m_doc->pageCount() - 1);
    m_pageList->setCurrentRow(target);
    updateStatus();
}

void MainWindow::replaceSelected()
{
    if (!m_doc->isLoaded() || m_pageList->currentRow() < 0)
        return;
    QString path = QFileDialog::getOpenFileName(this, tr("选择用于替换的 PDF 文件"),
                                                QString(), tr("PDF 文件 (*.pdf)"));
    if (path.isEmpty())
        return;
    PdfPageEditor sourceDoc;
    QString error;
    if (!sourceDoc.load(path, &error)) {
        QMessageBox::warning(this, tr("替换页面"), error);
        return;
    }
    PagePickerDialog picker(&sourceDoc, this);
    if (picker.exec() != QDialog::Accepted || picker.selectedPage() < 0)
        return;
    int target = m_pageList->currentRow();
    // 替换前先把源页面渲染成缩略图，替换后立即可显示（无需等待保存）
    QImage replacedImage = picker.renderSelectedPage(m_pageList->iconSize());
    if (!m_doc->replacePage(target, &sourceDoc, picker.selectedPage(), &error)) {
        QMessageBox::warning(this, tr("替换页面"), error);
        return;
    }
    setModified(true);
    remapPreviewAfterReplace(target, replacedImage);
    rebuildPageList();
    m_pageList->setCurrentRow(target);
    updateStatus();
}

// 新空白页的尺寸：优先继承前一页，其次继承后一页，最后按 A4
QSizeF MainWindow::chooseBlankPageSize(int newIndex) const
{
    int count = m_doc->pageCount();
    if (newIndex - 1 >= 0 && newIndex - 1 < count) {
        QRectF box = m_doc->pageMediaBox(newIndex - 1);
        return QSizeF(box.width(), box.height());
    }
    if (newIndex >= 0 && newIndex < count) {
        QRectF box = m_doc->pageMediaBox(newIndex);
        return QSizeF(box.width(), box.height());
    }
    return QSizeF(595, 842);
}

// 当前选中的页行号（升序）
QList<int> MainWindow::selectedPageRows() const
{
    QList<int> rows;
    for (int i = 0; i < m_pageList->count(); ++i) {
        if (m_pageList->item(i)->isSelected())
            rows.append(i);
    }
    return rows;
}

// ---------------------------------------------------------------- 旋转

void MainWindow::rotateSelected(int deltaDegrees)
{
    if (!m_doc->isLoaded())
        return;
    QList<int> rows = selectedPageRows();
    if (rows.isEmpty())
        return;
    QString error;
    if (!m_doc->rotatePages(rows, deltaDegrees, &error)) {
        QMessageBox::warning(this, tr("旋转页面"), error);
        return;
    }
    // 已渲染的真实缩略图直接同步旋转（pdfium 按 /Rotate 渲染，结果一致），
    // 占位图不随旋转变化（不含方向信息）
    const QSize box = m_pageList->iconSize();
    for (int i = 0; i < rows.size(); ++i) {
        int index = rows.at(i);
        if (index >= m_previewIcons.size() || !m_previewRendered.at(index)
                || m_previewIcons.at(index).isNull())
            continue;
        QImage image = m_previewIcons.at(index).pixmap(box).toImage();
        QTransform transform;
        transform.rotate(deltaDegrees);
        m_previewIcons[index] = QIcon(QPixmap::fromImage(
            PdfPreviewRenderer::composePageImage(image.transformed(transform), box)));
    }
    setModified(true);
    rebuildPageList(); // 页号与视觉尺寸文本随之更新
    for (int i = 0; i < rows.size(); ++i) {
        if (m_pageList->item(rows.at(i)))
            m_pageList->item(rows.at(i))->setSelected(true);
    }
    updateStatus();
}

// ---------------------------------------------------------------- 拖动排序

// 拖放完成后把列表新顺序同步回引擎，并按相同排列平移预览缓存
void MainWindow::syncPagesFromList()
{
    if (!m_doc->isLoaded())
        return;
    const int count = m_pageList->count();
    if (count != m_doc->pageCount())
        return;
    QVector<int> order;
    order.reserve(count);
    for (int i = 0; i < count; ++i)
        order.append(m_pageList->item(i)->data(Qt::UserRole).toInt());
    bool valid = true;
    QSet<int> seen;
    for (int i = 0; i < order.size(); ++i) {
        if (order.at(i) < 0 || order.at(i) >= count || seen.contains(order.at(i))) {
            valid = false;
            break;
        }
        seen.insert(order.at(i));
    }
    if (!valid) {
        rebuildPageList();
        return;
    }
    bool identity = true;
    for (int i = 0; i < order.size(); ++i) {
        if (order.at(i) != i) {
            identity = false;
            break;
        }
    }
    if (identity)
        return;

    // 记住拖动前选中的页（旧索引），重排后按原页恢复选中
    QSet<int> selectedOld;
    for (int i = 0; i < count; ++i) {
        if (m_pageList->item(i)->isSelected())
            selectedOld.insert(m_pageList->item(i)->data(Qt::UserRole).toInt());
    }

    QString error;
    if (!m_doc->setPageOrder(order, &error)) {
        QMessageBox::warning(this, tr("调整页面顺序"), error);
        rebuildPageList();
        return;
    }
    if (m_previewSource.size() == count) {
        QVector<int> src;
        QVector<QIcon> icons;
        QVector<bool> rendered;
        src.reserve(count);
        icons.reserve(count);
        rendered.reserve(count);
        for (int i = 0; i < count; ++i) {
            src.append(m_previewSource.at(order.at(i)));
            icons.append(m_previewIcons.at(order.at(i)));
            rendered.append(m_previewRendered.at(order.at(i)));
        }
        m_previewSource = src;
        m_previewIcons = icons;
        m_previewRendered = rendered;
    }
    setModified(true);
    rebuildPageList();
    for (int i = 0; i < count; ++i) {
        if (selectedOld.contains(order.at(i)) && m_pageList->item(i))
            m_pageList->item(i)->setSelected(true);
    }
    updateStatus();
}

// ---------------------------------------------------------------- 界面刷新

void MainWindow::rebuildPageList()
{
    m_pageList->clear();
    for (int i = 0; i < m_doc->pageCount(); ++i) {
        new QListWidgetItem(m_pageList);
        applyItemAppearance(i);
    }
    scheduleThumbnails();
}

void MainWindow::applyItemAppearance(int index)
{
    QListWidgetItem *item = m_pageList->item(index);
    if (!item)
        return;
    item->setData(Qt::UserRole, index); // 页面身份标识，拖动排序时用于还原顺序
    QSizeF visual = m_doc->pageVisualSize(index);
    item->setText(tr("第 %1 页\n%2 × %3 pt").arg(index + 1)
                  .arg(qRound(visual.width())).arg(qRound(visual.height())));
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
    if (index < m_previewIcons.size() && !m_previewIcons.at(index).isNull()
            && index < m_previewRendered.size() && m_previewRendered.at(index)) {
        item->setIcon(m_previewIcons.at(index));
        return;
    }
    item->setIcon(pagePlaceholderIcon(index + 1));
}

// 无渲染后端时的占位页图标（绘图实现在 PdfPreviewRenderer::placeholderImage）
QIcon MainWindow::pagePlaceholderIcon(int pageNumber) const
{
    return QIcon(QPixmap::fromImage(
        PdfPreviewRenderer::placeholderImage(pageNumber, m_pageList->iconSize())));
}

void MainWindow::updateActions()
{
    bool loaded = m_doc->isLoaded();
    m_saveAction->setEnabled(loaded);
    m_saveAsAction->setEnabled(loaded);
    m_insertBeforeAction->setEnabled(loaded);
    m_insertAfterAction->setEnabled(loaded);
    bool hasSelection = loaded && !m_pageList->selectedItems().isEmpty();
    m_deleteAction->setEnabled(hasSelection);
    m_replaceAction->setEnabled(hasSelection);
}

void MainWindow::updateStatus()
{
    if (!m_doc->isLoaded())
        m_statusLabel->setText(tr("将 PDF 文件拖入窗口，或点击“打开 PDF”"));
    else if (!m_preview)
        m_statusLabel->setText(tr("%1  ·  共 %2 页  ·  预览不可用（将 pdfium.dll 放到程序目录可启用）")
                               .arg(QDir::toNativeSeparators(m_filePath))
                               .arg(m_doc->pageCount()));
    else
        m_statusLabel->setText(tr("%1  ·  共 %2 页  ·  预览：%3")
                               .arg(QDir::toNativeSeparators(m_filePath))
                               .arg(m_doc->pageCount())
                               .arg(m_preview->backendName()));
}

void MainWindow::onSelectionChanged()
{
    updateActions();
}

void MainWindow::setModified(bool modified)
{
    m_modified = modified;
    setWindowModified(m_modified);
}

void MainWindow::updateWindowTitle()
{
    if (m_filePath.isEmpty())
        setWindowTitle(tr("AuPDF页面编辑器"));
    else
        setWindowTitle(tr("%1[*] - AuPDF页面编辑器")
                       .arg(QFileInfo(m_filePath).fileName()));
}

// ---------------------------------------------------------------- 事件

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls())
        return;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (int i = 0; i < urls.size(); ++i) {
        if (urls.at(i).toLocalFile().endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (int i = 0; i < urls.size(); ++i) {
        QString local = urls.at(i).toLocalFile();
        if (local.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
            openPath(local);
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::scheduleThumbnails()
{
    if (m_previewAvailable)
        m_thumbTimer->start();
}

void MainWindow::renderNextThumbnail()
{
    if (!m_doc->isLoaded())
        return;
    for (int i = 0; i < m_previewSource.size(); ++i) {
        int sourcePage = m_previewSource.at(i);
        if (sourcePage < 0 || m_previewRendered.at(i))
            continue;
        if (m_preview && sourcePage < m_preview->pageCount()) {
            QImage image = m_preview->renderPage(sourcePage, m_pageList->iconSize());
            if (!image.isNull()) {
                m_previewIcons[i] = QIcon(QPixmap::fromImage(image));
                m_previewRendered[i] = true;
                applyItemAppearance(i); // 用真实缩略图替换占位图并刷新页号文本
                m_thumbTimer->start();  // 逐页渲染，保持界面响应
                return;
            }
        }
        m_previewRendered[i] = true; // 该页渲染失败：保持占位图，不再重试
        m_thumbTimer->start();
        return;
    }
}

void MainWindow::remapPreviewAfterInsert(int newIndex)
{
    QVector<int> source;
    QVector<QIcon> icons;
    QVector<bool> rendered;
    int oldCount = m_previewSource.size();
    for (int i = 0; i <= oldCount; ++i) {
        if (i == newIndex) {
            source.append(-1);
            icons.append(QIcon());
            rendered.append(false);
        } else {
            int old = i < newIndex ? i : i - 1;
            source.append(m_previewSource.at(old));
            icons.append(m_previewIcons.at(old));
            rendered.append(m_previewRendered.at(old));
        }
    }
    m_previewSource = source;
    m_previewIcons = icons;
    m_previewRendered = rendered;
    // 空白页的真实内容就是纯白，无需等待保存即可显示
    QImage blank = PdfPreviewRenderer::blankPageImage(m_pageList->iconSize());
    m_previewIcons[newIndex] = QIcon(QPixmap::fromImage(blank));
    m_previewRendered[newIndex] = true;
}

void MainWindow::remapPreviewAfterDelete(const QVector<int> &deletedIndexes)
{
    QSet<int> deleted;
    for (int i = 0; i < deletedIndexes.size(); ++i)
        deleted.insert(deletedIndexes.at(i));
    QVector<int> source;
    QVector<QIcon> icons;
    QVector<bool> rendered;
    for (int i = 0; i < m_previewSource.size(); ++i) {
        if (deleted.contains(i))
            continue;
        source.append(m_previewSource.at(i));
        icons.append(m_previewIcons.at(i));
        rendered.append(m_previewRendered.at(i));
    }
    m_previewSource = source;
    m_previewIcons = icons;
    m_previewRendered = rendered;
}

void MainWindow::remapPreviewAfterReplace(int index, const QImage &pageImage)
{
    // 被替换页的磁盘预览已过期：直接使用替换前从源文档渲染的图像，
    // 保存后再由 refreshPreviewFromDisk 从磁盘重建
    m_previewSource[index] = -1;
    m_previewIcons[index] = pageImage.isNull()
            ? QIcon() : QIcon(QPixmap::fromImage(pageImage));
    m_previewRendered[index] = !pageImage.isNull();
}

void MainWindow::refreshPreviewFromDisk()
{
    m_previewAvailable = false;
    if (m_preview && !m_filePath.isEmpty())
        m_previewAvailable = m_preview->open(m_filePath);
    m_previewSource.clear();
    m_previewIcons.clear();
    m_previewRendered.clear();
    for (int i = 0; i < m_doc->pageCount(); ++i) {
        m_previewSource.append(m_previewAvailable ? i : -1);
        m_previewIcons.append(QIcon());
        m_previewRendered.append(false);
    }
}
