#ifndef FILEDATASOURCE_H
#define FILEDATASOURCE_H

#include <QByteArray>
#include <QString>

class FileDataSource {
public:
    // Read hex data from a TXT file
    // Supports formats: "0x11 0x22", "11 22", "11,22", mixed
    static QByteArray readHexFile(const QString &filePath, QString *errorMsg = nullptr);

    // Parse a hex string into raw bytes
    static QByteArray parseHexString(const QString &hexText, QString *errorMsg = nullptr);
};

#endif // FILEDATASOURCE_H
