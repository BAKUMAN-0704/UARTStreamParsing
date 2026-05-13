#ifndef PARSEWORKER_H
#define PARSEWORKER_H

#include "FileParseService.h"
#include <QObject>

class ParseWorker : public QObject {
    Q_OBJECT
public:
    explicit ParseWorker(QObject *parent = nullptr) : QObject(parent) {}

    void setConfigs(const QVector<ConfigEntry> &configs) { m_configs = configs; }
    void setAutoSaveDir(const QString &dir) { m_autoSaveDir = dir; }
    void setAutoSaveSequenceStart(int start) { m_autoSaveSequenceStart = start; }
    const FileParseResult &result() const { return m_result; }

public slots:
    void process(const QString &filePath);

signals:
    void progress(int percent, const QString &status);
    void finished(bool success, const QString &errorMsg);

private:
    QVector<ConfigEntry> m_configs;
    QString m_autoSaveDir;
    int m_autoSaveSequenceStart = 1;
    FileParseResult m_result;
};

#endif // PARSEWORKER_H
