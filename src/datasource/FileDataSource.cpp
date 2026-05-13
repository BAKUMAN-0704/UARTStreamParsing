#include "FileDataSource.h"
#include "../codec/HexTextDecoder.h"
#include <QFile>

QByteArray FileDataSource::readHexFile(const QString &filePath, QString *errorMsg,
                                       ProgressCallback progressCb) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMsg)
            *errorMsg = QString("无法打开文件: %1").arg(file.errorString());
        return {};
    }

    const qint64 fileSize = file.size();
    QByteArray result;
    result.reserve(static_cast<int>(fileSize / 4));

    // Block-based reading for much better performance on large files
    constexpr int BLOCK_SIZE = 256 * 1024; // 256KB blocks
    QByteArray block(BLOCK_SIZE, Qt::Uninitialized);
    qint64 totalRead = 0;
    int lastPct = -1;
    HexTextDecoder decoder;

    while (!file.atEnd()) {
        qint64 n = file.read(block.data(), BLOCK_SIZE);
        if (n <= 0)
            break;
        totalRead += n;

        result.append(decoder.append(block.left(static_cast<int>(n))));

        if (progressCb && fileSize > 0) {
            int pct = static_cast<int>(totalRead * 100 / fileSize);
            if (pct != lastPct) {
                progressCb(pct);
                lastPct = pct;
            }
        }
    }

    result.append(decoder.finish());
    return result;
}

QByteArray FileDataSource::parseHexString(const QString &hexText, QString *errorMsg) {
    if (errorMsg)
        errorMsg->clear();
    return HexTextDecoder::decodeAll(hexText);
}
