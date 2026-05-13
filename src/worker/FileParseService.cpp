#include "FileParseService.h"
#include "../datasource/FileDataSource.h"

int FileParseResult::totalFrames() const {
    int total = 0;
    for (const auto &frames : framesByConfig)
        total += frames.size();
    return total;
}

FileParseResult FileParseService::parse(const FileParseRequest &request,
                                        ProgressCallback progressCb) const {
    FileParseResult result;

    if (progressCb)
        progressCb(0, "正在读取文件...");

    QString errorMsg;
    result.rawData = FileDataSource::readHexFile(request.filePath, &errorMsg,
                                                [&](int pct) {
        if (progressCb)
            progressCb(pct * 40 / 100, "正在读取文件...");
    });

    if (!errorMsg.isEmpty()) {
        result.errorMsg = errorMsg;
        return result;
    }

    if (result.rawData.isEmpty()) {
        result.errorMsg = "文件为空或未包含有效的十六进制数据";
        return result;
    }

    if (progressCb)
        progressCb(40, QString("正在解析 %1 字节数据...").arg(result.rawData.size()));

    StreamParser parser;
    for (const auto &cfg : request.configs)
        parser.addConfig(cfg);
    if (!request.autoSaveDir.isEmpty())
        parser.setAutoSaveDir(request.autoSaveDir);
    parser.setAutoSaveSequenceStart(request.autoSaveSequenceStart);

    QObject::connect(&parser, &StreamParser::autoSaveCompleted, &parser,
                     [&result](const QStringList &files) {
                         result.autoSavedFiles.append(files);
                     },
                     Qt::DirectConnection);

    result.framesByConfig = parser.parseBatch(result.rawData, [&](int pct) {
        if (progressCb) {
            const int mappedPct = 40 + pct * 55 / 100;
            progressCb(mappedPct, QString("正在解析数据... %1%").arg(mappedPct));
        }
    });

    result.success = true;
    if (progressCb)
        progressCb(100, QString("解析完成: %1 帧").arg(result.totalFrames()));

    return result;
}
