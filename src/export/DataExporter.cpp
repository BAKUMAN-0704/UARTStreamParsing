#include "DataExporter.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <algorithm>

static QString formatValue(const ParsedField &field) {
    switch (field.dataType) {
    case DataType::FLOAT:
    case DataType::DOUBLE:
        return QString::number(field.value.toDouble(), 'f', 6);
    case DataType::UINT8:
    case DataType::UINT16:
    case DataType::UINT32:
        return QString::number(field.value.toUInt());
    case DataType::INT8:
    case DataType::INT16:
    case DataType::INT32:
        return QString::number(field.value.toInt());
    default:
        return field.value.toString();
    }
}

bool DataExporter::exportToTxt(const QString &filePath,
                               const QVector<ParsedFrame> &frames,
                               const FrameConfig &config,
                               QString *errorMsg) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMsg)
            *errorMsg = QString("无法创建文件: %1").arg(file.errorString());
        return false;
    }

    // Write UTF-8 BOM explicitly for Windows compatibility
    file.write("\xEF\xBB\xBF");
    QTextStream out(&file);
    out.setCodec("UTF-8");

    // Collect DATA field definitions in config order
    QVector<int> dataFieldIndices;
    QStringList dataFieldNames;
    for (int i = 0; i < config.fields.size(); ++i) {
        if (config.fields[i].fieldType == FieldType::DATA) {
            dataFieldIndices.append(i);
            dataFieldNames.append(config.fields[i].name);
        }
    }

    // Write tab-separated header line
    out << "FrameIndex\t" << dataFieldNames.join('\t') << "\n";

    // Write each frame as a row, DATA fields only, in config order
    for (const auto &frame : frames) {
        out << frame.frameIndex;
        for (int fi : dataFieldIndices) {
            out << '\t';
            const QString &name = config.fields[fi].name;
            for (const auto &pf : frame.fields) {
                if (pf.name == name && pf.fieldType == FieldType::DATA) {
                    out << formatValue(pf);
                    break;
                }
            }
        }
        out << "\n";
    }

    file.close();
    return true;
}

QStringList DataExporter::autoSaveMultiConfig(
    const QString &dirPath,
    const QMap<QString, QVector<ParsedFrame>> &framesByConfig,
    const QMap<QString, FrameConfig> &configMap,
    const QString &endFrameConfigName, QString *errorMsg) {
    QDir dir(dirPath);
    if (!dir.exists())
        dir.mkpath(".");

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QStringList savedFiles;

    auto saveGroup = [&](const QString &name, const QVector<ParsedFrame> &frames,
                         const QString &suffix) -> bool {
        if (name == endFrameConfigName || frames.isEmpty() || !configMap.contains(name))
            return true;

        QString fileName = name + "_" + timestamp + suffix + ".txt";
        QString fullPath = dir.absoluteFilePath(fileName);

        QString err;
        if (exportToTxt(fullPath, frames, configMap[name], &err)) {
            savedFiles << fullPath;
            return true;
        }

        if (errorMsg)
            *errorMsg = err;
        return false;
    };

    QVector<int> endOffsets;
    if (!endFrameConfigName.isEmpty() && framesByConfig.contains(endFrameConfigName)) {
        for (const auto &frame : framesByConfig[endFrameConfigName])
            endOffsets.append(frame.offsetInStream);
        std::sort(endOffsets.begin(), endOffsets.end());
    }

    if (!endOffsets.isEmpty()) {
        int sessionStart = -1;
        int sessionIndex = 1;
        for (int endOffset : endOffsets) {
            for (auto it = framesByConfig.constBegin(); it != framesByConfig.constEnd(); ++it) {
                QVector<ParsedFrame> sessionFrames;
                for (const auto &frame : it.value()) {
                    if (frame.offsetInStream > sessionStart && frame.offsetInStream < endOffset)
                        sessionFrames.append(frame);
                }
                if (!saveGroup(it.key(), sessionFrames,
                               QString("_session%1").arg(sessionIndex)))
                    return savedFiles;
            }
            sessionStart = endOffset;
            ++sessionIndex;
        }

        bool hasRemaining = false;
        for (auto it = framesByConfig.constBegin(); it != framesByConfig.constEnd(); ++it) {
            if (it.key() == endFrameConfigName)
                continue;
            for (const auto &frame : it.value()) {
                if (frame.offsetInStream > sessionStart) {
                    hasRemaining = true;
                    break;
                }
            }
            if (hasRemaining)
                break;
        }

        if (hasRemaining) {
            for (auto it = framesByConfig.constBegin(); it != framesByConfig.constEnd(); ++it) {
                QVector<ParsedFrame> sessionFrames;
                for (const auto &frame : it.value()) {
                    if (frame.offsetInStream > sessionStart)
                        sessionFrames.append(frame);
                }
                if (!saveGroup(it.key(), sessionFrames,
                               QString("_session%1").arg(sessionIndex)))
                    return savedFiles;
            }
        }

        return savedFiles;
    }

    for (auto it = framesByConfig.constBegin(); it != framesByConfig.constEnd(); ++it) {
        if (!saveGroup(it.key(), it.value(), QString()))
            return savedFiles;
    }

    return savedFiles;
}
