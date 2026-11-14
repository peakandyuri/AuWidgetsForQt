#ifndef PDFOBJECT_H
#define PDFOBJECT_H

#include <QByteArray>
#include <QPair>
#include <QVector>

// PDF 对象模型：页级编辑所需的轻量对象表示。
// 只保存原始字节（内容流不解码），足以完成页树的增删改与整体重写。
class PdfObject
{
public:
    enum Type {
        Null,
        Bool,
        Number,
        String,
        Name,
        Array,
        Dictionary,
        Stream,
        Reference
    };

    PdfObject(); // 默认构造为 Null

    static PdfObject nullObject();
    static PdfObject boolean(bool value);
    static PdfObject integer(qint64 value);
    static PdfObject real(double value);
    static PdfObject string(const QByteArray &value);
    static PdfObject name(const QByteArray &value);
    static PdfObject array();
    static PdfObject dictionary();
    // stream 的字典兼作流字典，可通过 dictGet()/dictSet() 直接访问
    static PdfObject stream(const PdfObject &dict, const QByteArray &rawData);
    static PdfObject reference(int objectNumber, int generation);

    Type type() const { return m_type; }
    bool isNull() const { return m_type == Null; }
    bool isBool() const { return m_type == Bool; }
    bool isNumber() const { return m_type == Number; }
    bool isString() const { return m_type == String; }
    bool isName() const { return m_type == Name; }
    bool isArray() const { return m_type == Array; }
    bool isDictionary() const { return m_type == Dictionary; }
    bool isStream() const { return m_type == Stream; }
    bool isReference() const { return m_type == Reference; }

    bool toBool(bool defaultValue = false) const;
    double toNumber(double defaultValue = 0.0) const;
    qint64 toInt(qint64 defaultValue = 0) const;
    bool isInteger() const { return m_isInt; }

    // String / Name 的字节内容
    QByteArray asString() const;

    // ---- 数组 ----
    int arraySize() const { return m_array.size(); }
    PdfObject arrayAt(int index) const;
    QVector<PdfObject> &arrayRef() { return m_array; }
    const QVector<PdfObject> &arrayRef() const { return m_array; }
    void arrayAppend(const PdfObject &value) { m_array.append(value); }

    // ---- 字典（Dictionary 与 Stream 共用，保持键的插入顺序）----
    int dictSize() const { return m_dict.size(); }
    bool hasKey(const QByteArray &key) const;
    PdfObject dictGet(const QByteArray &key) const; // 缺失时返回 Null
    void dictSet(const QByteArray &key, const PdfObject &value); // 覆盖或追加
    void dictRemove(const QByteArray &key);
    QVector<QPair<QByteArray, PdfObject> > &dict() { return m_dict; }
    const QVector<QPair<QByteArray, PdfObject> > &dict() const { return m_dict; }

    // ---- 流 ----
    QByteArray streamData() const { return m_streamData; }
    void setStreamData(const QByteArray &data) { m_streamData = data; }

    // ---- 引用 ----
    int refObjectNumber() const { return m_refNum; }
    int refGeneration() const { return m_refGen; }

private:
    Type m_type;
    bool m_bool;
    double m_number;
    bool m_isInt;
    qint64 m_int;
    QByteArray m_bytes;
    QVector<PdfObject> m_array;
    QVector<QPair<QByteArray, PdfObject> > m_dict;
    QByteArray m_streamData;
    int m_refNum;
    int m_refGen;
};

#endif // PDFOBJECT_H
