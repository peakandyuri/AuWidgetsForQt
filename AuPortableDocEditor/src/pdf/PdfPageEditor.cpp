#include "PdfPageEditor.h"

#include "PdfWriter.h"

#include <QDateTime>
#include <QFile>

#include <algorithm>
#include <functional>

namespace {

int maxObjectNumber(const QHash<int, PdfObject> &objects)
{
    int maxNum = 0;
    for (QHash<int, PdfObject>::const_iterator it = objects.constBegin();
         it != objects.constEnd(); ++it) {
        if (it.key() > maxNum)
            maxNum = it.key();
    }
    return maxNum;
}

// 页树节点间可继承的属性（PDF 规范 7.7.3.4）
const char *const kInheritableKeys[] = { "MediaBox", "CropBox", "Resources", "Rotate" };
const int kInheritableKeyCount = 4;

void setInherited(QVector<QPair<QByteArray, PdfObject> > *inherited,
                  const QByteArray &key, const PdfObject &value)
{
    for (int i = 0; i < inherited->size(); ++i) {
        if (inherited->at(i).first == key) {
            (*inherited)[i].second = value;
            return;
        }
    }
    inherited->append(qMakePair(key, value));
}

} // namespace

PdfPageEditor::PdfPageEditor()
    : m_loaded(false)
    , m_nextFree(1)
{
}

bool PdfPageEditor::load(const QString &path, QString *errorMessage)
{
    m_loaded = false;
    m_pages.clear();
    m_pageObjectNums.clear();
    m_file = PdfFileData();
    m_filePath.clear();
    m_nextFree = 1;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法打开文件：%1").arg(file.errorString());
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    PdfParser parser;
    if (!parser.load(data, m_file, errorMessage))
        return false;

    // ---- 展开页树 ----
    PdfObject catalog = m_file.objects.value(m_file.rootNum);
    if (!catalog.isDictionary()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("文档目录对象不是字典");
        return false;
    }
    PdfObject pagesRef = catalog.dictGet("Pages");
    if (!pagesRef.isReference() || !m_file.objects.contains(pagesRef.refObjectNumber())) {
        if (errorMessage)
            *errorMessage = QStringLiteral("文档缺少页树（/Pages）");
        return false;
    }
    QSet<int> visited;
    QVector<QPair<QByteArray, PdfObject> > inherited;
    if (!walkPageTree(m_file.objects.value(pagesRef.refObjectNumber()), inherited, 0, &visited)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("页树解析失败");
        return false;
    }
    if (m_pages.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("文档没有可编辑的页面");
        return false;
    }
    m_nextFree = maxObjectNumber(m_file.objects) + 1;
    m_filePath = path;
    m_loaded = true;
    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool PdfPageEditor::walkPageTree(const PdfObject &pagesDict,
                                 const QVector<QPair<QByteArray, PdfObject> > &inherited,
                                 int depth, QSet<int> *visited)
{
    if (depth > 32)
        return false;
    QVector<QPair<QByteArray, PdfObject> > merged = inherited;
    for (int k = 0; k < kInheritableKeyCount; ++k) {
        if (pagesDict.hasKey(kInheritableKeys[k]))
            setInherited(&merged, kInheritableKeys[k], pagesDict.dictGet(kInheritableKeys[k]));
    }
    PdfObject kids = pagesDict.dictGet("Kids");
    if (!kids.isArray())
        return false;
    for (int i = 0; i < kids.arraySize(); ++i) {
        PdfObject kid = kids.arrayAt(i);
        if (!kid.isReference())
            continue;
        int num = kid.refObjectNumber();
        if (visited->contains(num) || !m_file.objects.contains(num))
            continue;
        PdfObject node = m_file.objects.value(num);
        if (!node.isDictionary())
            continue;
        visited->insert(num);
        if (node.dictGet("Kids").isArray()) {
            // 中间页树节点
            if (!walkPageTree(node, merged, depth + 1, visited))
                return false;
        } else {
            // 页面对象：把可继承属性下拷，扁平保存
            PdfObject page = node;
            for (int k = 0; k < merged.size(); ++k) {
                if (!page.hasKey(merged.at(k).first))
                    page.dictSet(merged.at(k).first, merged.at(k).second);
            }
            m_pages.append(page);
            m_pageObjectNums.append(num);
        }
    }
    return true;
}

QRectF PdfPageEditor::pageMediaBox(int index) const
{
    if (index < 0 || index >= m_pages.size())
        return QRectF();
    PdfObject box = m_pages.at(index).dictGet("MediaBox");
    if (box.isArray() && box.arraySize() >= 4) {
        double x0 = box.arrayAt(0).toNumber();
        double y0 = box.arrayAt(1).toNumber();
        double x1 = box.arrayAt(2).toNumber();
        double y1 = box.arrayAt(3).toNumber();
        return QRectF(qMin(x0, x1), qMin(y0, y1), qAbs(x1 - x0), qAbs(y1 - y0));
    }
    return QRectF(0, 0, 595, 842); // A4 兜底
}

bool PdfPageEditor::insertBlankPage(int index, const QSizeF &sizePt, QString *errorMessage)
{
    if (!m_loaded) {
        if (errorMessage)
            *errorMessage = QStringLiteral("尚未打开文档");
        return false;
    }
    if (index < 0)
        index = 0;
    if (index > m_pages.size())
        index = m_pages.size();
    double w = sizePt.width() > 1.0 ? sizePt.width() : 595.0;
    double h = sizePt.height() > 1.0 ? sizePt.height() : 842.0;

    // 白色填充的内容流，保证任何阅读器里都呈现为纯白空页
    QByteArray content;
    content += "1 g\n0 0 ";
    content += QByteArray::number(w);
    content += ' ';
    content += QByteArray::number(h);
    content += " re f\n";
    int contentNum = allocateObjectNumber();
    m_file.objects.insert(contentNum, PdfObject::stream(PdfObject::dictionary(), content));

    PdfObject mediaBox = PdfObject::array();
    mediaBox.arrayAppend(PdfObject::integer(0));
    mediaBox.arrayAppend(PdfObject::integer(0));
    mediaBox.arrayAppend(PdfObject::real(w));
    mediaBox.arrayAppend(PdfObject::real(h));

    PdfObject page = PdfObject::dictionary();
    page.dictSet("Type", PdfObject::name("Page"));
    page.dictSet("MediaBox", mediaBox);
    page.dictSet("Resources", PdfObject::dictionary());
    page.dictSet("Contents", PdfObject::reference(contentNum, 0));

    m_pages.insert(index, page);
    m_pageObjectNums.insert(index, -1);
    return true;
}

bool PdfPageEditor::deletePages(const QList<int> &indices, QString *errorMessage)
{
    if (!m_loaded) {
        if (errorMessage)
            *errorMessage = QStringLiteral("尚未打开文档");
        return false;
    }
    QSet<int> seen;
    QList<int> sorted;
    for (int i = 0; i < indices.size(); ++i) {
        int index = indices.at(i);
        if (index < 0 || index >= m_pages.size() || seen.contains(index))
            continue;
        seen.insert(index);
        sorted.append(index);
    }
    if (sorted.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("未选择要删除的页面");
        return false;
    }
    if (m_pages.size() - sorted.size() < 1) {
        if (errorMessage)
            *errorMessage = QStringLiteral("不能删除文档的全部页面");
        return false;
    }
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int i = 0; i < sorted.size(); ++i) {
        m_pages.removeAt(sorted.at(i));
        m_pageObjectNums.removeAt(sorted.at(i));
    }
    return true;
}

bool PdfPageEditor::replacePage(int targetIndex, const PdfPageEditor *source,
                                int sourceIndex, QString *errorMessage)
{
    if (!m_loaded || !source || !source->m_loaded) {
        if (errorMessage)
            *errorMessage = QStringLiteral("源文档尚未装载");
        return false;
    }
    if (targetIndex < 0 || targetIndex >= m_pages.size()
            || sourceIndex < 0 || sourceIndex >= source->m_pages.size()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("页码超出范围");
        return false;
    }
    QHash<int, int> mapping;
    PdfObject copied = copyFromSource(source, source->m_pages.at(sourceIndex),
                                      source->m_pageObjectNums.at(sourceIndex),
                                      true, &mapping, 0);
    copied.dictSet("Type", PdfObject::name("Page"));
    m_pages[targetIndex] = copied;
    m_pageObjectNums[targetIndex] = -1;
    return true;
}

PdfObject PdfPageEditor::copyFromSource(const PdfPageEditor *source, const PdfObject &value,
                                        int rootPageNum, bool isPageRoot,
                                        QHash<int, int> *mapping, int depth)
{
    if (depth > 64)
        return PdfObject();
    switch (value.type()) {
    case PdfObject::Reference: {
        int srcNum = value.refObjectNumber();
        if (srcNum == rootPageNum)
            return PdfObject(); // 指回源页面自身（如注释的 /P）：丢弃
        if (mapping->contains(srcNum))
            return PdfObject::reference(mapping->value(srcNum), 0);
        if (!source->m_file.objects.contains(srcNum))
            return PdfObject(); // 悬空引用
        int newNum = allocateObjectNumber();
        mapping->insert(srcNum, newNum); // 先登记再递归，防环
        PdfObject copy = copyFromSource(source, source->m_file.objects.value(srcNum),
                                        rootPageNum, false, mapping, depth + 1);
        m_file.objects.insert(newNum, copy);
        return PdfObject::reference(newNum, 0);
    }
    case PdfObject::Array: {
        PdfObject out = PdfObject::array();
        for (int i = 0; i < value.arraySize(); ++i)
            out.arrayAppend(copyFromSource(source, value.arrayAt(i), rootPageNum,
                                           false, mapping, depth + 1));
        return out;
    }
    case PdfObject::Dictionary:
    case PdfObject::Stream: {
        PdfObject out = value.isStream()
                ? PdfObject::stream(PdfObject::dictionary(), value.streamData())
                : PdfObject::dictionary();
        for (int i = 0; i < value.dictSize(); ++i) {
            const QByteArray &key = value.dict().at(i).first;
            if (isPageRoot && key == "Parent")
                continue; // 页的归属由保存时重建的页树决定
            if (key == "P") {
                PdfObject p = value.dict().at(i).second;
                if (p.isReference() && p.refObjectNumber() == rootPageNum)
                    continue; // 注释回指源页面
            }
            out.dictSet(key, copyFromSource(source, value.dict().at(i).second,
                                            rootPageNum, false, mapping, depth + 1));
        }
        return out;
    }
    default:
        return value; // 标量直接复制
    }
}

int PdfPageEditor::allocateObjectNumber()
{
    if (m_nextFree < 1)
        m_nextFree = maxObjectNumber(m_file.objects) + 1;
    return m_nextFree++;
}

bool PdfPageEditor::setPageOrder(const QVector<int> &order, QString *errorMessage)
{
    if (!m_loaded) {
        if (errorMessage)
            *errorMessage = QStringLiteral("尚未打开文档");
        return false;
    }
    const int count = m_pages.size();
    if (order.size() != count) {
        if (errorMessage)
            *errorMessage = QStringLiteral("页面顺序不完整");
        return false;
    }
    QSet<int> seen;
    for (int i = 0; i < order.size(); ++i) {
        int old = order.at(i);
        if (old < 0 || old >= count || seen.contains(old)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("页面顺序不是有效排列");
            return false;
        }
        seen.insert(old);
    }
    QVector<PdfObject> pages;
    QVector<int> nums;
    pages.reserve(count);
    nums.reserve(count);
    for (int i = 0; i < order.size(); ++i) {
        pages.append(m_pages.at(order.at(i)));
        nums.append(m_pageObjectNums.at(order.at(i)));
    }
    m_pages = pages;
    m_pageObjectNums = nums;
    return true;
}

bool PdfPageEditor::rotatePages(const QList<int> &indices, int deltaDegrees, QString *errorMessage)
{
    if (!m_loaded) {
        if (errorMessage)
            *errorMessage = QStringLiteral("尚未打开文档");
        return false;
    }
    if (deltaDegrees % 90 != 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("旋转角度必须是 90 的倍数");
        return false;
    }
    QSet<int> seen;
    QList<int> targets;
    for (int i = 0; i < indices.size(); ++i) {
        int index = indices.at(i);
        if (index < 0 || index >= m_pages.size() || seen.contains(index))
            continue;
        seen.insert(index);
        targets.append(index);
    }
    if (targets.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("未选择要旋转的页面");
        return false;
    }
    for (int i = 0; i < targets.size(); ++i) {
        int index = targets.at(i);
        PdfObject page = m_pages.at(index);
        int current = page.dictGet("Rotate").toInt() % 360;
        int next = ((current + deltaDegrees) % 360 + 360) % 360;
        page.dictSet("Rotate", PdfObject::integer(next));
        m_pages[index] = page;
    }
    return true;
}

int PdfPageEditor::pageRotation(int index) const
{
    if (index < 0 || index >= m_pages.size())
        return 0;
    return ((m_pages.at(index).dictGet("Rotate").toInt() % 360) + 360) % 360;
}

QSizeF PdfPageEditor::pageVisualSize(int index) const
{
    QRectF box = pageMediaBox(index);
    if (pageRotation(index) % 180 == 90)
        return QSizeF(box.height(), box.width());
    return QSizeF(box.width(), box.height());
}

bool PdfPageEditor::saveAs(const QString &path, QString *errorMessage)
{
    if (!m_loaded) {
        if (errorMessage)
            *errorMessage = QStringLiteral("尚未打开文档");
        return false;
    }
    QHash<int, PdfObject> objects = m_file.objects;

    // ---- 重建单层页树：页字典与页树节点全部使用新对象号 ----
    int base = maxObjectNumber(objects) + 1;
    int pagesNodeNum = base + m_pages.size();
    for (int i = 0; i < m_pages.size(); ++i) {
        PdfObject page = m_pages.at(i);
        page.dictSet("Parent", PdfObject::reference(pagesNodeNum, 0));
        objects.insert(base + i, page);
    }
    PdfObject pagesNode = PdfObject::dictionary();
    pagesNode.dictSet("Type", PdfObject::name("Pages"));
    PdfObject kids = PdfObject::array();
    for (int i = 0; i < m_pages.size(); ++i)
        kids.arrayAppend(PdfObject::reference(base + i, 0));
    pagesNode.dictSet("Kids", kids);
    pagesNode.dictSet("Count", PdfObject::integer(m_pages.size()));
    objects.insert(pagesNodeNum, pagesNode);

    // ---- 目录指向新页树（旧页树与被换/被删页的对象随后被 mark-sweep 清理）----
    PdfObject catalog = objects.value(m_file.rootNum);
    catalog.dictSet("Pages", PdfObject::reference(pagesNodeNum, 0));
    objects.insert(m_file.rootNum, catalog);

    // ---- 更新修改时间 ----
    if (m_file.infoNum >= 0 && objects.contains(m_file.infoNum)) {
        PdfObject info = objects.value(m_file.infoNum);
        if (info.isDictionary()) {
            const QString stamp = QStringLiteral("D:%1Z")
                    .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmss")));
            info.dictSet("ModDate", PdfObject::string(stamp.toLatin1()));
            objects.insert(m_file.infoNum, info);
        }
    }
    return PdfWriter::writeToFile(path, objects, m_file.rootNum, m_file.infoNum,
                                  m_file.version, errorMessage);
}
