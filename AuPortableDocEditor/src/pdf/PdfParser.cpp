#include "PdfParser.h"

#include <QSet>

// 装载策略：
//   1. 经 startxref 定位交叉引用链（经典 xref 表 / xref 流 / 混合 XRefStm），
//      按新到旧的顺序合并各代条目（同一对象号先见者优先）；
//   2. 链条损坏或缺失时，回退为整文件顺序扫描修复；
//   3. 对象流（/Type /ObjStm）装载时解压平铺为常规对象；
//   4. 检测到 /Encrypt 即拒绝（不支持加密文档）。
// 内容流从不解码，只搬运原始字节。

namespace {

bool isWhitespaceChar(char c)
{
    return c == '\0' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == ' ';
}

bool isDelimiterChar(char c)
{
    return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']'
            || c == '{' || c == '}' || c == '/' || c == '%';
}

bool isRegularChar(char c)
{
    return !isWhitespaceChar(c) && !isDelimiterChar(c);
}

int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

qint64 readBigEndian(const QByteArray &data, qint64 pos, qint64 width, qint64 defaultValue)
{
    if (width <= 0)
        return defaultValue;
    qint64 value = 0;
    for (qint64 i = 0; i < width; ++i) {
        if (pos + i >= data.size())
            return defaultValue;
        value = (value << 8) | quint8(data.at(int(pos + i)));
    }
    return value;
}

// qUncompress 要求 4 字节大端“解压后大小”前缀，而 PDF 的 FlateDecode 流
// 没有该前缀。实际大小未知，按倍率放大估计值重试（估计值小于实际
// 解压大小时 qUncompress 返回空）。
QByteArray pdfInflate(const QByteArray &zlibData)
{
    if (zlibData.isEmpty())
        return QByteArray();
    qint64 estimate = qint64(zlibData.size()) * 4 + 64;
    while (estimate < (qint64(1) << 31)) {
        quint32 be = quint32(estimate);
        char prefix[4];
        prefix[0] = char((be >> 24) & 0xFF);
        prefix[1] = char((be >> 16) & 0xFF);
        prefix[2] = char((be >> 8) & 0xFF);
        prefix[3] = char(be & 0xFF);
        QByteArray prefixed(prefix, 4);
        prefixed.append(zlibData);
        QByteArray result = qUncompress(prefixed);
        if (!result.isNull())
            return result;
        estimate *= 4;
    }
    return QByteArray();
}

} // namespace

char PdfParser::charAt(qint64 pos) const
{
    if (pos < 0 || pos >= m_data->size())
        return '\0';
    return m_data->constData()[pos];
}

void PdfParser::skipWhitespace(qint64 &pos) const
{
    while (true) {
        char c = charAt(pos);
        if (isWhitespaceChar(c)) {
            ++pos;
            continue;
        }
        if (c == '%') { // 注释延伸到行尾
            while (pos < m_data->size()) {
                char ch = charAt(pos++);
                if (ch == '\n' || ch == '\r')
                    break;
            }
            continue;
        }
        break;
    }
}

bool PdfParser::matchKeyword(qint64 &pos, const char *keyword) const
{
    const char *raw = m_data->constData();
    int length = int(qstrlen(keyword));
    if (pos + length > m_data->size())
        return false;
    if (qstrncmp(raw + pos, keyword, size_t(length)) != 0)
        return false;
    // 关键字之后不能紧跟常规字符（避免把 "objx" 误判为 "obj"）
    char after = charAt(pos + length);
    if (isRegularChar(after))
        return false;
    pos += length;
    return true;
}

bool PdfParser::parseIntAt(qint64 pos, qint64 *value, qint64 *endPos) const
{
    qint64 p = pos;
    bool negative = false;
    if (charAt(p) == '+' || charAt(p) == '-') {
        negative = (charAt(p) == '-');
        ++p;
    }
    if (charAt(p) < '0' || charAt(p) > '9')
        return false;
    qint64 result = 0;
    while (charAt(p) >= '0' && charAt(p) <= '9') {
        result = result * 10 + (charAt(p) - '0');
        if (result > (qint64(1) << 60)) // 防溢出
            return false;
        ++p;
    }
    if (negative)
        result = -result;
    *value = result;
    if (endPos)
        *endPos = p;
    return true;
}

// 修复扫描用：从 from 位置（含）向回跳过空白后解析十进制整数。
// 返回整数第一个数字之前的位置，失败返回 -1。
qint64 PdfParser::backwardParseInt(qint64 from, qint64 *value) const
{
    qint64 p = from;
    while (p >= 0 && isWhitespaceChar(charAt(p)))
        --p;
    qint64 end = p;
    qint64 result = 0;
    qint64 scale = 1;
    while (p >= 0 && charAt(p) >= '0' && charAt(p) <= '9') {
        result += (charAt(p) - '0') * scale;
        scale *= 10;
        if (scale > (qint64(1) << 40))
            return -1;
        --p;
    }
    if (p == end || scale == 1) // 没有读到数字
        return -1;
    *value = result;
    return p;
}

PdfObject PdfParser::parseValue(qint64 &pos, int depth, bool *ok)
{
    if (ok)
        *ok = false;
    if (depth > 64) {
        m_error = QStringLiteral("PDF 对象嵌套过深");
        return PdfObject();
    }
    skipWhitespace(pos);
    char c = charAt(pos);
    if (c == '\0') {
        m_error = QStringLiteral("意外的文件结尾");
        return PdfObject();
    }

    // ---- 字面字符串 ----
    if (c == '(') {
        ++pos;
        QByteArray out;
        int level = 1;
        while (pos < m_data->size()) {
            char ch = charAt(pos++);
            if (ch == '\\') {
                if (pos >= m_data->size())
                    break;
                char e = charAt(pos++);
                switch (e) {
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case '(': out += '('; break;
                case ')': out += ')'; break;
                case '\\': out += '\\'; break;
                case '\n': break; // 行续接
                case '\r':
                    if (charAt(pos) == '\n')
                        ++pos;
                    break;
                default:
                    if (e >= '0' && e <= '7') { // 最多三位八进制
                        int value = e - '0';
                        int digits = 1;
                        while (digits < 3 && charAt(pos) >= '0' && charAt(pos) <= '7') {
                            value = value * 8 + (charAt(pos) - '0');
                            ++pos;
                            ++digits;
                        }
                        out += char(value);
                    } else {
                        out += e; // 反斜杠后跟其它字符按字符本身处理
                    }
                    break;
                }
            } else if (ch == '(') {
                ++level;
                out += ch;
            } else if (ch == ')') {
                if (--level == 0) {
                    if (ok)
                        *ok = true;
                    return PdfObject::string(out);
                }
                out += ch;
            } else {
                out += ch;
            }
        }
        m_error = QStringLiteral("字符串未闭合");
        return PdfObject();
    }

    // ---- 十六进制字符串 / 字典 ----
    if (c == '<') {
        if (charAt(pos + 1) == '<') {
            pos += 2;
            PdfObject dict = PdfObject::dictionary();
            while (true) {
                skipWhitespace(pos);
                if (charAt(pos) == '>' && charAt(pos + 1) == '>') {
                    pos += 2;
                    if (ok)
                        *ok = true;
                    return dict;
                }
                if (charAt(pos) == '\0') {
                    m_error = QStringLiteral("字典未闭合");
                    return PdfObject();
                }
                if (charAt(pos) != '/') {
                    m_error = QStringLiteral("字典键应为名字对象");
                    return PdfObject();
                }
                bool keyOk = false;
                PdfObject key = parseValue(pos, depth + 1, &keyOk);
                if (!keyOk)
                    return PdfObject();
                bool valueOk = false;
                PdfObject value = parseValue(pos, depth + 1, &valueOk);
                if (!valueOk)
                    return PdfObject();
                dict.dictSet(key.asString(), value);
            }
        }
        ++pos;
        QByteArray out;
        int nibble = -1;
        while (pos < m_data->size()) {
            char ch = charAt(pos++);
            if (ch == '>') {
                if (nibble >= 0)
                    out += char(nibble << 4); // 奇数位补 0
                if (ok)
                    *ok = true;
                return PdfObject::string(out);
            }
            int v = hexValue(ch);
            if (v < 0)
                continue; // 忽略空白等
            if (nibble < 0) {
                nibble = v;
            } else {
                out += char((nibble << 4) | v);
                nibble = -1;
            }
        }
        m_error = QStringLiteral("十六进制字符串未闭合");
        return PdfObject();
    }

    // ---- 数组 ----
    if (c == '[') {
        ++pos;
        PdfObject arr = PdfObject::array();
        while (true) {
            skipWhitespace(pos);
            if (charAt(pos) == ']') {
                ++pos;
                if (ok)
                    *ok = true;
                return arr;
            }
            if (charAt(pos) == '\0') {
                m_error = QStringLiteral("数组未闭合");
                return PdfObject();
            }
            bool itemOk = false;
            PdfObject item = parseValue(pos, depth + 1, &itemOk);
            if (!itemOk)
                return PdfObject();
            arr.arrayAppend(item);
        }
    }

    // ---- 名字 ----
    if (c == '/') {
        ++pos;
        QByteArray out;
        while (true) {
            char ch = charAt(pos);
            if (isWhitespaceChar(ch) || isDelimiterChar(ch) || ch == '\0')
                break;
            ++pos;
            if (ch == '#' && hexValue(charAt(pos)) >= 0 && hexValue(charAt(pos + 1)) >= 0) {
                out += char((hexValue(charAt(pos)) << 4) | hexValue(charAt(pos + 1)));
                pos += 2;
            } else {
                out += ch;
            }
        }
        if (ok)
            *ok = true;
        return PdfObject::name(out);
    }

    // ---- 数字（含引用前瞻：整数 整数 R）----
    if (c == '+' || c == '-' || c == '.' || (c >= '0' && c <= '9')) {
        qint64 p = pos;
        bool negative = false;
        if (charAt(p) == '+' || charAt(p) == '-') {
            negative = (charAt(p) == '-');
            ++p;
        }
        qint64 intPart = 0;
        bool anyDigit = false;
        while (charAt(p) >= '0' && charAt(p) <= '9') {
            intPart = intPart * 10 + (charAt(p) - '0');
            ++p;
            anyDigit = true;
        }
        bool isReal = false;
        double fracPart = 0.0;
        double fracScale = 0.1;
        if (charAt(p) == '.') {
            isReal = true;
            ++p;
            while (charAt(p) >= '0' && charAt(p) <= '9') {
                fracPart += (charAt(p) - '0') * fracScale;
                fracScale *= 0.1;
                ++p;
                anyDigit = true;
            }
        }
        if (!anyDigit) {
            m_error = QStringLiteral("无法解析的数字");
            return PdfObject();
        }
        pos = p;
        if (!isReal) {
            qint64 save = pos;
            qint64 second = 0;
            qint64 secondEnd = 0;
            skipWhitespace(save);
            if (parseIntAt(save, &second, &secondEnd)) {
                qint64 rpos = secondEnd;
                skipWhitespace(rpos);
                if (charAt(rpos) == 'R' && !isRegularChar(charAt(rpos + 1))) {
                    pos = rpos + 1;
                    if (ok)
                        *ok = true;
                    return PdfObject::reference(int(intPart), int(second));
                }
            }
        }
        if (ok)
            *ok = true;
        if (isReal) {
            double value = double(intPart) + fracPart;
            return PdfObject::real(negative ? -value : value);
        }
        return PdfObject::integer(negative ? -intPart : intPart);
    }

    // ---- 关键字 ----
    QByteArray token;
    while (isRegularChar(charAt(pos))) {
        token += charAt(pos);
        ++pos;
    }
    if (token == "true") {
        if (ok)
            *ok = true;
        return PdfObject::boolean(true);
    }
    if (token == "false") {
        if (ok)
            *ok = true;
        return PdfObject::boolean(false);
    }
    if (token == "null") {
        if (ok)
            *ok = true;
        return PdfObject();
    }
    m_error = QStringLiteral("无法识别的词法单元“%1”").arg(QString::fromLatin1(token));
    return PdfObject();
}

bool PdfParser::parseObjectAt(qint64 offset, int *objNum, PdfObject *out)
{
    qint64 pos = offset;
    qint64 num = 0;
    qint64 gen = 0;
    if (!parseIntAt(pos, &num, &pos))
        return false;
    skipWhitespace(pos);
    if (!parseIntAt(pos, &gen, &pos))
        return false;
    skipWhitespace(pos);
    if (!matchKeyword(pos, "obj"))
        return false;
    bool ok = false;
    PdfObject value = parseValue(pos, 0, &ok);
    if (!ok)
        return false;
    skipWhitespace(pos);
    if (matchKeyword(pos, "stream")) {
        QByteArray streamData = takeStreamData(value, pos, &ok);
        if (!ok)
            return false;
        value = PdfObject::stream(value, streamData);
        skipWhitespace(pos);
    }
    matchKeyword(pos, "endobj"); // 缺失时容忍
    if (objNum)
        *objNum = int(num);
    if (out)
        *out = value;
    return true;
}

QByteArray PdfParser::takeStreamData(const PdfObject &dict, qint64 &pos, bool *ok)
{
    if (ok)
        *ok = false;
    // stream 关键字之后的 EOL 不属于流数据
    if (charAt(pos) == '\r')
        ++pos;
    if (charAt(pos) == '\n')
        ++pos;
    qint64 dataStart = pos;
    qint64 length = -1;
    PdfObject lengthObj = dict.dictGet("Length");
    if (lengthObj.isNumber() && lengthObj.isInteger())
        length = lengthObj.toInt();
    if (length >= 0 && dataStart + length <= m_data->size()) {
        // 校验声明长度之后确为 endstream（声明不可信时回退到搜索）
        qint64 check = dataStart + length;
        skipWhitespace(check);
        if (matchKeyword(check, "endstream")) {
            pos = check;
            if (ok)
                *ok = true;
            return m_data->mid(int(dataStart), int(length));
        }
    }
    int idx = int(m_data->indexOf("endstream", int(dataStart)));
    if (idx < 0) {
        m_error = QStringLiteral("流数据缺少 endstream 结束标记");
        return QByteArray();
    }
    qint64 end = idx;
    if (end > dataStart && charAt(end - 1) == '\n')
        --end;
    if (end > dataStart && charAt(end - 1) == '\r')
        --end;
    QByteArray streamData = m_data->mid(int(dataStart), int(end - dataStart));
    pos = idx + 9;
    if (ok)
        *ok = true;
    return streamData;
}

bool PdfParser::load(const QByteArray &data, PdfFileData &out, QString *errorMessage)
{
    m_data = &data;
    m_error.clear();
    m_parsedObjStm.clear();

    // ---- 文件头版本 ----
    int headerPos = data.indexOf("%PDF-");
    if (headerPos < 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("不是有效的 PDF 文件（缺少 %PDF- 文件头）");
        return false;
    }
    qint64 p = headerPos + 5;
    QByteArray version;
    while (charAt(p) == '.' || (charAt(p) >= '0' && charAt(p) <= '9')) {
        version += charAt(p);
        ++p;
    }
    if (version.isEmpty())
        version = QByteArrayLiteral("1.7");
    out.version = version;

    // ---- 交叉引用链 ----
    bool loaded = false;
    int sx = int(data.lastIndexOf("startxref"));
    if (sx >= 0) {
        qint64 pos = sx + 9;
        skipWhitespace(pos);
        qint64 target = 0;
        if (parseIntAt(pos, &target, nullptr) && target > 0 && target < data.size())
            loaded = loadViaXref(target, out);
    }
    if (!loaded || out.rootNum < 0 || !out.objects.contains(out.rootNum)) {
        PdfFileData repaired;
        if (repairScan(repaired) && repaired.rootNum >= 0
                && repaired.objects.contains(repaired.rootNum)) {
            out = repaired;
            loaded = true;
        }
    }
    if (!loaded || out.rootNum < 0 || !out.objects.contains(out.rootNum)) {
        if (errorMessage) {
            *errorMessage = m_error.isEmpty()
                    ? QStringLiteral("无法解析 PDF 文件结构")
                    : m_error;
        }
        return false;
    }
    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool PdfParser::loadViaXref(qint64 startXrefPos, PdfFileData &out)
{
    QHash<int, qint64> offsets;
    QHash<int, QPair<int, int> > inObjStm;
    QSet<qint64> visited;
    qint64 pos = startXrefPos;

    while (pos >= 0 && pos < m_data->size() && !visited.contains(pos)) {
        visited.insert(pos);
        skipWhitespace(pos);
        PdfObject trailer;
        if (matchKeyword(pos, "xref")) {
            if (!parseClassicXref(pos, &offsets, &inObjStm, &trailer))
                return false;
        } else {
            if (!parseXrefStreamAt(pos, &offsets, &inObjStm, &trailer))
                return false;
        }
        if (!(trailer.isDictionary() || trailer.isStream()))
            break;
        if (trailer.hasKey("Encrypt")) {
            m_error = QStringLiteral("不支持加密的 PDF 文件");
            return false;
        }
        PdfObject root = trailer.dictGet("Root");
        if (root.isReference() && out.rootNum < 0)
            out.rootNum = root.refObjectNumber();
        PdfObject info = trailer.dictGet("Info");
        if (info.isReference() && out.infoNum < 0)
            out.infoNum = info.refObjectNumber();

        // 混合引用文件：/XRefStm 指向伴随的 xref 流（字节偏移），
        // 优先于 /Prev 处理，但不得覆盖当前表已有的条目
        PdfObject xrefStm = trailer.dictGet("XRefStm");
        if (xrefStm.isNumber()) {
            qint64 subPos = xrefStm.toInt();
            if (subPos >= 0 && subPos < m_data->size() && !visited.contains(subPos)) {
                visited.insert(subPos);
                PdfObject subTrailer;
                parseXrefStreamAt(subPos, &offsets, &inObjStm, &subTrailer);
            }
        }
        PdfObject prev = trailer.dictGet("Prev");
        if (prev.isNumber()) {
            pos = prev.toInt();
            continue;
        }
        break;
    }
    return materializeObjects(offsets, inObjStm, out);
}

bool PdfParser::parseClassicXref(qint64 &pos, QHash<int, qint64> *offsets,
                                 QHash<int, QPair<int, int> > *inObjStm,
                                 PdfObject *trailerDict)
{
    // 可有多个子表，每个子表为 “first count” 行加 count 个条目行
    while (true) {
        qint64 save = pos;
        qint64 first = 0;
        qint64 count = 0;
        skipWhitespace(pos);
        if (!parseIntAt(pos, &first, &pos))
            break;
        skipWhitespace(pos);
        if (!parseIntAt(pos, &count, &pos)) {
            pos = save;
            break;
        }
        for (qint64 i = 0; i < count; ++i) {
            skipWhitespace(pos);
            qint64 offset = 0;
            qint64 gen = 0;
            if (!parseIntAt(pos, &offset, &pos))
                return false;
            skipWhitespace(pos);
            if (!parseIntAt(pos, &gen, &pos))
                return false;
            skipWhitespace(pos);
            char type = charAt(pos);
            if (type == 'n' || type == 'f')
                ++pos;
            else
                return false;
            int objNum = int(first + i);
            if (type == 'n' && !offsets->contains(objNum) && !inObjStm->contains(objNum))
                offsets->insert(objNum, offset);
        }
    }
    skipWhitespace(pos);
    if (!matchKeyword(pos, "trailer")) {
        m_error = QStringLiteral("xref 表后缺少 trailer 字典");
        return false;
    }
    bool ok = false;
    PdfObject trailer = parseValue(pos, 0, &ok);
    if (!ok || !trailer.isDictionary()) {
        m_error = QStringLiteral("trailer 字典解析失败");
        return false;
    }
    *trailerDict = trailer;
    return true;
}

bool PdfParser::parseXrefStreamAt(qint64 objOffset, QHash<int, qint64> *offsets,
                                  QHash<int, QPair<int, int> > *inObjStm,
                                  PdfObject *trailerDict)
{
    int objNum = 0;
    PdfObject obj;
    if (!parseObjectAt(objOffset, &objNum, &obj) || !obj.isStream()) {
        m_error = QStringLiteral("交叉引用流解析失败");
        return false;
    }
    PdfObject w = obj.dictGet("W");
    if (!w.isArray() || w.arraySize() < 3) {
        m_error = QStringLiteral("交叉引用流缺少 /W 字段");
        return false;
    }
    qint64 fieldWidth[3];
    for (int i = 0; i < 3; ++i)
        fieldWidth[i] = w.arrayAt(i).toInt();

    QVector<qint64> starts;
    QVector<qint64> counts;
    PdfObject index = obj.dictGet("Index");
    if (index.isArray()) {
        for (int i = 0; i + 1 < index.arraySize(); i += 2) {
            starts.append(index.arrayAt(i).toInt());
            counts.append(index.arrayAt(i + 1).toInt());
        }
    } else {
        starts.append(0);
        counts.append(obj.dictGet("Size").toInt());
    }

    QByteArray decoded = obj.streamData();
    if (obj.hasKey("Filter")) {
        decoded = inflate(decoded);
        if (decoded.isEmpty()) {
            m_error = QStringLiteral("交叉引用流解压失败");
            return false;
        }
    }

    qint64 rowLength = fieldWidth[0] + fieldWidth[1] + fieldWidth[2];
    if (rowLength <= 0) {
        m_error = QStringLiteral("交叉引用流 /W 字段非法");
        return false;
    }
    qint64 rowPos = 0;
    for (int s = 0; s < starts.size(); ++s) {
        for (qint64 k = 0; k < counts.at(s); ++k) {
            if (rowPos + rowLength > decoded.size())
                break;
            qint64 type = fieldWidth[0] > 0
                    ? readBigEndian(decoded, rowPos, fieldWidth[0], 0)
                    : 1; // W[0] 为 0 时条目类型默认 1
            qint64 a = readBigEndian(decoded, rowPos + fieldWidth[0], fieldWidth[1], 0);
            qint64 b = readBigEndian(decoded, rowPos + fieldWidth[0] + fieldWidth[1],
                                     fieldWidth[2], 0);
            int objNum2 = int(starts.at(s) + k);
            if (type == 1) {
                if (!offsets->contains(objNum2) && !inObjStm->contains(objNum2))
                    offsets->insert(objNum2, a);
            } else if (type == 2) {
                if (!offsets->contains(objNum2) && !inObjStm->contains(objNum2))
                    inObjStm->insert(objNum2, qMakePair(int(a), int(b)));
            }
            rowPos += rowLength;
        }
    }
    *trailerDict = obj;
    return true;
}

bool PdfParser::materializeObjects(const QHash<int, qint64> &offsets,
                                   const QHash<int, QPair<int, int> > &inObjStm,
                                   PdfFileData &out)
{
    for (QHash<int, qint64>::const_iterator it = offsets.constBegin();
         it != offsets.constEnd(); ++it) {
        int objNum = 0;
        PdfObject obj;
        if (parseObjectAt(it.value(), &objNum, &obj))
            out.objects.insert(objNum, obj);
    }
    for (QHash<int, QPair<int, int> >::const_iterator it = inObjStm.constBegin();
         it != inObjStm.constEnd(); ++it) {
        int streamNum = it.value().first;
        if (!out.objects.contains(streamNum)) {
            QHash<int, qint64>::const_iterator off = offsets.constFind(streamNum);
            if (off != offsets.constEnd()) {
                int parsedNum = 0;
                PdfObject streamObj;
                if (parseObjectAt(off.value(), &parsedNum, &streamObj)
                        && streamObj.isStream()) {
                    out.objects.insert(parsedNum, streamObj);
                }
            }
        }
        if (out.objects.contains(streamNum) && !m_parsedObjStm.contains(streamNum)) {
            m_parsedObjStm.insert(streamNum, true);
            parseObjectStream(out.objects.value(streamNum), out);
        }
    }
    return true;
}

bool PdfParser::parseObjectStream(const PdfObject &objStm, PdfFileData &out)
{
    PdfObject countObj = objStm.dictGet("N");
    PdfObject firstObj = objStm.dictGet("First");
    if (!countObj.isNumber() || !firstObj.isNumber())
        return false;
    int count = int(countObj.toNumber());
    int first = int(firstObj.toNumber());
    if (count <= 0 || first < 0)
        return false;
    QByteArray raw = objStm.streamData();
    if (objStm.hasKey("Filter")) {
        raw = inflate(raw);
        if (raw.isEmpty())
            return false;
    }
    // 头部为 N 对 “对象号 相对偏移”
    qint64 pos = 0;
    QVector<QPair<int, int> > pairs;
    pairs.reserve(count);
    for (int i = 0; i < count; ++i) {
        skipWhitespace(pos);
        qint64 num = 0;
        qint64 offset = 0;
        if (!parseIntAt(pos, &num, &pos) || !parseIntAt(pos, &offset, &pos))
            return false;
        pairs.append(qMakePair(int(num), int(offset)));
    }
    for (int i = 0; i < pairs.size(); ++i) {
        qint64 bodyPos = first + pairs.at(i).second;
        if (bodyPos < 0 || bodyPos >= raw.size())
            continue;
        qint64 p = bodyPos;
        bool ok = false;
        PdfObject value = parseValue(p, 0, &ok);
        if (ok)
            out.objects.insert(pairs.at(i).first, value);
    }
    return true;
}

bool PdfParser::repairScan(PdfFileData &out)
{
    // 整文件顺序扫描：定位每个 “N G obj”，解析对象；
    // 流对象解析成功后跳过流数据，避免匹配到流内容里的 “obj”。
    out = PdfFileData();

    int headerPos = m_data->indexOf("%PDF-");
    if (headerPos >= 0) {
        qint64 p = headerPos + 5;
        QByteArray version;
        while (charAt(p) == '.' || (charAt(p) >= '0' && charAt(p) <= '9')) {
            version += charAt(p);
            ++p;
        }
        out.version = version.isEmpty() ? QByteArrayLiteral("1.7") : version;
    }

    bool encryptSeen = false;
    int rootCandidate = -1;
    int infoCandidate = -1;
    PdfObject lastTrailer;

    // ---- trailer 字典（最后一个含 /Root 的为准）----
    qint64 searchPos = 0;
    while (true) {
        int idx = int(m_data->indexOf("trailer", int(searchPos)));
        if (idx < 0)
            break;
        searchPos = idx + 7;
        if (isRegularChar(charAt(searchPos)))
            continue; // 如 “trailers” 之类
        qint64 p = searchPos;
        bool ok = false;
        PdfObject trailer = parseValue(p, 0, &ok);
        if (ok && trailer.isDictionary()) {
            if (trailer.hasKey("Encrypt"))
                encryptSeen = true;
            if (trailer.hasKey("Root")) {
                lastTrailer = trailer;
                PdfObject root = trailer.dictGet("Root");
                if (root.isReference())
                    rootCandidate = root.refObjectNumber();
                PdfObject info = trailer.dictGet("Info");
                infoCandidate = info.isReference() ? info.refObjectNumber() : -1;
            }
        }
    }

    // xref 流也可充当 trailer（其字典含 /Root）
    qint64 objPos = 0;
    while (true) {
        int idx = int(m_data->indexOf("obj", int(objPos)));
        if (idx < 0)
            break;
        objPos = idx + 3;
        qint64 gen = -1;
        qint64 num = -1;
        qint64 p = backwardParseInt(idx - 1, &gen);
        if (p >= 0)
            p = backwardParseInt(p, &num);
        if (p < 0 || num < 0)
            continue;
        qint64 bodyPos = idx + 3;
        bool ok = false;
        PdfObject value = parseValue(bodyPos, 0, &ok);
        if (ok) {
            skipWhitespace(bodyPos);
            if (matchKeyword(bodyPos, "stream")) {
                QByteArray streamData = takeStreamData(value, bodyPos, &ok);
                if (ok)
                    value = PdfObject::stream(value, streamData);
            }
        }
        if (!ok)
            continue;
        // 后出现者覆盖先出现者（增量更新时最新版本生效）
        out.objects.insert(int(num), value);
        if (value.isStream() && value.dictGet("Type").asString() == "XRef"
                && value.hasKey("Root")) {
            PdfObject root = value.dictGet("Root");
            if (root.isReference() && rootCandidate < 0)
                rootCandidate = root.refObjectNumber();
            PdfObject info = value.dictGet("Info");
            if (info.isReference() && infoCandidate < 0)
                infoCandidate = info.refObjectNumber();
            if (value.hasKey("Encrypt"))
                encryptSeen = true;
        }
        objPos = bodyPos; // 跳过刚解析的对象（含流数据）
    }

    if (encryptSeen) {
        m_error = QStringLiteral("不支持加密的 PDF 文件");
        return false;
    }
    if (lastTrailer.isDictionary() && lastTrailer.hasKey("Root")) {
        PdfObject root = lastTrailer.dictGet("Root");
        if (root.isReference())
            rootCandidate = root.refObjectNumber();
    }
    out.rootNum = rootCandidate;
    out.infoNum = infoCandidate;
    return rootCandidate >= 0;
}

QByteArray PdfParser::inflate(const QByteArray &zlibData) const
{
    return pdfInflate(zlibData);
}
