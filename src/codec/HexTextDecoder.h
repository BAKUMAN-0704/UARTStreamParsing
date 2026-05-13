#ifndef HEXTEXTDECODER_H
#define HEXTEXTDECODER_H

#include <QByteArray>
#include <QString>

class HexTextDecoder {
public:
    QByteArray append(const QByteArray &text);
    QByteArray finish();
    void reset();

    static QByteArray decodeAll(const QByteArray &text);
    static QByteArray decodeAll(const QString &text);

private:
    void flushPending(QByteArray &out);
    void emitPendingZero(QByteArray &out);

    int m_pendingNibble = -1;
    bool m_pendingZeroPrefix = false;
    bool m_inToken = false;
};

#endif // HEXTEXTDECODER_H
