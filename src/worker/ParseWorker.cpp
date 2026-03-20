#include "ParseWorker.h"
#include "../datasource/FileDataSource.h"

void ParseWorker::process(const QString &filePath) {
    // Phase 1: Read hex file (0% - 40%)
    emit progress(0, "正在读取文件...");

    QString errorMsg;
    m_rawData = FileDataSource::readHexFile(filePath, &errorMsg, [this](int pct) {
        emit progress(pct * 40 / 100, "正在读取文件...");
    });

    if (!errorMsg.isEmpty()) {
        emit finished(false, errorMsg);
        return;
    }

    if (m_rawData.isEmpty()) {
        emit finished(false, "文件为空或未包含有效的十六进制数据");
        return;
    }

    // Phase 2: Parse frames (40% - 95%)
    emit progress(40, QString("正在解析 %1 字节数据...").arg(m_rawData.size()));

    FrameParser parser;
    parser.setConfig(m_config);
    m_frames = parser.parse(m_rawData, [this](int pct) {
        emit progress(40 + pct * 55 / 100,
                      QString("正在解析数据... %1%").arg(40 + pct * 55 / 100));
    });

    // Done
    emit progress(100, QString("解析完成: %1 帧").arg(m_frames.size()));
    emit finished(true, QString());
}
