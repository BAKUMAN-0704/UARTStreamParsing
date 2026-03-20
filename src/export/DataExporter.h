#ifndef DATAEXPORTER_H
#define DATAEXPORTER_H

#include "../config/FrameFieldDef.h"
#include <QString>
#include <QVector>

class DataExporter {
public:
    static bool exportToTxt(const QString &filePath,
                            const QVector<ParsedFrame> &frames,
                            const FrameConfig &config,
                            QString *errorMsg = nullptr);
};

#endif // DATAEXPORTER_H
