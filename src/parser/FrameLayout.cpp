#include "FrameLayout.h"
#include <QtEndian>

namespace {

uint32_t readLengthValue(const char *ptr, int byteCount, Endianness endian) {
    uint32_t lengthVal = 0;
    if (byteCount == 1) {
        lengthVal = static_cast<uint8_t>(*ptr);
    } else if (byteCount == 2) {
        lengthVal = endian == Endianness::BIG ? qFromBigEndian<uint16_t>(ptr)
                                              : qFromLittleEndian<uint16_t>(ptr);
    } else if (byteCount == 4) {
        lengthVal = endian == Endianness::BIG ? qFromBigEndian<uint32_t>(ptr)
                                              : qFromLittleEndian<uint32_t>(ptr);
    }
    return lengthVal;
}

int nonDataSize(const FrameConfig &config) {
    int size = 0;
    for (const auto &field : config.fields) {
        if (field.fieldType != FieldType::DATA)
            size += field.byteCount;
    }
    return size;
}

}

namespace FrameLayout {

FrameSizeResult resolveFrameSize(const FrameConfig &config,
                                 const QByteArray &data,
                                 int offset,
                                 IncompleteLengthPolicy incompletePolicy) {
    if (!config.hasLengthField())
        return {config.totalFrameSize(), true, false, false};

    for (const auto &field : config.fields) {
        if (field.fieldType != FieldType::LENGTH)
            continue;

        const int fieldOffset = config.fieldOffset(field.index);
        if (fieldOffset < 0 || offset + fieldOffset + field.byteCount > data.size()) {
            if (incompletePolicy == IncompleteLengthPolicy::UseStaticLayoutSize)
                return {config.totalFrameSize(), true, true, true};
            return {0, false, true, true};
        }

        const char *lengthPtr = data.constData() + offset + fieldOffset;
        const uint32_t lengthVal = readLengthValue(lengthPtr, field.byteCount, field.endianness);
        const int size = field.lengthMeaning == LengthMeaning::TOTAL
                             ? static_cast<int>(lengthVal)
                             : static_cast<int>(lengthVal) + nonDataSize(config);
        return {size, true, true, false};
    }

    return {config.totalFrameSize(), true, false, false};
}

}
