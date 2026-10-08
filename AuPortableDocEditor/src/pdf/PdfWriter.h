#ifndef PDFWRITER_H
#define PDFWRITER_H

#include <QByteArray>
#include <QHash>
#include <QString>

#include "PdfObject.h"

// PDF 写出器：把对象表全量重写为一个新文件。
// 只保留从 /Root 与 /Info 可达的对象（mark-sweep 清理被删页、被换页
// 遗留的孤儿对象），并紧凑重编号；输出经典 xref 表，兼容性最好。
class PdfWriter
{
public:
    static bool writeToFile(const QString &path, const QHash<int, PdfObject> &objects,
                            int rootNum, int infoNum, const QByteArray &version,
                            QString *errorMessage);

private:
    static void collectReferences(const PdfObject &obj, QVector<int> *out);
    static QByteArray serializeObject(const PdfObject &obj, const QHash<int, int> &mapping);
    static void serializeInto(const PdfObject &obj, const QHash<int, int> &mapping,
                              QByteArray *out);
    static QByteArray escapeName(const QByteArray &name);
    static QByteArray escapeString(const QByteArray &value);
    static QByteArray numberString(double value);
};

#endif // PDFWRITER_H
