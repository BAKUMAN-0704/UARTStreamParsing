#include "FrameParser.h"
#include "CrcCalculator.h"
#include <QtEndian>
#include <cstring>

void FrameParser::setConfig(const FrameConfig &config) { m_config = config; }

QVector<ParsedFrame> FrameParser::parse(const QByteArray &rawData) {
    QVector<ParsedFrame> frames;
    if (m_config.fields.isEmpty() || rawData.isEmpty())
        return frames;

    int offset = 0;
    int frameCounter = 0;

    while (offset < rawData.size()) {
        // Find next header
        int headerPos = findHeader(rawData, offset);
        if (headerPos < 0)
            break;

        ParsedFrame frame;
        int frameEnd = 0;
        if (tryParseFrame(rawData, headerPos, frame, frameEnd)) {
            frame.frameIndex = ++frameCounter;
            frame.offsetInStream = headerPos;
            frames.append(frame);
            offset = frameEnd;
        } else {
            // Header found but frame invalid, skip past this header
            offset = headerPos + m_config.headerSize();
        }
    }

    return frames;
}

int FrameParser::findHeader(const QByteArray &data, int offset) {
    QByteArray header = m_config.headerBytes();
    if (header.isEmpty())
        return offset; // no header defined

    for (int i = offset; i <= data.size() - header.size(); ++i) {
        if (data.mid(i, header.size()) == header)
            return i;
    }
    return -1;
}

bool FrameParser::tryParseFrame(const QByteArray &data, int offset,
                                ParsedFrame &frame, int &frameEnd) {
    int totalSize = m_config.totalFrameSize();

    // If there's a LENGTH field, we need to read it to determine actual frame size
    if (m_config.hasLengthField()) {
        // First check if we have enough bytes to reach the LENGTH field
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

        // Read length value
        QByteArray lengthBytes =
            data.mid(offset + lengthFieldOffset, lengthFieldByteCount);
        uint32_t lengthVal = 0;
        if (lengthFieldByteCount == 1) {
            lengthVal = static_cast<uint8_t>(lengthBytes[0]);
        } else if (lengthFieldByteCount == 2) {
            if (lengthEndian == Endianness::BIG)
                lengthVal = qFromBigEndian<uint16_t>(lengthBytes.constData());
            else
                lengthVal = qFromLittleEndian<uint16_t>(lengthBytes.constData());
        } else if (lengthFieldByteCount == 4) {
            if (lengthEndian == Endianness::BIG)
                lengthVal = qFromBigEndian<uint32_t>(lengthBytes.constData());
            else
                lengthVal = qFromLittleEndian<uint32_t>(lengthBytes.constData());
        }

        if (lengthMeaning == LengthMeaning::TOTAL) {
            totalSize = static_cast<int>(lengthVal);
        } else {
            // PAYLOAD: length is data area only, add non-data field sizes
            int nonDataSize = 0;
            for (const auto &f : m_config.fields) {
                if (f.fieldType != FieldType::DATA)
                    nonDataSize += f.byteCount;
            }
            totalSize = static_cast<int>(lengthVal) + nonDataSize;
        }
    }

    // Check if we have enough data
    if (offset + totalSize > data.size())
        return false;

    QByteArray frameData = data.mid(offset, totalSize);

    // Validate tail
    if (!validateTail(frameData))
        return false;

    // Validate padding fields
    if (!validatePadding(frameData))
        return false;

    // Validate CRC
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
            pf.value = extractValue(frameData, fieldOffset, fieldDef.dataType,
                                    fieldDef.byteCount, fieldDef.endianness);
            break;
        case FieldType::LENGTH: {
            pf.value = extractValue(frameData, fieldOffset, fieldDef.dataType,
                                    fieldDef.byteCount, fieldDef.endianness);
            break;
        }
        case FieldType::HEADER:
        case FieldType::TAIL:
        case FieldType::PADDING:
        case FieldType::CRC:
            pf.value = pf.rawHex;
            break;
        }

        frame.fields.append(pf);
        fieldOffset += fieldDef.byteCount;
    }

    frameEnd = offset + totalSize;
    return true;
}

bool FrameParser::validateTail(const QByteArray &frameData) {
    QByteArray tail = m_config.tailBytes();
    if (tail.isEmpty())
        return true;

    int tailSize = m_config.tailSize();
    if (frameData.size() < tailSize)
        return false;

    QByteArray frameTail = frameData.right(tailSize);
    return frameTail == tail;
}

bool FrameParser::validatePadding(const QByteArray &frameData) {
    int offset = 0;
    for (const auto &f : m_config.fields) {
        if (f.fieldType == FieldType::PADDING && !f.fixedValue.isEmpty()) {
            if (offset + f.byteCount > frameData.size())
                return false;
            QByteArray actual = frameData.mid(offset, f.byteCount);
            if (actual != f.fixedValue)
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

        // Get the data range for CRC calculation
        QByteArray crcData =
            m_config.fieldRange(frameData, f.crcStartField, f.crcEndField);
        if (crcData.isEmpty())
            return false;

        // Get the CRC bytes from the frame
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
            QVariant val =
                extractValue(frameData, offset, f.dataType, f.byteCount, f.endianness);
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
        uint16_t val;
        if (endian == Endianness::BIG)
            val = qFromBigEndian<uint16_t>(ptr);
        else
            val = qFromLittleEndian<uint16_t>(ptr);
        return static_cast<uint>(val);
    }
    case DataType::INT16: {
        int16_t val;
        uint16_t raw;
        if (endian == Endianness::BIG)
            raw = qFromBigEndian<uint16_t>(ptr);
        else
            raw = qFromLittleEndian<uint16_t>(ptr);
        std::memcpy(&val, &raw, sizeof(val));
        return static_cast<int>(val);
    }
    case DataType::UINT32: {
        uint32_t val;
        if (endian == Endianness::BIG)
            val = qFromBigEndian<uint32_t>(ptr);
        else
            val = qFromLittleEndian<uint32_t>(ptr);
        return static_cast<quint32>(val);
    }
    case DataType::INT32: {
        int32_t val;
        uint32_t raw;
        if (endian == Endianness::BIG)
            raw = qFromBigEndian<uint32_t>(ptr);
        else
            raw = qFromLittleEndian<uint32_t>(ptr);
        std::memcpy(&val, &raw, sizeof(val));
        return static_cast<qint32>(val);
    }
    case DataType::FLOAT: {
        uint32_t raw;
        if (endian == Endianness::BIG)
            raw = qFromBigEndian<uint32_t>(ptr);
        else
            raw = qFromLittleEndian<uint32_t>(ptr);
        float val;
        std::memcpy(&val, &raw, sizeof(val));
        return static_cast<double>(val);
    }
    case DataType::DOUBLE: {
        uint64_t raw;
        if (endian == Endianness::BIG)
            raw = qFromBigEndian<uint64_t>(ptr);
        else
            raw = qFromLittleEndian<uint64_t>(ptr);
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
        result += QString("%1").arg(static_cast<uint8_t>(data[i]), 2, 16, QChar('0')).toUpper();
    }
    return result;
}
