#ifndef FILEDATASOURCE_H
#define FILEDATASOURCE_H

#include <QByteArray>
#include <QString>
#include <functional>

class FileDataSource {
public:
    using ProgressCallback = std::function<void(int percent)>;

    // Read hex data from a TXT file (streaming, line by line)
    static QByteArray readHexFile(const QString &filePath,
                                  QString *errorMsg = nullptr,
                                  ProgressCallback progressCb = nullptr);

    // Parse a hex string into raw bytes (for small data / serial)
    static QByteArray parseHexString(const QString &hexText,
                                     QString *errorMsg = nullptr);
};

#endif // FILEDATASOURCE_H
