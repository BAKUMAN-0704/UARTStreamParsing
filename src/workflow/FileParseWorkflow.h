#ifndef FILEPARSEWORKFLOW_H
#define FILEPARSEWORKFLOW_H

#include "../worker/FileParseService.h"

struct FileParseCompletionView {
    QByteArray rawData;
    QMap<QString, QVector<ParsedFrame>> framesByConfig;
    QString statusMessage;
    bool exportEnabled = false;
    bool showEmptyResultMessage = false;
    bool showResultDialog = false;
};

class FileParseWorkflow {
public:
    FileParseCompletionView completeFileParse(const FileParseResult &result) const;
};

#endif // FILEPARSEWORKFLOW_H
