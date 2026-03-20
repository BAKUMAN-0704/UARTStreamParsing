#include "DataExporter.h"
#include <QFile>
#include <QTextStream>

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

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out.setGenerateByteOrderMark(true);

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
    out << "帧序号\t" << dataFieldNames.join('\t') << "\n";

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
