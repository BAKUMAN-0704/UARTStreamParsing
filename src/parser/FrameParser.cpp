#include "FrameParser.h"
#include "CrcCalculator.h"
#include <QtEndian>
#include <cstring>

void FrameParser::setConfig(const FrameConfig &config) {
    m_config = config;
    m_headerBytes = m_config.headerBytes();
}

QVector<ParsedFrame> FrameParser::parse(const QByteArray &rawData,
                                        ProgressCallback progressCb) {
    QVector<ParsedFrame> frames;
    if (m_config.fields.isEmpty() || rawData.isEmpty())
        return frames;

    const int totalSize = rawData.size();
    int offset = 0;
    int frameCounter = 0;
    int lastPct = -1;

    while (offset < totalSize) {
        int headerPos = findHeader(rawData, offset);
        if (headerPos < 0)
            break;

        ParsedFrame frame;
        int frameEnd = 0;
        if (tryParseFrame(rawData, headerPos, frame, frameEnd)) {
            frame.frameIndex = ++frameCounter;
            frame.offsetInStream = headerPos;
            frames.append(std::move(frame));
            offset = frameEnd;
        } else {
            offset = headerPos + m_config.headerSize();
        }

        // Report progress periodically
        if (progressCb) {
            int pct = static_cast<int>(static_cast<qint64>(offset) * 100 / totalSize);
            if (pct != lastPct) {
                progressCb(pct);
                lastPct = pct;
            }
        }
    }

    return frames;
}

int FrameParser::findHeader(const QByteArray &data, int offset) {
    if (m_headerBytes.isEmpty())
        return offset;

    const char *dataPtr = data.constData();
    const char *headerPtr = m_headerBytes.constData();
    const int headerLen = m_headerBytes.size();
    const int limit = data.size() - headerLen;

    for (int i = offset; i <= limit; ++i) {
        if (std::memcmp(dataPtr + i, headerPtr, headerLen) == 0)
            return i;
    }
    return -1;
}

bool FrameParser::tryParseFrame(const QByteArray &data, int offset,
                                ParsedFrame &frame, int &frameEnd) {
    int totalSize = m_config.totalFrameSize();

    if (m_config.hasLengthField()) {
        int lengthFieldOffset = 0;
        int lengthFieldByteCount = 0;
        Endianness lengthEndian = Endianness::LITTLE;
        LengthMeaning lengthMeaning = LengthMeaning::TOTAL;

        int tempOffset = 0;
        for (const auto &f : m_config.fields) {
            if (f.fieldType == FieldType::LENGTH) {
                lengthFieldOffset = tempOffset;
                lengthFieldByteCount = f.byteCount;
                lengthEndian = f.endianness;
                lengthMeaning = f.lengthMeaning;
                break;
            }
            tempOffset += f.byteCount;
        }

        if (offset + lengthFieldOffset + lengthFieldByteCount > data.size())
            return false;

        const char *lp = data.constData() + offset + lengthFieldOffset;
        uint32_t lengthVal = 0;
        if (lengthFieldByteCount == 1) {
            lengthVal = static_cast<uint8_t>(*lp);
        } else if (lengthFieldByteCount == 2) {
            lengthVal = (lengthEndian == Endianness::BIG)
                            ? qFromBigEndian<uint16_t>(lp)
                            : qFromLittleEndian<uint16_t>(lp);
        } else if (lengthFieldByteCount == 4) {
            lengthVal = (lengthEndian == Endianness::BIG)
                            ? qFromBigEndian<uint32_t>(lp)
                            : qFromLittleEndian<uint32_t>(lp);
        }

        if (lengthMeaning == LengthMeaning::TOTAL) {
            totalSize = static_cast<int>(lengthVal);
        } else {
            int nonDataSize = 0;
            for (const auto &f : m_config.fields) {
                if (f.fieldType != FieldType::DATA)
                    nonDataSize += f.byteCount;
            }
            totalSize = static_cast<int>(lengthVal) + nonDataSize;
        }
    }

    if (offset + totalSize > data.size())
        return false;

    QByteArray frameData = data.mid(offset, totalSize);

    if (!validateTail(frameData))
        return false;
    if (!validatePadding(frameData))
        return false;

    frame.crcValid = validateCrc(frameData);

    // Extract all fields
    int fieldOffset = 0;
    for (const auto &fieldDef : m_config.fields) {
        ParsedField pf;
        pf.name = fieldDef.name;
        pf.fieldType = fieldDef.fieldType;
        pf.dataType = fieldDef.dataType;

        if (fieldOffset + fieldDef.byteCount > frameData.size())
            break;

        QByteArray fieldBytes = frameData.mid(fieldOffset, fieldDef.byteCount);
        pf.rawHex = toHexString(fieldBytes);

        switch (fieldDef.fieldType) {
        case FieldType::DATA:
        case FieldType::LENGTH:
            pf.value = extractValue(frameData, fieldOffset, fieldDef.dataType,
                                    fieldDef.byteCount, fieldDef.endianness);
            break;
        case FieldType::HEADER:
        case FieldType::TAIL:
        case FieldType::PADDING:
        case FieldType::CRC:
            pf.value = pf.rawHex;
            break;
        }

        frame.fields.append(std::move(pf));
        fieldOffset += fieldDef.byteCount;
    }

    frameEnd = offset + totalSize;
    return true;
}

bool FrameParser::validateTail(const QByteArray &frameData) {
    QByteArray tail = m_config.tailBytes();
    if (tail.isEmpty())
        return true;
    int tailSize = tail.size();
    if (frameData.size() < tailSize)
        return false;
    return std::memcmp(frameData.constData() + frameData.size() - tailSize,
                       tail.constData(), tailSize) == 0;
}

bool FrameParser::validatePadding(const QByteArray &frameData) {
    int offset = 0;
    for (const auto &f : m_config.fields) {
        if (f.fieldType == FieldType::PADDING && !f.fixedValue.isEmpty()) {
            if (offset + f.byteCount > frameData.size())
                return false;
            if (std::memcmp(frameData.constData() + offset,
                            f.fixedValue.constData(), f.byteCount) != 0)
                return false;
        }
        offset += f.byteCount;
    }
    return true;
}

bool FrameParser::validateCrc(const QByteArray &frameData) {
    for (const auto &f : m_config.fields) {
        if (f.fieldType != FieldType::CRC)
            continue;
        if (f.crcAlgorithm == CrcAlgorithm::NONE)
            continue;

        QByteArray crcData =
            m_config.fieldRange(frameData, f.crcStartField, f.crcEndField);
        if (crcData.isEmpty())
            return false;

        int crcOffset = m_config.fieldOffset(f.index);
        if (crcOffset < 0 || crcOffset + f.byteCount > frameData.size())
            return false;

        QByteArray expectedCrc = frameData.mid(crcOffset, f.byteCount);
        if (!CrcCalculator::verify(f.crcAlgorithm, crcData, expectedCrc,
                                   f.endianness))
            return false;
    }
    return true;
}

int FrameParser::readLengthField(const QByteArray &frameData) {
    for (const auto &f : m_config.fields) {
        if (f.fieldType == FieldType::LENGTH) {
            int offset = m_config.fieldOffset(f.index);
            if (offset < 0)
                return -1;
            QVariant val = extractValue(frameData, offset, f.dataType,
                                        f.byteCount, f.endianness);
            return val.toInt();
        }
    }
    return -1;
}

QVariant FrameParser::extractValue(const QByteArray &data, int offset,
                                   DataType type, int byteCount,
                                   Endianness endian) {
    if (offset + byteCount > data.size())
        return {};

    const char *ptr = data.constData() + offset;

    switch (type) {
    case DataType::UINT8:
        return static_cast<uint>(static_cast<uint8_t>(*ptr));
    case DataType::INT8:
        return static_cast<int>(static_cast<int8_t>(*ptr));
    case DataType::UINT16: {
        uint16_t val = (endian == Endianness::BIG) ? qFromBigEndian<uint16_t>(ptr)
                                                   : qFromLittleEndian<uint16_t>(ptr);
        return static_cast<uint>(val);
    }
    case DataType::INT16: {
        uint16_t raw = (endian == Endianness::BIG) ? qFromBigEndian<uint16_t>(ptr)
                                                   : qFromLittleEndian<uint16_t>(ptr);
        int16_t val;
        std::memcpy(&val, &raw, sizeof(val));
        return static_cast<int>(val);
    }
    case DataType::UINT32: {
        uint32_t val = (endian == Endianness::BIG) ? qFromBigEndian<uint32_t>(ptr)
                                                   : qFromLittleEndian<uint32_t>(ptr);
        return static_cast<quint32>(val);
    }
    case DataType::INT32: {
        uint32_t raw = (endian == Endianness::BIG) ? qFromBigEndian<uint32_t>(ptr)
                                                   : qFromLittleEndian<uint32_t>(ptr);
        int32_t val;
        std::memcpy(&val, &raw, sizeof(val));
        return static_cast<qint32>(val);
    }
    case DataType::FLOAT: {
        uint32_t raw = (endian == Endianness::BIG) ? qFromBigEndian<uint32_t>(ptr)
                                                   : qFromLittleEndian<uint32_t>(ptr);
        float val;
        std::memcpy(&val, &raw, sizeof(val));
        return static_cast<double>(val);
    }
    case DataType::DOUBLE: {
        uint64_t raw = (endian == Endianness::BIG) ? qFromBigEndian<uint64_t>(ptr)
                                                   : qFromLittleEndian<uint64_t>(ptr);
        double val;
        std::memcpy(&val, &raw, sizeof(val));
        return val;
    }
    case DataType::NONE:
        return toHexString(data.mid(offset, byteCount));
    }

    return {};
}

QString FrameParser::toHexString(const QByteArray &data) {
    QString result;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0)
            result += " ";
        result += QString("%1")
                      .arg(static_cast<uint8_t>(data[i]), 2, 16, QChar('0'))
                      .toUpper();
    }
    return result;
}
