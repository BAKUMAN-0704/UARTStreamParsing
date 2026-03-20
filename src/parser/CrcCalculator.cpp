#include "CrcCalculator.h"
#include <QtEndian>

uint8_t CrcCalculator::crc8(const QByteArray &data) {
    uint8_t crc = 0x00;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<uint8_t>(data[i]);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}

uint16_t CrcCalculator::crc16Modbus(const QByteArray &data) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<uint8_t>(data[i]);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

uint16_t CrcCalculator::crc16Ccitt(const QByteArray &data) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= (static_cast<uint8_t>(data[i]) << 8);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

uint32_t CrcCalculator::crc32(const QByteArray &data) {
    uint32_t crc = 0xFFFFFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<uint8_t>(data[i]);
        for (int j = 0; j < 8; ++j) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

QByteArray CrcCalculator::calculate(CrcAlgorithm algo, const QByteArray &data,
                                    Endianness endian) {
    QByteArray result;
    switch (algo) {
    case CrcAlgorithm::CRC8: {
        uint8_t val = crc8(data);
        result.append(static_cast<char>(val));
        break;
    }
    case CrcAlgorithm::CRC16_MODBUS: {
        uint16_t val = crc16Modbus(data);
        result.resize(2);
        if (endian == Endianness::BIG)
            qToBigEndian(val, result.data());
        else
            qToLittleEndian(val, result.data());
        break;
    }
    case CrcAlgorithm::CRC16_CCITT: {
        uint16_t val = crc16Ccitt(data);
        result.resize(2);
        if (endian == Endianness::BIG)
            qToBigEndian(val, result.data());
        else
            qToLittleEndian(val, result.data());
        break;
    }
    case CrcAlgorithm::CRC32: {
        uint32_t val = crc32(data);
        result.resize(4);
        if (endian == Endianness::BIG)
            qToBigEndian(val, result.data());
        else
            qToLittleEndian(val, result.data());
        break;
    }
    case CrcAlgorithm::NONE:
        break;
    }
    return result;
}

bool CrcCalculator::verify(CrcAlgorithm algo, const QByteArray &data,
                           const QByteArray &expected, Endianness endian) {
    if (algo == CrcAlgorithm::NONE)
        return true;
    QByteArray calculated = calculate(algo, data, endian);
    return calculated == expected;
}
