#ifndef CRCCALCULATOR_H
#define CRCCALCULATOR_H

#include "../config/FrameFieldDef.h"
#include <QByteArray>
#include <cstdint>

class CrcCalculator {
public:
    static uint8_t crc8(const QByteArray &data);
    static uint16_t crc16Modbus(const QByteArray &data);
    static uint16_t crc16Ccitt(const QByteArray &data);
    static uint32_t crc32(const QByteArray &data);

    // Calculate CRC and return as QByteArray with specified endianness
    static QByteArray calculate(CrcAlgorithm algo, const QByteArray &data,
                                Endianness endian);

    // Verify CRC: compare calculated vs expected
    static bool verify(CrcAlgorithm algo, const QByteArray &data,
                       const QByteArray &expected, Endianness endian);
};

#endif // CRCCALCULATOR_H
