#include "FileDataSource.h"
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
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

    qint64 bytesRead = 0;
    int lastPct = -1;

    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        bytesRead += line.size();

        const char *p = line.constData();
        const int len = line.size();
        int i = 0;

        while (i < len) {
            // Skip separators
            while (i < len && isSep(p[i]))
                ++i;
            if (i >= len)
                break;

            // Skip "0x" or "0X" prefix
            if (i + 1 < len && p[i] == '0' && (p[i + 1] == 'x' || p[i + 1] == 'X'))
                i += 2;

            // Collect consecutive hex digits
            int tokenStart = i;
            while (i < len && hexCharVal(p[i]) >= 0)
                ++i;
            int tokenLen = i - tokenStart;

            if (tokenLen == 0) {
                ++i; // skip invalid character
                continue;
            }

            // Parse hex digit pairs
            int j = tokenStart;
            if (tokenLen % 2 != 0) {
                // Odd: treat first digit as a single byte
                result.append(static_cast<char>(hexCharVal(p[j])));
                ++j;
            }
            for (; j + 1 <= tokenStart + tokenLen; j += 2) {
                int hi = hexCharVal(p[j]);
                int lo = hexCharVal(p[j + 1]);
                result.append(static_cast<char>((hi << 4) | lo));
            }
        }

        if (progressCb && fileSize > 0) {
            int pct = static_cast<int>(bytesRead * 100 / fileSize);
            if (pct != lastPct) {
                progressCb(pct);
                lastPct = pct;
            }
        }
    }

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
