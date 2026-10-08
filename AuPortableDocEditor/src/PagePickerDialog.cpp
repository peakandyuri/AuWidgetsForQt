#include "PagePickerDialog.h"
#include "PdfPreviewRenderer.h"
#include "pdf/PdfPageEditor.h"

#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QRectF>
#include <QTimer>
#include <QVBoxLayout>

PagePickerDialog::PagePickerDialog(const PdfPageEditor *source, QWidget *parent)
    : QDialog(parent)
    , m_source(source)
    , m_selectedPage(-1)
    , m_nextToRender(0)
    , m_pageList(nullptr)
    , m_preview(PdfPreviewRenderer::create())
    , m_thumbTimer(nullptr)
{
    // 窗口标题 = 源文件名
    const QString name = QFileInfo(m_source->filePath()).fileName();
    setWindowTitle(name.isEmpty() ? tr("选择替换页") : name);
    setModal(true);
    resize(860, 600);

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_pageList = new QListWidget(this);
    // 平铺参数与主窗口一致：左到右换行，宽度自适应
    m_pageList->setViewMode(QListView::IconMode);
    m_pageList->setFlow(QListView::LeftToRight);
    m_pageList->setWrapping(true);
    m_pageList->setResizeMode(QListView::Adjust);
    m_pageList->setMovement(QListView::Static);
    m_pageList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pageList->setIconSize(QSize(140, 180));
    m_pageList->setGridSize(QSize(170, 230));
    m_pageList->setSpacing(8);
    m_pageList->setUniformItemSizes(true);
    m_pageList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pageList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(m_pageList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item) {
                m_selectedPage = m_pageList->row(item);
                accept();
            });
    layout->addWidget(m_pageList, 1);

    bool previewOk = false;
    if (m_preview && !m_source->filePath().isEmpty())
        previewOk = m_preview->open(m_source->filePath());
    if (!previewOk) {
        delete m_preview;
        m_preview = nullptr;
        layout->addWidget(new QLabel(tr("无法渲染页面预览（可将 pdfium.dll 放到程序目录启用），双击页码项仍可选择"), this));
    }

    for (int i = 0; i < m_source->pageCount(); ++i) {
        QListWidgetItem *item = new QListWidgetItem(m_pageList);
        QRectF box = m_source->pageMediaBox(i);
        item->setText(tr("第 %1 页\n%2 × %3 pt").arg(i + 1)
                      .arg(qRound(box.width())).arg(qRound(box.height())));
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setIcon(QIcon(QPixmap::fromImage(
            PdfPreviewRenderer::placeholderImage(i + 1, m_pageList->iconSize()))));
    }

    if (m_preview) {
        m_thumbTimer = new QTimer(this);
        m_thumbTimer->setSingleShot(true);
        m_thumbTimer->setInterval(0);
        connect(m_thumbTimer, &QTimer::timeout, this, &PagePickerDialog::renderNextThumbnail);
        m_thumbTimer->start();
    }
}

PagePickerDialog::~PagePickerDialog()
{
    delete m_preview;
}

int PagePickerDialog::selectedPage() const
{
    return m_selectedPage;
}

QImage PagePickerDialog::renderSelectedPage(const QSize &box) const
{
    if (!m_preview || m_selectedPage < 0)
        return QImage();
    return m_preview->renderPage(m_selectedPage, box);
}

// 逐页异步渲染缩略图，保持界面响应
void PagePickerDialog::renderNextThumbnail()
{
    if (!m_preview)
        return;
    int i = m_nextToRender;
    if (i >= m_pageList->count())
        return;
    QImage image = m_preview->renderPage(i, m_pageList->iconSize());
    if (!image.isNull())
        m_pageList->item(i)->setIcon(QIcon(QPixmap::fromImage(image)));
    m_nextToRender = i + 1;
    if (m_nextToRender < m_pageList->count())
        m_thumbTimer->start();
}
