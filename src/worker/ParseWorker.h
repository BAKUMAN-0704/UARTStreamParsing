#ifndef PARSEWORKER_H
#define PARSEWORKER_H

#include "../config/FrameFieldDef.h"
#include "../parser/StreamParser.h"
#include <QByteArray>
#include <QMap>
#include <QObject>
#include <QVector>

class ParseWorker : public QObject {
    Q_OBJECT
public:
    explicit ParseWorker(QObject *parent = nullptr) : QObject(parent) {}

    void setConfigs(const QVector<ConfigEntry> &configs) { m_configs = configs; }

    // Results (safe to read from main thread after finished() is emitted)
    QMap<QString, QVector<ParsedFrame>> m_framesByConfig;
    QByteArray m_rawData;

public slots:
    void process(const QString &filePath);

signals:
    void progress(int percent, const QString &status);
    void finished(bool success, const QString &errorMsg);

private:
    QVector<ConfigEntry> m_configs;
};

#endif // PARSEWORKER_H
