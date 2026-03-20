#ifndef STREAMPARSER_H
#define STREAMPARSER_H

#include "../config/FrameFieldDef.h"
#include "FrameParser.h"
#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QVector>
#include <functional>

struct ConfigEntry {
    QString name;
    QString filePath;
    FrameConfig config;
    bool isEndFrame = false;
};

class StreamParser : public QObject {
    Q_OBJECT
public:
    using ProgressCallback = std::function<void(int percent)>;

    explicit StreamParser(QObject *parent = nullptr);

    // Config management
    void addConfig(const ConfigEntry &entry);
    void removeConfig(const QString &name);
    void clearConfigs();
    void setEndFrameConfig(const QString &name);
    const QVector<ConfigEntry> &configs() const { return m_configs; }

    // Streaming mode (serial)
    void feedData(const QByteArray &chunk);
    void resetStream();

    // Batch mode (file)
    QMap<QString, QVector<ParsedFrame>> parseBatch(const QByteArray &rawData,
                                                   ProgressCallback progressCb = nullptr);

    // Accumulated frames
    const QMap<QString, QVector<ParsedFrame>> &accumulatedFrames() const {
        return m_accumulatedFrames;
    }
    void clearAccumulatedFrames();

    // Auto-save
    void setAutoSaveDir(const QString &dir) { m_autoSaveDir = dir; }
    QString autoSaveDir() const { return m_autoSaveDir; }

Q_SIGNALS:
    void frameParsed(const ParsedFrame &frame, const QString &configName);
    void endFrameDetected();
    void autoSaveCompleted(const QStringList &savedFiles);

private:
    void tryParseBuffer();
    void performAutoSave();

    static int findHeaderIn(const QByteArray &data, int offset,
                            const QByteArray &headerBytes);
    static int computeFrameSize(const FrameConfig &config, const QByteArray &data,
                                int offset);

    QVector<ConfigEntry> m_configs;
    QVector<FrameParser> m_parsers;
    QByteArray m_buffer;
    QMap<QString, QVector<ParsedFrame>> m_accumulatedFrames;
    QMap<QString, int> m_frameCounters;
    QString m_autoSaveDir;
    int m_maxHeaderSize = 0;
};

#endif // STREAMPARSER_H
