#ifndef PAGEPICKERDIALOG_H
#define PAGEPICKERDIALOG_H

#include <QDialog>
#include <QImage>
#include <QSize>

class QListWidget;
class QTimer;
class PdfPageEditor;
class PdfPreviewRenderer;

// 从源 PDF 中挑选一页（“替换页”操作的第二步）。
// 所有页面以平铺缩略图展示（与主窗口一致，宽度自适应换行），
// 双击某一页即完成选择；窗口标题为源文件名。
class PagePickerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PagePickerDialog(const PdfPageEditor *source, QWidget *parent = nullptr);
    ~PagePickerDialog();

    int selectedPage() const; // 0 基页号，未选择时为 -1

    // 渲染当前选中页（供替换后立即显示缩略图），无后端时返回空图像
    QImage renderSelectedPage(const QSize &box) const;

private slots:
    void renderNextThumbnail();

private:
    const PdfPageEditor *m_source;
    int m_selectedPage;
    int m_nextToRender;
    QListWidget *m_pageList;
    PdfPreviewRenderer *m_preview; // 源文档预览用，可能为空
    QTimer *m_thumbTimer;
};

#endif // PAGEPICKERDIALOG_H
