#include "PdfWriter.h"

#include <QSaveFile>
#include <QVector>

#include <math.h>

namespace {

// 名字中的保留字符用 #xx 转义
bool needsNameEscape(char c)
{
    if (c <= 32 || c >= 127)
        return true;
    switch (c) {
    case '(': case ')': case '<': case '>': case '[': case ']':
    case '{': case '}': case '/': case '%': case '#':
        return true;
    default:
        return false;
    }
}

inline bool isPdfWhitespace(char c)
{
    return c == '\0' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == ' ';
}

} // namespace

// 收集对象中直接出现的全部引用号（跳过 /Length——写出时改为直接整数）
void PdfWriter::collectReferences(const PdfObject &obj, QVector<int> *out)
{
    switch (obj.type()) {
    case PdfObject::Reference:
        out->append(obj.refObjectNumber());
        break;
    case PdfObject::Array: {
        const QVector<PdfObject> &items = obj.arrayRef();
        for (int i = 0; i < items.size(); ++i)
            collectReferences(items.at(i), out);
        break;
    }
    case PdfObject::Dictionary:
    case PdfObject::Stream: {
        const QVector<QPair<QByteArray, PdfObject> > &dict = obj.dict();
        for (int i = 0; i < dict.size(); ++i) {
            if (dict.at(i).first == "Length")
                continue;
            collectReferences(dict.at(i).second, out);
        }
        break;
    }
    default:
        break;
    }
}

QByteArray PdfWriter::escapeName(const QByteArray &name)
{
    QByteArray out;
    out.reserve(name.size() + 8);
    static const char digits[] = "0123456789ABCDEF";
    for (int i = 0; i < name.size(); ++i) {
        char c = name.at(i);
        if (needsNameEscape(c)) {
            out += '#';
            out += digits[(quint8(c) >> 4) & 0xF];
            out += digits[quint8(c) & 0xF];
        } else {
            out += c;
        }
    }
    return out;
}

// 字符串含不可打印字节时写成十六进制串（PDF 允许字符串任意使用十六进制形式）
QByteArray PdfWriter::escapeString(const QByteArray &value)
{
    bool printable = true;
    for (int i = 0; i < value.size(); ++i) {
        char c = value.at(i);
        if (quint8(c) < 32 || quint8(c) > 126) {
            printable = false;
            break;
        }
    }
    if (printable) {
        QByteArray out;
        out.reserve(value.size() + 2);
        out += '(';
        for (int i = 0; i < value.size(); ++i) {
            char c = value.at(i);
            if (c == '(' || c == ')' || c == '\\')
                out += '\\';
            out += c;
        }
        out += ')';
        return out;
    }
    static const char digits[] = "0123456789ABCDEF";
    QByteArray out;
    out.reserve(value.size() * 2 + 2);
    out += '<';
    for (int i = 0; i < value.size(); ++i) {
        out += digits[(quint8(value.at(i)) >> 4) & 0xF];
        out += digits[quint8(value.at(i)) & 0xF];
    }
    out += '>';
    return out;
}

QByteArray PdfWriter::numberString(double value)
{
    // 整数值按整数输出，避免 "595.000000" 之类的冗余
    if (fabs(value) < 1.0e15 && value == floor(value))
        return QByteArray::number(qint64(value));
    QString s = QString::number(value, 'f', 6);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    if (s == QLatin1String("-0"))
        s = QLatin1String("0");
    return s.toLatin1();
}

void PdfWriter::serializeInto(const PdfObject &obj, const QHash<int, int> &mapping,
                              QByteArray *out)
{
    switch (obj.type()) {
    case PdfObject::Null:
        *out += "null";
        break;
    case PdfObject::Bool:
        *out += (obj.toBool() ? "true" : "false");
        break;
    case PdfObject::Number:
        if (obj.isInteger())
            *out += QByteArray::number(obj.toInt());
        else
            *out += numberString(obj.toNumber());
        break;
    case PdfObject::String:
        *out += escapeString(obj.asString());
        break;
    case PdfObject::Name:
        *out += '/';
        *out += escapeName(obj.asString());
        break;
    case PdfObject::Reference: {
        int newNum = mapping.value(obj.refObjectNumber(), 0);
        if (newNum > 0) {
            *out += QByteArray::number(newNum);
            *out += " 0 R";
        } else {
            *out += "null"; // 引用了不可达对象：按规范写成空对象
        }
        break;
    }
    case PdfObject::Array: {
        *out += '[';
        const QVector<PdfObject> &items = obj.arrayRef();
        for (int i = 0; i < items.size(); ++i) {
            if (i)
                *out += ' ';
            serializeInto(items.at(i), mapping, out);
        }
        *out += ']';
        break;
    }
    case PdfObject::Dictionary:
    case PdfObject::Stream: {
        *out += "<<";
        const QVector<QPair<QByteArray, PdfObject> > &dict = obj.dict();
        bool lengthWritten = false;
        for (int i = 0; i < dict.size(); ++i) {
            *out += ' ';
            *out += '/';
            *out += escapeName(dict.at(i).first);
            *out += ' ';
            if (obj.isStream() && dict.at(i).first == "Length") {
                *out += QByteArray::number(obj.streamData().size()); // 长度写成直接整数
                lengthWritten = true;
            } else {
                serializeInto(dict.at(i).second, mapping, out);
            }
        }
        if (obj.isStream() && !lengthWritten) {
            // /Length 为必填项：新建流（如插入的空白页内容流）可能没有该字段
            *out += " /Length ";
            *out += QByteArray::number(obj.streamData().size());
        }
        *out += " >>";
        if (obj.isStream()) {
            *out += "\nstream\n";
            out->append(obj.streamData());
            *out += "\nendstream";
        }
        break;
    }
    }
}

QByteArray PdfWriter::serializeObject(const PdfObject &obj, const QHash<int, int> &mapping)
{
    QByteArray out;
    serializeInto(obj, mapping, &out);
    return out;
}

bool PdfWriter::writeToFile(const QString &path, const QHash<int, PdfObject> &objects,
                            int rootNum, int infoNum, const QByteArray &version,
                            QString *errorMessage)
{
    if (rootNum < 0 || !objects.contains(rootNum)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("文档缺少目录对象，无法保存");
        return false;
    }

    // ---- mark-sweep：从 /Root 与 /Info 收集可达对象并紧凑重编号 ----
    QHash<int, int> mapping; // 旧对象号 -> 新对象号
    QVector<int> order;
    QVector<int> queue;
    queue.append(rootNum);
    if (infoNum >= 0 && objects.contains(infoNum))
        queue.append(infoNum);
    int nextNumber = 1;
    for (int head = 0; head < queue.size(); ++head) {
        int oldNum = queue.at(head);
        if (mapping.contains(oldNum) || !objects.contains(oldNum))
            continue;
        mapping.insert(oldNum, nextNumber++);
        order.append(oldNum);
        collectReferences(objects.value(oldNum), &queue);
    }

    // ---- 逐对象序列化并记录偏移 ----
    QByteArray body;
    body.reserve(4096);
    QByteArray versionLine = version.isEmpty() ? QByteArrayLiteral("1.7") : version;
    body += "%PDF-";
    body += versionLine;
    body += "\n%\xE2\xE3\xCF\xD3\n"; // 二进制标记行，提示按二进制处理

    QVector<qint64> offsets(nextNumber, 0);
    for (int i = 0; i < order.size(); ++i) {
        int oldNum = order.at(i);
        int newNum = mapping.value(oldNum);
        offsets[newNum] = body.size();
        body += QByteArray::number(newNum);
        body += " 0 obj\n";
        body += serializeObject(objects.value(oldNum), mapping);
        body += "\nendobj\n";
    }

    // ---- xref 表 ----
    qint64 xrefPos = body.size();
    int size = nextNumber; // 含 0 号空闲表项
    body += "xref\n";
    body += "0 ";
    body += QByteArray::number(size);
    body += '\n';
    body += "0000000000 65535 f \n";
    for (int num = 1; num < size; ++num) {
        QString entry = QString::fromLatin1("%1 00000 n \n").arg(offsets.at(num), 10, 10, QLatin1Char('0'));
        body += entry.toLatin1();
    }

    // ---- trailer ----
    body += "trailer\n<< /Size ";
    body += QByteArray::number(size);
    body += " /Root ";
    body += QByteArray::number(mapping.value(rootNum));
    body += " 0 R";
    if (infoNum >= 0 && mapping.contains(infoNum)) {
        body += " /Info ";
        body += QByteArray::number(mapping.value(infoNum));
        body += " 0 R";
    }
    body += " >>\nstartxref\n";
    body += QByteArray::number(xrefPos);
    body += "\n%%EOF\n";

    // ---- 原子写入 ----
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法写入文件：%1").arg(file.errorString());
        return false;
    }
    if (file.write(body) != body.size()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("写入文件失败：%1").arg(file.errorString());
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("保存文件失败：%1").arg(file.errorString());
        return false;
    }
    if (errorMessage)
        errorMessage->clear();
    return true;
}
