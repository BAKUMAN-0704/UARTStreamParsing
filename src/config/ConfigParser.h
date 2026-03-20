#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include "FrameFieldDef.h"
#include <QString>

class ConfigParser {
public:
    // Auto-detect format by file extension and load
    static FrameConfig loadConfig(const QString &filePath, QString *errorMsg = nullptr);

    // Parse .xlsx file using QXlsx
    static FrameConfig parseXlsx(const QString &filePath, QString *errorMsg = nullptr);

    // Parse .csv file
    static FrameConfig parseCsv(const QString &filePath, QString *errorMsg = nullptr);

    // Validate a loaded config
    static bool validateConfig(const FrameConfig &config, QString *errorMsg = nullptr);

private:
    static FrameFieldDef parseRow(const QStringList &columns, QString *errorMsg);
    static FieldType parseFieldType(const QString &str);
    static DataType parseDataType(const QString &str);
    static Endianness parseEndianness(const QString &str);
    static CrcAlgorithm parseCrcAlgorithm(const QString &str);
    static LengthMeaning parseLengthMeaning(const QString &str);
    static QByteArray parseHexValue(const QString &str);
};

#endif // CONFIGPARSER_H
