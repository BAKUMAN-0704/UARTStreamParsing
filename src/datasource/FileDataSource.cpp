#include "FileDataSource.h"
#include <QFile>
#include <QRegularExpression>
#include <cstring>

static inline int hexCharVal(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

static inline bool isSep(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == ';';
}

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
    bool pendingHigh = false; // carry-over: have high nibble from previous block
    int highNibble = 0;
    bool inPrefix = false; // just saw '0', waiting for 'x'/'X'
    bool inToken = false;

    while (!file.atEnd()) {
        qint64 n = file.read(block.data(), BLOCK_SIZE);
        if (n <= 0)
            break;
        totalRead += n;

        const char *p = block.constData();
        for (qint64 i = 0; i < n; ++i) {
            char c = p[i];

            if (isSep(c)) {
                // Separator: flush pending high nibble as single-digit byte
                if (pendingHigh) {
                    result.append(static_cast<char>(highNibble));
                    pendingHigh = false;
                }
                inToken = false;
                inPrefix = false;
                continue;
            }

            // Handle "0x" prefix
            if (!inToken && c == '0') {
                inPrefix = true;
                inToken = true;
                continue;
            }
            if (inPrefix && (c == 'x' || c == 'X')) {
                inPrefix = false;
                continue;
            }
            inPrefix = false;
            inToken = true;

            int val = hexCharVal(c);
            if (val < 0) {
                // Non-hex char: flush pending and skip
                if (pendingHigh) {
                    result.append(static_cast<char>(highNibble));
                    pendingHigh = false;
                }
                inToken = false;
                continue;
            }

            if (!pendingHigh) {
                highNibble = val;
                pendingHigh = true;
            } else {
                result.append(static_cast<char>((highNibble << 4) | val));
                pendingHigh = false;
            }
        }

        if (progressCb && fileSize > 0) {
            int pct = static_cast<int>(totalRead * 100 / fileSize);
            if (pct != lastPct) {
                progressCb(pct);
                lastPct = pct;
            }
        }
    }

    // Flush any remaining nibble
    if (pendingHigh)
        result.append(static_cast<char>(highNibble));

    return result;
}

QByteArray FileDataSource::parseHexString(const QString &hexText, QString *errorMsg) {
    QByteArray result;
    static QRegularExpression separator("[\\s,;]+");
    QStringList tokens = hexText.split(separator, Qt::SkipEmptyParts);

    for (const QString &token : tokens) {
        QString hex = token.trimmed();
        if (hex.isEmpty())
            continue;
        if (hex.startsWith("0x", Qt::CaseInsensitive))
            hex = hex.mid(2);
        if (hex.length() % 2 != 0)
            hex = "0" + hex;

        for (int i = 0; i < hex.length(); i += 2) {
            QString byteStr = hex.mid(i, 2);
            bool ok;
            uint8_t byte = static_cast<uint8_t>(byteStr.toUInt(&ok, 16));
            if (!ok) {
                if (errorMsg)
                    *errorMsg = QString("无效的16进制值: %1").arg(token);
                return {};
            }
            result.append(static_cast<char>(byte));
        }
    }

    return result;
}
