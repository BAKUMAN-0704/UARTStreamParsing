#ifndef FRAMEPARSER_H
#define FRAMEPARSER_H

#include "../config/FrameFieldDef.h"
#include <QByteArray>
#include <QVariant>

class FrameParser {
public:
    void setConfig(const FrameConfig &config);
    QVector<ParsedFrame> parse(const QByteArray &rawData);

private:
    FrameConfig m_config;

    // Try to parse one frame starting at the given offset
    // Returns true if successful, sets frameEnd to the byte after the frame
    bool tryParseFrame(const QByteArray &data, int offset, ParsedFrame &frame,
                       int &frameEnd);

    // Extract a typed value from raw bytes
    QVariant extractValue(const QByteArray &data, int offset, DataType type,
                          int byteCount, Endianness endian);

    // Find next header occurrence starting from offset
    int findHeader(const QByteArray &data, int offset);

    // Validate CRC for a frame
    bool validateCrc(const QByteArray &frameData);

    // Validate tail bytes
    bool validateTail(const QByteArray &frameData);

    // Read length field value from frame data
    int readLengthField(const QByteArray &frameData);

    // Convert raw bytes to hex string
    static QString toHexString(const QByteArray &data);
};

#endif // FRAMEPARSER_H
