#include "ParseWorker.h"

void ParseWorker::process(const QString &filePath) {
    FileParseRequest request;
    request.filePath = filePath;
    request.configs = m_configs;
    request.autoSaveDir = m_autoSaveDir;
    request.autoSaveSequenceStart = m_autoSaveSequenceStart;

    FileParseService service;
    m_result = service.parse(request, [this](int percent, const QString &status) {
        emit progress(percent, status);
    });

    emit finished(m_result.success, m_result.errorMsg);
}
