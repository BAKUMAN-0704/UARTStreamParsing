#ifndef FRAMEPARSER_H
#define FRAMEPARSER_H

#include "../config/FrameFieldDef.h"
#include <QByteArray>
#include <QVariant>
#include <functional>

class FrameParser {
public:
    using ProgressCallback = std::function<void(int percent)>;

    void setConfig(const FrameConfig &config);
    void setLightweight(bool on) { m_lightweight = on; }
    QVector<ParsedFrame> parse(const QByteArray &rawData,
                               ProgressCallback progressCb = nullptr);

private:
    FrameConfig m_config;
    bool m_lightweight = false; // skip rawHex generation for streaming

    bool tryParseFrame(const QByteArray &data, int offset, ParsedFrame &frame,
                       int &frameEnd);
    QVariant extractValue(const QByteArray &data, int offset, DataType type,
                          int byteCount, Endianness endian);
    int findHeader(const QByteArray &data, int offset);
    bool validateCrc(const QByteArray &frameData);
    bool validateTail(const QByteArray &frameData);
    bool validatePadding(const QByteArray &frameData);
    int readLengthField(const QByteArray &frameData);
    static QString toHexString(const QByteArray &data);

    // Cached header bytes for fast searching
    QByteArray m_headerBytes;
};

#endif // FRAMEPARSER_H
