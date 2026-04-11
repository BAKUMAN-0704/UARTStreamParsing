#ifndef STREAMPARSER_H
#define STREAMPARSER_H

#include "../config/FrameFieldDef.h"
#include "FrameParser.h"
#include <QDateTime>
#include <QElapsedTimer>
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
    void flushAndSave(); // save remaining accumulated frames

Q_SIGNALS:
    void frameParsed(const ParsedFrame &frame, const QString &configName);
    void streamProgress(int totalFrames); // throttled, max ~10/sec
    void endFrameDetected();
    void autoSaveCompleted(const QStringList &savedFiles);

private:
    void tryParseBuffer();
    void performAutoSave();
    QStringList saveFramesToDir(const QMap<QString, QVector<ParsedFrame>> &frames);
    void emitThrottledProgress();
    int findEarliestHeaderPos(const QByteArray &data, int offset) const;
    bool selectMatchingConfig(const QByteArray &data, int headerPos, int &configIdx,
                              int &frameSize, ParsedFrame &frame,
                              bool &needMoreData);

    static int findHeaderIn(const QByteArray &data, int offset,
                            const QByteArray &headerBytes);
    static int computeFrameSize(const FrameConfig &config, const QByteArray &data,
                                int offset);

    QVector<ConfigEntry> m_configs;
    QVector<FrameParser> m_parsers;
    QVector<QByteArray> m_cachedHeaders;
    QByteArray m_buffer;
    QMap<QString, QVector<ParsedFrame>> m_accumulatedFrames;
    QMap<QString, int> m_frameCounters;
    QString m_autoSaveDir;
    int m_maxHeaderSize = 0;
    bool m_streamingMode = false;

    // Throttle: emit streamProgress at most every 100ms
    QElapsedTimer m_progressTimer;
    int m_totalStreamFrames = 0;
};

#endif // STREAMPARSER_H
