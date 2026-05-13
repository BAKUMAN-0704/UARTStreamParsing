#ifndef PARSEEXPORTCONTROLWORKFLOW_H
#define PARSEEXPORTCONTROLWORKFLOW_H

#include "../config/FrameFieldDef.h"
#include <QMap>
#include <QString>
#include <QVector>

enum class ParseExportSourceMode {
    File,
    Serial,
};

struct ParseExportControlInput {
    ParseExportSourceMode sourceMode = ParseExportSourceMode::File;
    bool hasConfigs = false;
    bool hasRawData = false;
    bool serialOpen = false;
    bool hasFilePath = false;
    bool streamingActive = false;
    bool workerActive = false;
    bool parsing = false;
    bool hasExportableFrames = false;
};

struct ParseExportControlView {
    int sourcePageIndex = 0;
    bool progressVisible = false;
    int progressValue = 0;
    bool parseEnabled = false;
    bool exportEnabled = false;
    bool browseConfigEnabled = true;
    bool browseDataFileEnabled = true;
};

class ParseExportControlWorkflow {
public:
    ParseExportControlView resolve(const ParseExportControlInput &input) const;
    bool hasExportableFrames(const QMap<QString, QVector<ParsedFrame>> &framesByConfig) const;
};

#endif // PARSEEXPORTCONTROLWORKFLOW_H
