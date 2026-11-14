#ifndef PDFPARSER_H
#define PDFPARSER_H

#include <QByteArray>
#include <QHash>
#include <QString>

#include "PdfObject.h"

// 解析结果：对象表 + 文档级信息（目录、信息字典、版本号）
struct PdfFileData
{
    QHash<int, PdfObject> objects; // 对象号 -> 对象（增量更新时以最新版本为准）
    QByteArray version;            // 文件头声明的版本，如 "1.7"
    int rootNum;                   // /Root（Catalog）对象号，-1 表示未知
    int infoNum;                   // /Info 对象号，-1 表示没有

    PdfFileData() : rootNum(-1), infoNum(-1) {}
};

// PDF 文件装载器：支持 xref 表、xref 流（PDF 1.5+）、对象流与
// 增量更新链；文件损坏时回退到整文件扫描修复。
// 不支持加密文档（检测到 /Encrypt 时报告错误）。
class PdfParser
{
public:
    bool load(const QByteArray &data, PdfFileData &out, QString *errorMessage);

private:
    // ---- 词法 ----
    char charAt(qint64 pos) const;
    void skipWhitespace(qint64 &pos) const;
    bool matchKeyword(qint64 &pos, const char *keyword) const;
    bool parseIntAt(qint64 pos, qint64 *value, qint64 *endPos) const;
    qint64 backwardParseInt(qint64 from, qint64 *value) const; // 修复扫描用：从 from 起向回解析整数
    PdfObject parseValue(qint64 &pos, int depth, bool *ok);

    // ---- 对象 ----
    bool parseObjectAt(qint64 offset, int *objNum, PdfObject *out);
    QByteArray takeStreamData(const PdfObject &dict, qint64 &pos, bool *ok);

    // ---- xref 装载 ----
    bool loadViaXref(qint64 startXrefPos, PdfFileData &out);
    bool parseClassicXref(qint64 &pos, QHash<int, qint64> *offsets,
                          QHash<int, QPair<int, int> > *inObjStm,
                          PdfObject *trailerDict);
    bool parseXrefStreamAt(qint64 objOffset, QHash<int, qint64> *offsets,
                           QHash<int, QPair<int, int> > *inObjStm,
                           PdfObject *trailerDict);
    bool materializeObjects(const QHash<int, qint64> &offsets,
                            const QHash<int, QPair<int, int> > &inObjStm,
                            PdfFileData &out);
    bool parseObjectStream(const PdfObject &objStm, PdfFileData &out);

    // ---- 修复扫描 ----
    bool repairScan(PdfFileData &out);

    QByteArray inflate(const QByteArray &zlibData) const;

    const QByteArray *m_data;
    QString m_error;
    QHash<int, bool> m_parsedObjStm; // 已解析的对象流
};

#endif // PDFPARSER_H
