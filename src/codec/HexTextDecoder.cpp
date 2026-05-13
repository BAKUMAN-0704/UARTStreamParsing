#include "HexTextDecoder.h"

static int hexCharValue(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

static bool isHexSeparator(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == ';';
}

QByteArray HexTextDecoder::append(const QByteArray &text) {
    QByteArray result;
    result.reserve(text.size() / 2);

    for (char c : text) {
        if (isHexSeparator(c)) {
            flushPending(result);
            continue;
        }

        if (m_pendingZeroPrefix) {
            m_pendingZeroPrefix = false;
            if (c == 'x' || c == 'X') {
                m_inToken = true;
                continue;
            }
            m_pendingNibble = 0;
        }

        const int value = hexCharValue(c);
        if (value < 0) {
            flushPending(result);
            continue;
        }

        if (!m_inToken && value == 0 && m_pendingNibble < 0) {
            m_pendingZeroPrefix = true;
            m_inToken = true;
            continue;
        }

        if (m_pendingNibble < 0) {
            m_pendingNibble = value;
        } else {
            result.append(static_cast<char>((m_pendingNibble << 4) | value));
            m_pendingNibble = -1;
        }
        m_inToken = true;
    }

    return result;
}

QByteArray HexTextDecoder::finish() {
    QByteArray result;
    flushPending(result);
    return result;
}

void HexTextDecoder::reset() {
    m_pendingNibble = -1;
    m_pendingZeroPrefix = false;
    m_inToken = false;
}

QByteArray HexTextDecoder::decodeAll(const QByteArray &text) {
    HexTextDecoder decoder;
    QByteArray result = decoder.append(text);
    result.append(decoder.finish());
    return result;
}

QByteArray HexTextDecoder::decodeAll(const QString &text) {
    return decodeAll(text.toUtf8());
}

void HexTextDecoder::flushPending(QByteArray &out) {
    if (m_pendingZeroPrefix)
        emitPendingZero(out);
    if (m_pendingNibble >= 0) {
        out.append(static_cast<char>(m_pendingNibble));
        m_pendingNibble = -1;
    }
    m_inToken = false;
}

void HexTextDecoder::emitPendingZero(QByteArray &out) {
    out.append(static_cast<char>(0));
    m_pendingZeroPrefix = false;
}
