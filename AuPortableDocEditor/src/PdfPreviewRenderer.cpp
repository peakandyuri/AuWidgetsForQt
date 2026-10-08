#include "PdfPreviewRenderer.h"

#include <QCoreApplication>
#include <QFile>
#include <QIODevice>
#include <QDir>
#include <QLibrary>
#include <QPainter>
#include <QPixmap>
#include <QRect>

#include <math.h>

PdfPreviewRenderer::~PdfPreviewRenderer()
{
}

QImage PdfPreviewRenderer::composePageImage(const QImage &pageImage, const QSize &box)
{
    if (box.isEmpty())
        return QImage();
    QImage result(box, QImage::Format_RGB32);
    result.fill(Qt::white);
    if (pageImage.isNull())
        return result;
    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    // 页面四周留 1px 空隙，边框画在页面外侧
    QRect target = QRect(QPoint(0, 0), pageImage.size());
    target.setSize(target.size().scaled(box - QSize(2, 2), Qt::KeepAspectRatio));
    target.moveCenter(QRect(QPoint(0, 0), box).center());
    painter.drawImage(target, pageImage);
    painter.setPen(QColor(190, 190, 190));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(target.adjusted(0, 0, -1, -1));
    painter.end();
    return result;
}

QImage PdfPreviewRenderer::blankPageImage(const QSize &box)
{
    if (box.isEmpty())
        return QImage();
    QImage result(box, QImage::Format_RGB32);
    result.fill(Qt::white);
    QPainter painter(&result);
    painter.setPen(QColor(190, 190, 190));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(0, 0, box.width() - 1, box.height() - 1);
    painter.end();
    return result;
}

QImage PdfPreviewRenderer::placeholderImage(int pageNumber, const QSize &box)
{
    QPixmap pixmap(box);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(160, 160, 160), 1.5));
    painter.setBrush(QColor(250, 250, 250));
    painter.drawRoundedRect(20, 8, box.width() - 40, box.height() - 32, 6, 6);
    painter.setPen(QPen(QColor(205, 205, 205), 5, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(38, 42, box.width() - 38, 42);
    painter.drawLine(38, 62, box.width() - 38, 62);
    painter.drawLine(38, 82, box.width() - 50, 82);
    painter.setPen(QColor(130, 130, 130));
    painter.drawText(QRect(20, box.height() - 62, box.width() - 40, 30),
                     Qt::AlignCenter, QString::number(pageNumber));
    painter.end();
    return pixmap.toImage();
}

#ifdef AUPDE_WITH_PREVIEW

#include <QPdfDocument>

// 后端一：QtPdf 模块（Qt 自带，QPdfDocument 只读渲染）
class QtPdfPreviewRenderer : public PdfPreviewRenderer
{
public:
    QString backendName() const { return QStringLiteral("QtPdf"); }

    bool open(const QString &path)
    {
        m_doc.close();
        return m_doc.load(path) == QPdfDocument::NoError;
    }
    int pageCount() const { return m_doc.pageCount(); }

    QImage renderPage(int index, const QSize &box) override
    {
        if (index < 0 || index >= m_doc.pageCount())
            return QImage();
        QSizeF pt = m_doc.pagePointSize(index);
        if (pt.isEmpty())
            return QImage();
        // 2 倍超采样，按页面纵横比整页渲染
        QSize target = QSize(int(pt.width() * 2), int(pt.height() * 2));
        target.scale(box * 2, Qt::KeepAspectRatio);
        QImage image = m_doc.render(index, target, QPdfDocumentRenderOptions(),
                                    QImage::Format_RGB32, Qt::white);
        if (image.isNull())
            return image;
        return composePageImage(image, box);
    }

private:
    QPdfDocument m_doc;
};

#endif // AUPDE_WITH_PREVIEW

// 后端二：pdfium 动态库运行时加载（与 Chrome 同款渲染引擎，纯 C ABI）
class PdfiumPreviewRenderer : public PdfPreviewRenderer
{
public:
    PdfiumPreviewRenderer()
    {
        ++s_instanceCount;
        resolve();
    }
    ~PdfiumPreviewRenderer()
    {
        closeDoc();
        // 多实例共享 pdfium 全局状态，仅最后一个实例析构时才卸载
        if (--s_instanceCount == 0 && m_destroyLibrary)
            m_destroyLibrary();
    }

    QString backendName() const { return QStringLiteral("pdfium"); }

    bool isValid() const { return !m_failed; }

    bool open(const QString &path) override
    {
        if (m_failed)
            return false;
        closeDoc();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        m_data = file.readAll();
        m_doc = m_loadMemDocument(m_data.constData(), uint(m_data.size()), nullptr);
        return m_doc != nullptr;
    }

    int pageCount() const override
    {
        return m_doc ? m_getPageCount(m_doc) : 0;
    }

    QImage renderPage(int index, const QSize &box) override
    {
        if (!m_doc || m_failed || box.isEmpty())
            return QImage();
        void *page = m_loadPage(m_doc, index);
        if (!page)
            return QImage();
        double pageW = m_getPageWidthF(page);
        double pageH = m_getPageHeightF(page);
        QImage result;
        if (pageW > 1.0 && pageH > 1.0) {
            // 2 倍超采样：按页面纵横比计算位图尺寸
            double scale = qMin(box.width() * 2.0 / pageW, box.height() * 2.0 / pageH);
            int w = qMax(1, int(pageW * scale));
            int h = qMax(1, int(pageH * scale));
            void *bitmap = m_bitmapCreate(w, h, 3); // FPDFBitmap_BGRx（对应 QImage RGB32）
            if (bitmap) {
                m_bitmapFillRect(bitmap, 0, 0, w, h, 0xFFFFFFFF);
                m_renderPageBitmap(bitmap, page, 0, 0, w, h, 0, 1 /* FPDF_ANNOT */);
                int stride = m_bitmapGetStride(bitmap);
                const uchar *buffer = reinterpret_cast<const uchar *>(m_bitmapGetBuffer(bitmap));
                if (buffer && stride > 0) {
                    QImage raw(buffer, w, h, stride, QImage::Format_RGB32);
                    // 深拷贝脱离 pdfium 缓冲，再合成到目标框
                    result = composePageImage(raw.copy(), box);
                }
                m_bitmapDestroy(bitmap);
            }
        }
        m_closePage(page);
        return result;
    }

private:
    static int s_instanceCount; // 存活实例数（pdfium 全局初始化/卸载用）

    // pdfium C API（句柄一律按 void* 保存，兼容各版本头文件差异）
    typedef void *(*FnLoadMemDocument)(const char *, unsigned long, const char *);
    typedef void (*FnCloseDoc)(void *);
    typedef int (*FnGetLastError)();
    typedef int (*FnGetPageCount)(void *);
    typedef void *(*FnLoadPage)(void *, int);
    typedef void (*FnClosePage)(void *);
    // 注意：FPDF_GetPageWidthF / HeightF 的 F 后缀表示返回 float（非 double）
    typedef float (*FnGetPagePointF)(void *);
    typedef void *(*FnBitmapCreate)(int, int, int);
    typedef void (*FnBitmapDestroy)(void *);
    typedef signed int (*FnBitmapFillRect)(void *, int, int, int, int, unsigned int);
    typedef void (*FnRenderPageBitmap)(void *, void *, int, int, int, int, int, int);
    typedef void *(*FnBitmapGetBuffer)(void *);
    typedef int (*FnBitmapGetStride)(void *);
    typedef void (*FnVoidFn)();

    void resolve()
    {
        // 与 pdfium 官方二进制命名一致；放到程序目录即可启用
        QString base = QCoreApplication::applicationDirPath();
        QStringList candidates;
#ifdef Q_OS_WIN
        candidates << base + QStringLiteral("/pdfium.dll");
#else
        candidates << base + QStringLiteral("/libpdfium.so")
                   << base + QStringLiteral("/libpdfium.so.0");
#endif
        for (int i = 0; i < candidates.size(); ++i) {
            if (QFile::exists(candidates.at(i))) {
                m_library.setFileName(candidates.at(i));
                break;
            }
        }
        if (m_library.fileName().isEmpty())
            m_library.setFileName(QStringLiteral("pdfium")); // 交给系统库搜索路径
        if (!m_library.load()) {
            m_failed = true;
            return;
        }
        m_loadMemDocument = reinterpret_cast<FnLoadMemDocument>(m_library.resolve("FPDF_LoadMemDocument"));
        m_closeDocument = reinterpret_cast<FnCloseDoc>(m_library.resolve("FPDF_CloseDocument"));
        m_getLastError = reinterpret_cast<FnGetLastError>(m_library.resolve("FPDF_GetLastError"));
        m_getPageCount = reinterpret_cast<FnGetPageCount>(m_library.resolve("FPDF_GetPageCount"));
        m_loadPage = reinterpret_cast<FnLoadPage>(m_library.resolve("FPDF_LoadPage"));
        m_closePage = reinterpret_cast<FnClosePage>(m_library.resolve("FPDF_ClosePage"));
        m_getPageWidthF = reinterpret_cast<FnGetPagePointF>(m_library.resolve("FPDF_GetPageWidthF"));
        m_getPageHeightF = reinterpret_cast<FnGetPagePointF>(m_library.resolve("FPDF_GetPageHeightF"));
        m_bitmapCreate = reinterpret_cast<FnBitmapCreate>(m_library.resolve("FPDFBitmap_Create"));
        m_bitmapDestroy = reinterpret_cast<FnBitmapDestroy>(m_library.resolve("FPDFBitmap_Destroy"));
        m_bitmapFillRect = reinterpret_cast<FnBitmapFillRect>(m_library.resolve("FPDFBitmap_FillRect"));
        m_renderPageBitmap = reinterpret_cast<FnRenderPageBitmap>(m_library.resolve("FPDF_RenderPageBitmap"));
        m_bitmapGetBuffer = reinterpret_cast<FnBitmapGetBuffer>(m_library.resolve("FPDFBitmap_GetBuffer"));
        m_bitmapGetStride = reinterpret_cast<FnBitmapGetStride>(m_library.resolve("FPDFBitmap_GetStride"));

        // 任一关键符号缺失即视为后端不可用（避免调用到空指针）
        m_failed = !m_loadMemDocument || !m_closeDocument || !m_getPageCount
                || !m_loadPage || !m_closePage || !m_getPageWidthF || !m_getPageHeightF
                || !m_bitmapCreate || !m_bitmapDestroy || !m_bitmapFillRect
                || !m_renderPageBitmap || !m_bitmapGetBuffer || !m_bitmapGetStride;
        if (m_failed)
            return;

        // 初始化是可选的（老版本必然导出；新版本缺失时 pdfium 会懒加载）
        FnVoidFn initLibrary = reinterpret_cast<FnVoidFn>(m_library.resolve("FPDF_InitLibrary"));
        m_destroyLibrary = reinterpret_cast<FnVoidFn>(m_library.resolve("FPDF_DestroyLibrary"));
        if (initLibrary)
            initLibrary();
    }

    void closeDoc()
    {
        if (m_doc) {
            m_closeDocument(m_doc);
            m_doc = nullptr;
        }
        m_data.clear();
    }

    QLibrary m_library;
    bool m_failed = false;

    FnLoadMemDocument m_loadMemDocument = nullptr;
    FnCloseDoc m_closeDocument = nullptr;
    FnGetLastError m_getLastError = nullptr;
    FnGetPageCount m_getPageCount = nullptr;
    FnLoadPage m_loadPage = nullptr;
    FnClosePage m_closePage = nullptr;
    FnGetPagePointF m_getPageWidthF = nullptr;
    FnGetPagePointF m_getPageHeightF = nullptr;
    FnBitmapCreate m_bitmapCreate = nullptr;
    FnBitmapDestroy m_bitmapDestroy = nullptr;
    FnBitmapFillRect m_bitmapFillRect = nullptr;
    FnRenderPageBitmap m_renderPageBitmap = nullptr;
    FnBitmapGetBuffer m_bitmapGetBuffer = nullptr;
    FnBitmapGetStride m_bitmapGetStride = nullptr;
    FnVoidFn m_destroyLibrary = nullptr;

    void *m_doc = nullptr;
    QByteArray m_data; // FPDF_LoadMemDocument 要求缓冲区保持存活
};

int PdfiumPreviewRenderer::s_instanceCount = 0;

PdfPreviewRenderer *PdfPreviewRenderer::create()
{
#ifdef AUPDE_WITH_PREVIEW
    return new QtPdfPreviewRenderer; // QtPdf 内部同为 pdfium，优先用链接版
#else
    PdfiumPreviewRenderer *renderer = new PdfiumPreviewRenderer;
    if (renderer->isValid())
        return renderer;
    delete renderer;
    return nullptr;
#endif
}
