#ifndef DATAEXPORTER_H
#define DATAEXPORTER_H

#include "../config/FrameFieldDef.h"
#include <QMap>
#include <QString>
#include <QVector>

class DataExporter {
public:
    static bool exportToTxt(const QString &filePath,
                            const QVector<ParsedFrame> &frames,
                            const FrameConfig &config,
                            QString *errorMsg = nullptr);

    // Multi-config export: one file per config in dirPath
    // Returns list of saved file paths
    static QStringList autoSaveMultiConfig(
        const QString &dirPath,
        const QMap<QString, QVector<ParsedFrame>> &framesByConfig,
        const QMap<QString, FrameConfig> &configMap,
        const QString &endFrameConfigName = QString(),
        QString *errorMsg = nullptr);
};

#endif // DATAEXPORTER_H
