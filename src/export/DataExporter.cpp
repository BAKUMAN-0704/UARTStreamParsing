#include "DataExporter.h"
#include <QFile>
#include <QTextStream>

bool DataExporter::exportToTxt(const QString &filePath,
                               const QVector<ParsedFrame> &frames,
                               const FrameConfig &config,
                               QString *errorMsg) {
    Q_UNUSED(config);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMsg)
            *errorMsg = QString("无法创建文件: %1").arg(file.errorString());
        return false;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");

    out << QString("UART数据解析结果") << "\n";
    out << QString("总帧数: %1").arg(frames.size()) << "\n";
    out << QString("========================================") << "\n\n";

    for (const auto &frame : frames) {
        out << QString("--- 帧 #%1 (偏移: %2, CRC: %3) ---")
                   .arg(frame.frameIndex)
                   .arg(frame.offsetInStream)
                   .arg(frame.crcValid ? "OK" : "FAIL")
            << "\n";

        for (const auto &field : frame.fields) {
            QString valueStr;
            if (field.fieldType == FieldType::DATA) {
                // Format based on data type
                switch (field.dataType) {
                case DataType::FLOAT:
                case DataType::DOUBLE:
                    valueStr = QString::number(field.value.toDouble(), 'f', 6);
                    break;
                case DataType::UINT8:
                case DataType::UINT16:
                case DataType::UINT32:
                    valueStr = QString::number(field.value.toUInt());
                    break;
                case DataType::INT8:
                case DataType::INT16:
                case DataType::INT32:
                    valueStr = QString::number(field.value.toInt());
                    break;
                default:
                    valueStr = field.value.toString();
                    break;
                }
                out << QString("  %1 = %2  [原始: %3]")
                           .arg(field.name, valueStr, field.rawHex)
                    << "\n";
            } else {
                out << QString("  %1: %2").arg(field.name, field.rawHex) << "\n";
            }
        }
        out << "\n";
    }

    file.close();
    return true;
}
