#include "ConfigParser.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <xlsxdocument.h>

FrameConfig ConfigParser::loadConfig(const QString &filePath, QString *errorMsg) {
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    if (ext == "xlsx" || ext == "xls")
        return parseXlsx(filePath, errorMsg);
    else if (ext == "csv")
        return parseCsv(filePath, errorMsg);
    else {
        if (errorMsg)
            *errorMsg = QString("不支持的配置文件格式: .%1").arg(ext);
        return {};
    }
}

FrameConfig ConfigParser::parseXlsx(const QString &filePath, QString *errorMsg) {
    QXlsx::Document xlsx(filePath);
    if (!xlsx.load()) {
        if (errorMsg)
            *errorMsg = "无法打开Excel文件";
        return {};
    }

    FrameConfig config;
    // Read from first sheet, skip header row (row 1)
    int row = 2;
    while (true) {
        QVariant firstCell = xlsx.read(row, 1);
        if (!firstCell.isValid() || firstCell.toString().trimmed().isEmpty())
            break;

        QStringList columns;
        for (int col = 1; col <= 14; ++col) {
            QVariant cell = xlsx.read(row, col);
            columns.append(cell.isValid() ? cell.toString().trimmed() : "");
        }

        QString rowError;
        FrameFieldDef field = parseRow(columns, &rowError);
        if (!rowError.isEmpty()) {
            if (errorMsg)
                *errorMsg = QString("第%1行: %2").arg(row).arg(rowError);
            return {};
        }
        config.fields.append(field);
        ++row;
    }

    if (config.fields.isEmpty()) {
        if (errorMsg)
            *errorMsg = "配置文件中没有找到有效字段";
        return {};
    }

    if (!validateConfig(config, errorMsg))
        return {};

    return config;
}

FrameConfig ConfigParser::parseCsv(const QString &filePath, QString *errorMsg) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMsg)
            *errorMsg = QString("无法打开CSV文件: %1").arg(file.errorString());
        return {};
    }

    QTextStream in(&file);
    // Skip header row
    if (!in.atEnd())
        in.readLine();

    FrameConfig config;
    int lineNum = 2;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        QStringList columns = line.split(',');
        // Pad to 14 columns
        while (columns.size() < 14)
            columns.append("");
        for (auto &col : columns)
            col = col.trimmed();

        QString rowError;
        FrameFieldDef field = parseRow(columns, &rowError);
        if (!rowError.isEmpty()) {
            if (errorMsg)
                *errorMsg = QString("第%1行: %2").arg(lineNum).arg(rowError);
            return {};
        }
        config.fields.append(field);
        ++lineNum;
    }

    file.close();

    if (config.fields.isEmpty()) {
        if (errorMsg)
            *errorMsg = "CSV文件中没有找到有效字段";
        return {};
    }

    if (!validateConfig(config, errorMsg))
        return {};

    return config;
}

bool ConfigParser::validateConfig(const FrameConfig &config, QString *errorMsg) {
    bool hasHeader = false;
    for (const auto &f : config.fields) {
        if (f.fieldType == FieldType::HEADER)
            hasHeader = true;
        if (f.byteCount <= 0) {
            if (errorMsg)
                *errorMsg = QString("字段 '%1' 的字节数必须大于0").arg(f.name);
            return false;
        }
        if (f.fieldType == FieldType::CRC && f.crcAlgorithm != CrcAlgorithm::NONE) {
            if (f.crcStartField <= 0 || f.crcEndField <= 0) {
                if (errorMsg)
                    *errorMsg = QString("CRC字段 '%1' 的起止字段序号无效").arg(f.name);
                return false;
            }
        }
    }

    if (!hasHeader) {
        if (errorMsg)
            *errorMsg = "配置中必须包含至少一个HEADER字段";
        return false;
    }

    return true;
}

FrameFieldDef ConfigParser::parseRow(const QStringList &columns, QString *errorMsg) {
    FrameFieldDef field;

    // Column A: 序号
    field.index = columns[0].toInt();

    // Column B: 字段名称
    field.name = columns[1];
    if (field.name.isEmpty()) {
        if (errorMsg)
            *errorMsg = "字段名称不能为空";
        return field;
    }

    // Column C: 字段类型
    field.fieldType = parseFieldType(columns[2]);

    // Column D: 数据类型
    field.dataType = parseDataType(columns[3]);

    // Column E: 起始字节 (informational, skipped)
    // Column F: 结束字节 (informational, skipped)

    // Column G: 字节数
    field.byteCount = columns[6].toInt();
    if (field.byteCount <= 0) {
        if (errorMsg)
            *errorMsg = QString("字段 '%1' 字节数无效").arg(field.name);
        return field;
    }

    // Column H: 固定值
    field.fixedValue = parseHexValue(columns[7]);

    // Column I: 大小端
    field.endianness = parseEndianness(columns[8]);

    // Column J: CRC算法
    field.crcAlgorithm = parseCrcAlgorithm(columns[9]);

    // Column K: CRC起始字段序号
    field.crcStartField = columns[10].toInt();

    // Column L: CRC结束字段序号
    field.crcEndField = columns[11].toInt();

    // Column M: LENGTH含义
    field.lengthMeaning = parseLengthMeaning(columns[12]);

    // Column N: 备注
    if (columns.size() > 13)
        field.note = columns[13];

    return field;
}

FieldType ConfigParser::parseFieldType(const QString &str) {
    QString s = str.toUpper().trimmed();
    if (s == "HEADER") return FieldType::HEADER;
    if (s == "TAIL") return FieldType::TAIL;
    if (s == "LENGTH") return FieldType::LENGTH;
    if (s == "DATA") return FieldType::DATA;
    if (s == "CRC") return FieldType::CRC;
    if (s == "PADDING") return FieldType::PADDING;
    return FieldType::DATA; // default
}

DataType ConfigParser::parseDataType(const QString &str) {
    QString s = str.toUpper().trimmed();
    if (s == "UINT8") return DataType::UINT8;
    if (s == "INT8") return DataType::INT8;
    if (s == "UINT16") return DataType::UINT16;
    if (s == "INT16") return DataType::INT16;
    if (s == "UINT32") return DataType::UINT32;
    if (s == "INT32") return DataType::INT32;
    if (s == "FLOAT") return DataType::FLOAT;
    if (s == "DOUBLE") return DataType::DOUBLE;
    return DataType::NONE;
}

Endianness ConfigParser::parseEndianness(const QString &str) {
    QString s = str.toUpper().trimmed();
    if (s == "BIG") return Endianness::BIG;
    return Endianness::LITTLE; // default
}

CrcAlgorithm ConfigParser::parseCrcAlgorithm(const QString &str) {
    QString s = str.toUpper().trimmed();
    if (s == "CRC8") return CrcAlgorithm::CRC8;
    if (s == "CRC16_MODBUS" || s == "CRC16MODBUS") return CrcAlgorithm::CRC16_MODBUS;
    if (s == "CRC16_CCITT" || s == "CRC16CCITT") return CrcAlgorithm::CRC16_CCITT;
    if (s == "CRC32") return CrcAlgorithm::CRC32;
    return CrcAlgorithm::NONE;
}

LengthMeaning ConfigParser::parseLengthMeaning(const QString &str) {
    QString s = str.toUpper().trimmed();
    if (s == "PAYLOAD") return LengthMeaning::PAYLOAD;
    return LengthMeaning::TOTAL; // default
}

QByteArray ConfigParser::parseHexValue(const QString &str) {
    QString s = str.trimmed();
    if (s.isEmpty() || s == "-")
        return {};

    // Remove "0x" prefix
    if (s.startsWith("0x", Qt::CaseInsensitive))
        s = s.mid(2);

    // Pad odd-length
    if (s.length() % 2 != 0)
        s = "0" + s;

    QByteArray result;
    for (int i = 0; i < s.length(); i += 2) {
        bool ok;
        uint8_t byte = static_cast<uint8_t>(s.mid(i, 2).toUInt(&ok, 16));
        if (ok)
            result.append(static_cast<char>(byte));
    }
    return result;
}
