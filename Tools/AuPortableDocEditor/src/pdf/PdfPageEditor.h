#ifndef PDFPAGEEDITOR_H
#define PDFPAGEEDITOR_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QRectF>
#include <QSet>
#include <QSizeF>
#include <QString>
#include <QVector>

#include "PdfObject.h"
#include "PdfParser.h"

// PDF 页面编辑器：装载 PDF 并展开页树（继承属性下拷到每页），
// 支持插入空白页、删除页、从其它文档替换页，保存时重建单层页树
// 并整体重写文件。纯逻辑类，不依赖 Qt 对象系统。
class PdfPageEditor
{
public:
    PdfPageEditor();

    bool load(const QString &path, QString *errorMessage);
    bool saveAs(const QString &path, QString *errorMessage);

    bool isLoaded() const { return m_loaded; }
    QString filePath() const { return m_filePath; }
    int pageCount() const { return m_pages.size(); }
    // 页面尺寸（pt，MediaBox），无 MediaBox 时按 A4 兜底
    QRectF pageMediaBox(int index) const;

    // 在 index（0..pageCount）位置插入一页空白页，sizePt 为页面尺寸
    bool insertBlankPage(int index, const QSizeF &sizePt, QString *errorMessage);
    // 删除若干页（自动去重；不允许删掉最后一页）
    bool deletePages(const QList<int> &indices, QString *errorMessage);
    // 用 source 文档的 sourceIndex 页深拷贝替换本档 targetIndex 页
    bool replacePage(int targetIndex, const PdfPageEditor *source, int sourceIndex,
                     QString *errorMessage);
    // 重排页面：order[k] = 重排后第 k 页对应的原页面下标（必须为 0..n-1 的排列）
    bool setPageOrder(const QVector<int> &order, QString *errorMessage);
    // 旋转若干页（deltaDegrees 为 90 的倍数，正值顺时针；仅设置 /Rotate，无损）
    bool rotatePages(const QList<int> &indices, int deltaDegrees, QString *errorMessage);

    // 页面有效 /Rotate（0/90/180/270；装载时继承属性已下拷到每页）
    int pageRotation(int index) const;
    // 计入旋转后的视觉尺寸（旋转 90/270 时宽高互换），用于界面显示
    QSizeF pageVisualSize(int index) const;

private:
    bool walkPageTree(const PdfObject &pagesDict,
                      const QVector<QPair<QByteArray, PdfObject> > &inherited,
                      int depth, QSet<int> *visited);
    // 把 value 中引用的源文档对象深拷贝进本文档，返回重映射后的副本
    PdfObject copyFromSource(const PdfPageEditor *source, const PdfObject &value,
                             int rootPageNum, bool isPageRoot,
                             QHash<int, int> *mapping, int depth);
    int allocateObjectNumber();

    PdfFileData m_file;
    QVector<PdfObject> m_pages;    // 各页字典（继承属性已下拷）
    QVector<int> m_pageObjectNums; // 各页装载时的对象号（-1 表示新建/替换的页）
    QString m_filePath;
    bool m_loaded;
    int m_nextFree; // 下一个可分配的新对象号
};

#endif // PDFPAGEEDITOR_H
