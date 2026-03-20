#include "FileDataSource.h"
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

QByteArray FileDataSource::readHexFile(const QString &filePath, QString *errorMsg) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMsg)
            *errorMsg = QString("无法打开文件: %1").arg(file.errorString());
        return {};
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    return parseHexString(content, errorMsg);
}

QByteArray FileDataSource::parseHexString(const QString &hexText, QString *errorMsg) {
    QByteArray result;
    // Split by whitespace, commas, semicolons, or newlines
    static QRegularExpression separator("[\\s,;]+");
    QStringList tokens = hexText.split(separator, Qt::SkipEmptyParts);

    for (const QString &token : tokens) {
        QString hex = token.trimmed();
        if (hex.isEmpty())
            continue;

        // Remove "0x" or "0X" prefix
        if (hex.startsWith("0x", Qt::CaseInsensitive))
            hex = hex.mid(2);

        // Parse pairs of hex digits
        if (hex.length() % 2 != 0) {
            hex = "0" + hex; // pad with leading zero
        }

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
