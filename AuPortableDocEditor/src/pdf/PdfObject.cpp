#include "PdfObject.h"

PdfObject::PdfObject()
    : m_type(Null)
    , m_bool(false)
    , m_number(0.0)
    , m_isInt(false)
    , m_int(0)
    , m_refNum(0)
    , m_refGen(0)
{
}

PdfObject PdfObject::nullObject()
{
    return PdfObject();
}

PdfObject PdfObject::boolean(bool value)
{
    PdfObject obj;
    obj.m_type = Bool;
    obj.m_bool = value;
    return obj;
}

PdfObject PdfObject::integer(qint64 value)
{
    PdfObject obj;
    obj.m_type = Number;
    obj.m_isInt = true;
    obj.m_int = value;
    obj.m_number = double(value);
    return obj;
}

PdfObject PdfObject::real(double value)
{
    PdfObject obj;
    obj.m_type = Number;
    obj.m_isInt = false;
    obj.m_int = qint64(value);
    obj.m_number = value;
    return obj;
}

PdfObject PdfObject::string(const QByteArray &value)
{
    PdfObject obj;
    obj.m_type = String;
    obj.m_bytes = value;
    return obj;
}

PdfObject PdfObject::name(const QByteArray &value)
{
    PdfObject obj;
    obj.m_type = Name;
    obj.m_bytes = value;
    return obj;
}

PdfObject PdfObject::array()
{
    PdfObject obj;
    obj.m_type = Array;
    return obj;
}

PdfObject PdfObject::dictionary()
{
    PdfObject obj;
    obj.m_type = Dictionary;
    return obj;
}

PdfObject PdfObject::stream(const PdfObject &dict, const QByteArray &rawData)
{
    PdfObject obj;
    obj.m_type = Stream;
    obj.m_dict = dict.m_dict;
    obj.m_streamData = rawData;
    return obj;
}

PdfObject PdfObject::reference(int objectNumber, int generation)
{
    PdfObject obj;
    obj.m_type = Reference;
    obj.m_refNum = objectNumber;
    obj.m_refGen = generation;
    return obj;
}

bool PdfObject::toBool(bool defaultValue) const
{
    if (m_type == Bool)
        return m_bool;
    return defaultValue;
}

double PdfObject::toNumber(double defaultValue) const
{
    if (m_type != Number)
        return defaultValue;
    if (m_isInt)
        return double(m_int);
    return m_number;
}

qint64 PdfObject::toInt(qint64 defaultValue) const
{
    if (m_type != Number)
        return defaultValue;
    if (m_isInt)
        return m_int;
    return qint64(m_number + 0.5 * (m_number >= 0 ? 1.0 : -1.0));
}

QByteArray PdfObject::asString() const
{
    if (m_type == String || m_type == Name)
        return m_bytes;
    return QByteArray();
}

PdfObject PdfObject::arrayAt(int index) const
{
    if (index >= 0 && index < m_array.size())
        return m_array.at(index);
    return PdfObject();
}

bool PdfObject::hasKey(const QByteArray &key) const
{
    for (int i = 0; i < m_dict.size(); ++i) {
        if (m_dict.at(i).first == key)
            return true;
    }
    return false;
}

PdfObject PdfObject::dictGet(const QByteArray &key) const
{
    for (int i = 0; i < m_dict.size(); ++i) {
        if (m_dict.at(i).first == key)
            return m_dict.at(i).second;
    }
    return PdfObject();
}

void PdfObject::dictSet(const QByteArray &key, const PdfObject &value)
{
    for (int i = 0; i < m_dict.size(); ++i) {
        if (m_dict.at(i).first == key) {
            m_dict[i].second = value;
            return;
        }
    }
    m_dict.append(qMakePair(key, value));
}

void PdfObject::dictRemove(const QByteArray &key)
{
    for (int i = 0; i < m_dict.size(); ++i) {
        if (m_dict.at(i).first == key) {
            m_dict.remove(i);
            return;
        }
    }
}
