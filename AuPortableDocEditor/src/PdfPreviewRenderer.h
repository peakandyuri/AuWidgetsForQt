#ifndef PDFPREVIEWRENDERER_H
#define PDFPREVIEWRENDERER_H

#include <QImage>
#include <QSize>
#include <QString>

// 页面预览渲染器：把 PDF 页面渲染成图像，用于缩略图与选页预览。
//
// 后端按优先级自动探测（create()）：
//   1. QtPdf 模块（Qt6 且安装了 QtPdf 时编译启用）
//   2. pdfium 动态库运行时加载（QLibrary，无链接期依赖；
//      Windows 找 pdfium.dll，Linux 找 libpdfium.so，放到程序目录即可）
// 都不可用时 create() 返回空指针，界面退回占位图。
class PdfPreviewRenderer
{
public:
    virtual ~PdfPreviewRenderer();

    static PdfPreviewRenderer *create();

    virtual QString backendName() const = 0;
    virtual bool open(const QString &path) = 0;
    virtual int pageCount() const = 0;
    // 渲染整页：保持纵横比、白底居中缩放到 box（含 1px 页面边框），
    // 内部以 2 倍超采样后平滑缩小。失败返回空图像。
    virtual QImage renderPage(int index, const QSize &box) = 0;

public:
    // 把整页图像合成到 box 大小的白底画布上（居中、平滑缩放、加边框）。
    // 公开给界面层用于同步变换已渲染的缩略图（如旋转）
    static QImage composePageImage(const QImage &pageImage, const QSize &box);
    // 空白页缩略图（纯白 + 页面边框）：插入空白页后无需保存即可显示
    static QImage blankPageImage(const QSize &box);
    // 无渲染后端时的占位页图（纸形 + 内容示意线 + 页码）
    static QImage placeholderImage(int pageNumber, const QSize &box);

protected:
    PdfPreviewRenderer() {}
};

#endif // PDFPREVIEWRENDERER_H
