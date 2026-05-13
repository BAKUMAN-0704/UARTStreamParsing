#include "ParseExportControlWorkflow.h"

ParseExportControlView ParseExportControlWorkflow::resolve(
    const ParseExportControlInput &input) const {
    ParseExportControlView view;
    view.sourcePageIndex = input.sourceMode == ParseExportSourceMode::Serial ? 1 : 0;
    view.progressVisible = input.parsing;
    view.progressValue = 0;
    view.browseConfigEnabled = !input.parsing;
    view.browseDataFileEnabled = !input.parsing;
    view.exportEnabled = !input.parsing && input.hasExportableFrames;

    const bool hasFileData =
        input.sourceMode == ParseExportSourceMode::File && input.hasFilePath;
    const bool hasData = input.hasRawData || input.serialOpen || hasFileData;
    const bool streamingSerial =
        input.sourceMode == ParseExportSourceMode::Serial && input.streamingActive;
    view.parseEnabled = !input.parsing && input.hasConfigs && hasData && !streamingSerial &&
                        !input.workerActive;

    return view;
}

bool ParseExportControlWorkflow::hasExportableFrames(
    const QMap<QString, QVector<ParsedFrame>> &framesByConfig) const {
    for (const auto &frames : framesByConfig) {
        if (!frames.isEmpty())
            return true;
    }
    return false;
}
