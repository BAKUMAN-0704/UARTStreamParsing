#ifndef FILEPARSESERVICE_H
#define FILEPARSESERVICE_H

#include "../parser/StreamParser.h"
#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

struct FileParseRequest {
    QString filePath;
    QVector<ConfigEntry> configs;
    QString autoSaveDir;
    int autoSaveSequenceStart = 1;
};

struct FileParseResult {
    bool success = false;
    QString errorMsg;
    QByteArray rawData;
    QMap<QString, QVector<ParsedFrame>> framesByConfig;
    QStringList autoSavedFiles;

    int totalFrames() const;
};

class FileParseService {
public:
    using ProgressCallback = std::function<void(int percent, const QString &status)>;

    FileParseResult parse(const FileParseRequest &request,
                          ProgressCallback progressCb = nullptr) const;
};

#endif // FILEPARSESERVICE_H
