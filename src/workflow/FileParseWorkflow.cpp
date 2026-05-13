#include "FileParseWorkflow.h"

FileParseCompletionView FileParseWorkflow::completeFileParse(const FileParseResult &result) const {
    FileParseCompletionView view;
    const int totalFrames = result.totalFrames();

    view.rawData = result.rawData;
    view.framesByConfig = result.framesByConfig;
    view.statusMessage = QString("解析完成: %1 帧, 共 %2 字节")
                             .arg(totalFrames)
                             .arg(result.rawData.size());
    if (!result.autoSavedFiles.isEmpty())
        view.statusMessage += QString(", 自动保存 %1 个文件").arg(result.autoSavedFiles.size());
    view.exportEnabled = totalFrames > 0;
    view.showEmptyResultMessage = totalFrames == 0;
    view.showResultDialog = totalFrames > 0;

    return view;
}
