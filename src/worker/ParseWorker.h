#ifndef PARSEWORKER_H
#define PARSEWORKER_H

#include "../config/FrameFieldDef.h"
#include "../parser/FrameParser.h"
#include <QByteArray>
#include <QObject>
#include <QVector>

class ParseWorker : public QObject {
    Q_OBJECT
public:
    explicit ParseWorker(QObject *parent = nullptr) : QObject(parent) {}

    void setConfig(const FrameConfig &config) { m_config = config; }

    // Results (safe to read from main thread after finished() is emitted)
    QVector<ParsedFrame> m_frames;
    QByteArray m_rawData;

public slots:
    void process(const QString &filePath);

signals:
    void progress(int percent, const QString &status);
    void finished(bool success, const QString &errorMsg);

private:
    FrameConfig m_config;
};

#endif // PARSEWORKER_H
