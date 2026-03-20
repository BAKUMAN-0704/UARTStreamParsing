#ifndef FRAMEFIELDDEF_H
#define FRAMEFIELDDEF_H

#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QVector>

enum class FieldType { HEADER, TAIL, LENGTH, DATA, CRC, PADDING };

enum class DataType { UINT8, INT8, UINT16, INT16, UINT32, INT32, FLOAT, DOUBLE, NONE };

enum class Endianness { BIG, LITTLE };

enum class CrcAlgorithm { NONE, CRC8, CRC16_MODBUS, CRC16_CCITT, CRC32 };

enum class LengthMeaning { TOTAL, PAYLOAD };

struct FrameFieldDef {
    int index = 0;
    QString name;
    FieldType fieldType = FieldType::DATA;
    DataType dataType = DataType::NONE;
    int byteCount = 0;
    QByteArray fixedValue;
    Endianness endianness = Endianness::LITTLE;
    CrcAlgorithm crcAlgorithm = CrcAlgorithm::NONE;
    int crcStartField = 0;
    int crcEndField = 0;
    LengthMeaning lengthMeaning = LengthMeaning::TOTAL;
    QString note;
};

struct FrameConfig {
    QVector<FrameFieldDef> fields;

    int totalFrameSize() const {
        int size = 0;
        for (const auto &f : fields)
            size += f.byteCount;
        return size;
    }

    QByteArray headerBytes() const {
        QByteArray result;
        for (const auto &f : fields) {
            if (f.fieldType == FieldType::HEADER)
                result.append(f.fixedValue);
        }
        return result;
    }

    QByteArray tailBytes() const {
        QByteArray result;
        for (const auto &f : fields) {
            if (f.fieldType == FieldType::TAIL)
                result.append(f.fixedValue);
        }
        return result;
    }

    int headerSize() const {
        int size = 0;
        for (const auto &f : fields) {
            if (f.fieldType == FieldType::HEADER)
                size += f.byteCount;
        }
        return size;
    }

    int tailSize() const {
        int size = 0;
        for (const auto &f : fields) {
            if (f.fieldType == FieldType::TAIL)
                size += f.byteCount;
        }
        return size;
    }

    // Get the byte offset of a field by its index (1-based)
    int fieldOffset(int fieldIndex) const {
        int offset = 0;
        for (const auto &f : fields) {
            if (f.index == fieldIndex)
                return offset;
            offset += f.byteCount;
        }
        return -1;
    }

    // Get total bytes covered by fields from startIdx to endIdx (1-based)
    QByteArray fieldRange(const QByteArray &frameData, int startIdx, int endIdx) const {
        int offset = 0;
        int rangeStart = -1;
        int rangeEnd = -1;
        for (const auto &f : fields) {
            if (f.index == startIdx)
                rangeStart = offset;
            if (f.index == endIdx)
                rangeEnd = offset + f.byteCount;
            offset += f.byteCount;
        }
        if (rangeStart >= 0 && rangeEnd > rangeStart && rangeEnd <= frameData.size())
            return frameData.mid(rangeStart, rangeEnd - rangeStart);
        return {};
    }

    bool hasLengthField() const {
        for (const auto &f : fields) {
            if (f.fieldType == FieldType::LENGTH)
                return true;
        }
        return false;
    }

    bool hasCrcField() const {
        for (const auto &f : fields) {
            if (f.fieldType == FieldType::CRC)
                return true;
        }
        return false;
    }
};

struct ParsedField {
    QString name;
    QVariant value;
    QString rawHex;
    DataType dataType = DataType::NONE;
    FieldType fieldType = FieldType::DATA;
};

struct ParsedFrame {
    int frameIndex = 0;
    int offsetInStream = 0;
    QVector<ParsedField> fields;
    bool crcValid = true;
};

#endif // FRAMEFIELDDEF_H
