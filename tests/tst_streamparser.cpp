#include "src/parser/StreamParser.h"
#include "src/parser/FrameLayout.h"
#include "src/datasource/FileDataSource.h"
#include "src/export/DataExporter.h"
#include "src/codec/HexTextDecoder.h"
#include "src/worker/FileParseService.h"
#include "src/workflow/FileParseWorkflow.h"
#include "src/workflow/ParseExportControlWorkflow.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <algorithm>

class StreamParserTest : public QObject {
    Q_OBJECT

private slots:
    void decodesHexTextAsWholeString();
    void decodesHexTextAcrossChunks();
    void fileDataSourceUsesSharedHexRules();
    void fileParseServiceParsesHexFileWithProgress();
    void fileParseServiceCapturesAutoSaveCompleted();
    void fileParseServiceRejectsEmptyDecodedData();
    void fileParseWorkflowBuildsSuccessfulCompletionWithFrames();
    void fileParseWorkflowBuildsEmptyCompletion();
    void fileParseWorkflowIncludesAutoSaveCountInStatus();
    void parseExportControlSelectsSourcePage();
    void parseExportControlEnablesFileParseWhenReady();
    void parseExportControlDisablesSerialParseWhileStreaming();
    void parseExportControlDisablesSerialParseAfterCloseWithoutBufferedData();
    void parseExportControlDisablesControlsWhileParsing();
    void parseExportControlEnablesExportOnlyWithFrames();
    void frameLayoutComputesFixedSize();
    void frameLayoutComputesTotalLengthSize();
    void frameLayoutComputesPayloadLengthSize();
    void frameLayoutPreservesIncompleteLengthPolicies();
    void parsesSharedHeaderFormatsRegardlessOfOrder();
    void specificConfigBeatsGenericFirstConfig();
    void manualMultiConfigExportSplitsByEndFrameOffsets();
    void manualMultiConfigExportUsesConfiguredLeadingSequence();
    void streamingAutoSaveSplitsAcrossChunkBoundaries();
    void streamingAutoSaveUsesConfiguredLeadingSequence();
    void streamingAutoSaveSequenceIsPerParserInstance();
    void generatedFileStreamingMatchesBatchAutoSave();

private:
    static QByteArray bytes(std::initializer_list<unsigned char> values);
    static FrameConfig makeConfig(unsigned char typeByte, bool fixedType, FieldType typeFieldType);
    static QByteArray makeFrame(unsigned char typeByte, unsigned char payload);
    static QMap<QString, int> parseCounts(const QVector<ConfigEntry> &configs,
                                          const QByteArray &stream);
    static int exportedDataRowCount(const QString &filePath);
    static QString findGeneratedStreamPath();
    static QVector<ConfigEntry> generatedConfigs();
    static QMap<QString, QVector<int>> exportedRowsByConfig(const QStringList &files);
};

QByteArray StreamParserTest::bytes(std::initializer_list<unsigned char> values) {
    QByteArray data;
    data.reserve(static_cast<int>(values.size()));
    for (unsigned char value : values)
        data.append(static_cast<char>(value));
    return data;
}

FrameConfig StreamParserTest::makeConfig(unsigned char typeByte, bool fixedType,
                                         FieldType typeFieldType) {
    FrameConfig config;

    FrameFieldDef header;
    header.index = 1;
    header.name = "header";
    header.fieldType = FieldType::HEADER;
    header.byteCount = 2;
    header.fixedValue = bytes({0x51, 0xFA});
    config.fields.append(header);

    FrameFieldDef type;
    type.index = 2;
    type.name = "type";
    type.fieldType = typeFieldType;
    type.dataType = typeFieldType == FieldType::DATA ? DataType::UINT8 : DataType::NONE;
    type.byteCount = 1;
    if (fixedType)
        type.fixedValue = bytes({typeByte});
    config.fields.append(type);

    FrameFieldDef payload;
    payload.index = 3;
    payload.name = "payload";
    payload.fieldType = FieldType::DATA;
    payload.dataType = DataType::UINT8;
    payload.byteCount = 1;
    config.fields.append(payload);

    FrameFieldDef tail;
    tail.index = 4;
    tail.name = "tail";
    tail.fieldType = FieldType::TAIL;
    tail.byteCount = 2;
    tail.fixedValue = bytes({0x5A, 0x78});
    config.fields.append(tail);

    return config;
}

QByteArray StreamParserTest::makeFrame(unsigned char typeByte, unsigned char payload) {
    return bytes({0x51, 0xFA, typeByte, payload, 0x5A, 0x78});
}

QMap<QString, int> StreamParserTest::parseCounts(const QVector<ConfigEntry> &configs,
                                                 const QByteArray &stream) {
    StreamParser parser;
    for (const auto &config : configs)
        parser.addConfig(config);

    QMap<QString, int> counts;
    auto results = parser.parseBatch(stream);
    for (auto it = results.constBegin(); it != results.constEnd(); ++it)
        counts[it.key()] = it.value().size();
    return counts;
}

int StreamParserTest::exportedDataRowCount(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;

    int rows = 0;
    const auto lines = file.readAll().split('\n');
    for (int i = 1; i < lines.size(); ++i) {
        if (!lines[i].trimmed().isEmpty())
            ++rows;
    }
    return rows;
}

QString StreamParserTest::findGeneratedStreamPath() {
    const QStringList candidates = {
        "test/test_stream_mixed.txt",
        "../test/test_stream_mixed.txt",
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return QString();
}

QVector<ConfigEntry> StreamParserTest::generatedConfigs() {
    auto fixedField = [](int index, const QString &name, FieldType type, int byteCount,
                         QByteArray fixedValue = {}) {
        FrameFieldDef field;
        field.index = index;
        field.name = name;
        field.fieldType = type;
        field.byteCount = byteCount;
        field.fixedValue = fixedValue;
        return field;
    };
    auto dataField = [](int index, const QString &name, DataType type, int byteCount,
                        Endianness endianness = Endianness::LITTLE) {
        FrameFieldDef field;
        field.index = index;
        field.name = name;
        field.fieldType = FieldType::DATA;
        field.dataType = type;
        field.byteCount = byteCount;
        field.endianness = endianness;
        return field;
    };
    auto lengthField = [](int index, const QString &name, int byteCount, Endianness endianness,
                          LengthMeaning meaning) {
        FrameFieldDef field;
        field.index = index;
        field.name = name;
        field.fieldType = FieldType::LENGTH;
        field.dataType = byteCount == 2 ? DataType::UINT16 : DataType::UINT32;
        field.byteCount = byteCount;
        field.endianness = endianness;
        field.lengthMeaning = meaning;
        return field;
    };
    auto crcField = [](int index, int byteCount, Endianness endianness,
                       CrcAlgorithm algorithm, int startField, int endField) {
        FrameFieldDef field;
        field.index = index;
        field.name = "CRC";
        field.fieldType = FieldType::CRC;
        field.byteCount = byteCount;
        field.endianness = endianness;
        field.crcAlgorithm = algorithm;
        field.crcStartField = startField;
        field.crcEndField = endField;
        return field;
    };

    FrameConfig frame1;
    frame1.fields = {
        fixedField(1, "帧头", FieldType::HEADER, 2, bytes({0x51, 0xFA})),
        fixedField(2, "帧类型", FieldType::PADDING, 1, bytes({0x01})),
        dataField(3, "序号", DataType::UINT32, 4, Endianness::BIG),
        dataField(4, "温度", DataType::INT16, 2, Endianness::BIG),
        dataField(5, "电压", DataType::UINT16, 2, Endianness::LITTLE),
        crcField(6, 2, Endianness::LITTLE, CrcAlgorithm::CRC16_MODBUS, 2, 5),
        fixedField(7, "帧尾", FieldType::TAIL, 2, bytes({0x5A, 0x78})),
    };

    FrameConfig frame2;
    frame2.fields = {
        fixedField(1, "帧头", FieldType::HEADER, 2, bytes({0x51, 0xFA})),
        fixedField(2, "帧类型", FieldType::PADDING, 1, bytes({0x02})),
        lengthField(3, "帧长度", 2, Endianness::BIG, LengthMeaning::TOTAL),
        dataField(4, "通道", DataType::UINT8, 1),
        dataField(5, "采样序号", DataType::UINT32, 4, Endianness::BIG),
        dataField(6, "加速度X", DataType::FLOAT, 4, Endianness::LITTLE),
        dataField(7, "状态", DataType::UINT8, 1),
        crcField(8, 1, Endianness::LITTLE, CrcAlgorithm::CRC8, 2, 7),
        fixedField(9, "帧尾", FieldType::TAIL, 2, bytes({0x5A, 0x78})),
    };

    FrameConfig frame3;
    frame3.fields = {
        fixedField(1, "帧头", FieldType::HEADER, 2, bytes({0x51, 0xFA})),
        fixedField(2, "帧类型", FieldType::PADDING, 1, bytes({0x03})),
        lengthField(3, "数据长度", 2, Endianness::LITTLE, LengthMeaning::PAYLOAD),
        dataField(4, "压力", DataType::UINT16, 2, Endianness::LITTLE),
        dataField(5, "电流", DataType::UINT16, 2, Endianness::LITTLE),
        dataField(6, "标志", DataType::UINT8, 1),
        fixedField(7, "填充", FieldType::PADDING, 1, bytes({0x00})),
        crcField(8, 2, Endianness::BIG, CrcAlgorithm::CRC16_CCITT, 2, 7),
        fixedField(9, "帧尾", FieldType::TAIL, 2, bytes({0x5A, 0x78})),
    };

    FrameConfig endFrame;
    endFrame.fields = {
        fixedField(1, "帧头", FieldType::HEADER, 2, bytes({0x51, 0xFA})),
        fixedField(2, "帧类型", FieldType::PADDING, 1, bytes({0x04})),
        dataField(3, "会话序号", DataType::UINT16, 2, Endianness::BIG),
        dataField(4, "结束原因", DataType::UINT8, 1),
        dataField(5, "frame2计数", DataType::UINT32, 4, Endianness::BIG),
        crcField(6, 2, Endianness::LITTLE, CrcAlgorithm::CRC16_MODBUS, 2, 5),
        fixedField(7, "帧尾", FieldType::TAIL, 2, bytes({0x5A, 0x78})),
    };

    return {
        {"test_config", {}, frame1, false},
        {"test_config_with_length", {}, frame2, false},
        {"test_config_multi_padding", {}, frame3, false},
        {"test_config_endframe", {}, endFrame, true},
    };
}

void StreamParserTest::decodesHexTextAsWholeString() {
    QCOMPARE(HexTextDecoder::decodeAll(QByteArray("55 AA 1a")), bytes({0x55, 0xAA, 0x1A}));
    QCOMPARE(HexTextDecoder::decodeAll(QByteArray("0x55,0XAA;1a")), bytes({0x55, 0xAA, 0x1A}));
    QCOMPARE(HexTextDecoder::decodeAll(QByteArray("A")), bytes({0x0A}));
    QCOMPARE(HexTextDecoder::decodeAll(QByteArray("0")), bytes({0x00}));
}

void StreamParserTest::decodesHexTextAcrossChunks() {
    HexTextDecoder decoder;
    QByteArray decoded;
    decoded.append(decoder.append("5"));
    decoded.append(decoder.append("5"));
    decoded.append(decoder.finish());
    QCOMPARE(decoded, bytes({0x55}));

    decoder.reset();
    decoded.clear();
    decoded.append(decoder.append("0"));
    decoded.append(decoder.append("x55"));
    decoded.append(decoder.finish());
    QCOMPARE(decoded, bytes({0x55}));

    decoder.reset();
    decoded.clear();
    decoded.append(decoder.append("A"));
    decoded.append(decoder.finish());
    QCOMPARE(decoded, bytes({0x0A}));

    decoder.reset();
    decoded.clear();
    decoded.append(decoder.append("5Z"));
    decoded.append(decoder.append("A"));
    decoded.append(decoder.finish());
    QCOMPARE(decoded, bytes({0x05, 0x0A}));
}

void StreamParserTest::fileDataSourceUsesSharedHexRules() {
    QString errorMsg;
    QCOMPARE(FileDataSource::parseHexString("0x55,0XAA;1a", &errorMsg),
             bytes({0x55, 0xAA, 0x1A}));
    QVERIFY2(errorMsg.isEmpty(), qPrintable(errorMsg));
    QCOMPARE(FileDataSource::parseHexString("0", &errorMsg), bytes({0x00}));
    QVERIFY2(errorMsg.isEmpty(), qPrintable(errorMsg));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("hex.txt");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("0x55,0XAA;1a 0");
    file.close();

    QCOMPARE(FileDataSource::readHexFile(path, &errorMsg), bytes({0x55, 0xAA, 0x1A, 0x00}));
    QVERIFY2(errorMsg.isEmpty(), qPrintable(errorMsg));
}

void StreamParserTest::fileParseServiceParsesHexFileWithProgress() {
    QVector<ConfigEntry> configs = {
        {"data", {}, makeConfig(0x01, true, FieldType::DATA), false},
    };
    QByteArray stream;
    stream.append(makeFrame(0x01, 0x10));
    stream.append(makeFrame(0x01, 0x11));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("stream.txt");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(stream.toHex(' '));
    file.close();

    FileParseRequest request;
    request.filePath = path;
    request.configs = configs;

    QVector<QPair<int, QString>> progress;
    FileParseService service;
    const FileParseResult result = service.parse(request, [&progress](int percent, const QString &status) {
        progress.append({percent, status});
    });

    QVERIFY2(result.success, qPrintable(result.errorMsg));
    QVERIFY(result.errorMsg.isEmpty());
    QCOMPARE(result.rawData, stream);
    QCOMPARE(result.framesByConfig.value("data").size(), 2);
    QCOMPARE(result.totalFrames(), 2);
    QVERIFY(!progress.isEmpty());
    QCOMPARE(progress.first().first, 0);
    QCOMPARE(progress.first().second, QString("正在读取文件..."));
    QVERIFY(std::any_of(progress.cbegin(), progress.cend(), [](const auto &item) {
        return item.first == 40 && item.second.startsWith("正在解析 ");
    }));
    QCOMPARE(progress.last().first, 100);
    QCOMPARE(progress.last().second, QString("解析完成: 2 帧"));
}

void StreamParserTest::fileParseServiceCapturesAutoSaveCompleted() {
    QVector<ConfigEntry> configs = {
        {"data", {}, makeConfig(0x01, true, FieldType::DATA), false},
        {"end", {}, makeConfig(0x04, true, FieldType::DATA), true},
    };

    QByteArray stream;
    stream.append(makeFrame(0x01, 0x10));
    stream.append(makeFrame(0x01, 0x11));
    stream.append(makeFrame(0x04, 0x40));
    stream.append(makeFrame(0x01, 0x12));
    stream.append(makeFrame(0x04, 0x41));

    QTemporaryDir inputDir;
    QTemporaryDir saveDir;
    QVERIFY(inputDir.isValid());
    QVERIFY(saveDir.isValid());
    const QString path = inputDir.filePath("stream.txt");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(stream.toHex(' '));
    file.close();

    FileParseRequest request;
    request.filePath = path;
    request.configs = configs;
    request.autoSaveDir = saveDir.path();
    request.autoSaveSequenceStart = -5;

    FileParseService service;
    const FileParseResult result = service.parse(request);

    QVERIFY2(result.success, qPrintable(result.errorMsg));
    QCOMPARE(result.framesByConfig.value("data").size(), 3);
    QCOMPARE(result.framesByConfig.value("end").size(), 2);
    QCOMPARE(result.autoSavedFiles.size(), 2);
    const QString firstName = QFileInfo(result.autoSavedFiles[0]).fileName();
    const QString secondName = QFileInfo(result.autoSavedFiles[1]).fileName();
    QVERIFY2(firstName.startsWith("-5-data_"), qPrintable(firstName));
    QVERIFY2(secondName.startsWith("-4-data_"), qPrintable(secondName));
    QCOMPARE(exportedDataRowCount(result.autoSavedFiles[0]), 2);
    QCOMPARE(exportedDataRowCount(result.autoSavedFiles[1]), 1);
}

void StreamParserTest::fileParseServiceRejectsEmptyDecodedData() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("empty.txt");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("   , ; \n");
    file.close();

    FileParseRequest request;
    request.filePath = path;
    request.configs = {{"data", {}, makeConfig(0x01, true, FieldType::DATA), false}};

    FileParseService service;
    const FileParseResult result = service.parse(request);

    QVERIFY(!result.success);
    QCOMPARE(result.errorMsg, QString("文件为空或未包含有效的十六进制数据"));
    QVERIFY(result.rawData.isEmpty());
    QVERIFY(result.autoSavedFiles.isEmpty());
    QCOMPARE(result.totalFrames(), 0);
}

void StreamParserTest::fileParseWorkflowBuildsSuccessfulCompletionWithFrames() {
    FileParseResult result;
    result.success = true;
    result.rawData = bytes({0x51, 0xFA, 0x01, 0x10, 0x5A, 0x78});
    ParsedFrame frame;
    frame.configName = "data";
    result.framesByConfig["data"].append(frame);

    FileParseWorkflow workflow;
    const FileParseCompletionView view = workflow.completeFileParse(result);

    QCOMPARE(view.rawData, result.rawData);
    QCOMPARE(view.framesByConfig.value("data").size(), 1);
    QCOMPARE(view.statusMessage, QString("解析完成: 1 帧, 共 6 字节"));
    QVERIFY(view.exportEnabled);
    QVERIFY(view.showResultDialog);
    QVERIFY(!view.showEmptyResultMessage);
}

void StreamParserTest::fileParseWorkflowBuildsEmptyCompletion() {
    FileParseResult result;
    result.success = true;
    result.rawData = bytes({0x51, 0xFA, 0x01, 0x10, 0x5A, 0x78});

    FileParseWorkflow workflow;
    const FileParseCompletionView view = workflow.completeFileParse(result);

    QCOMPARE(view.rawData, result.rawData);
    QVERIFY(view.framesByConfig.isEmpty());
    QCOMPARE(view.statusMessage, QString("解析完成: 0 帧, 共 6 字节"));
    QVERIFY(!view.exportEnabled);
    QVERIFY(!view.showResultDialog);
    QVERIFY(view.showEmptyResultMessage);
}

void StreamParserTest::fileParseWorkflowIncludesAutoSaveCountInStatus() {
    FileParseResult result;
    result.success = true;
    result.rawData = bytes({0x51, 0xFA, 0x01, 0x10, 0x5A, 0x78});
    ParsedFrame frame;
    frame.configName = "data";
    result.framesByConfig["data"].append(frame);
    result.autoSavedFiles << "first.txt" << "second.txt";

    FileParseWorkflow workflow;
    const FileParseCompletionView view = workflow.completeFileParse(result);

    QCOMPARE(view.statusMessage, QString("解析完成: 1 帧, 共 6 字节, 自动保存 2 个文件"));
    QVERIFY(view.exportEnabled);
    QVERIFY(view.showResultDialog);
    QVERIFY(!view.showEmptyResultMessage);
}

void StreamParserTest::parseExportControlSelectsSourcePage() {
    ParseExportControlWorkflow workflow;
    ParseExportControlInput input;

    input.sourceMode = ParseExportSourceMode::File;
    QCOMPARE(workflow.resolve(input).sourcePageIndex, 0);

    input.sourceMode = ParseExportSourceMode::Serial;
    QCOMPARE(workflow.resolve(input).sourcePageIndex, 1);
}

void StreamParserTest::parseExportControlEnablesFileParseWhenReady() {
    ParseExportControlWorkflow workflow;
    ParseExportControlInput input;
    input.sourceMode = ParseExportSourceMode::File;
    input.hasConfigs = true;
    input.hasFilePath = true;

    QVERIFY(workflow.resolve(input).parseEnabled);

    input.workerActive = true;
    QVERIFY(!workflow.resolve(input).parseEnabled);
}

void StreamParserTest::parseExportControlDisablesSerialParseWhileStreaming() {
    ParseExportControlWorkflow workflow;
    ParseExportControlInput input;
    input.sourceMode = ParseExportSourceMode::Serial;
    input.hasConfigs = true;
    input.serialOpen = true;
    input.streamingActive = true;

    QVERIFY(!workflow.resolve(input).parseEnabled);
}

void StreamParserTest::parseExportControlDisablesSerialParseAfterCloseWithoutBufferedData() {
    ParseExportControlWorkflow workflow;
    ParseExportControlInput input;
    input.sourceMode = ParseExportSourceMode::Serial;
    input.hasConfigs = true;

    QVERIFY(!workflow.resolve(input).parseEnabled);
}

void StreamParserTest::parseExportControlDisablesControlsWhileParsing() {
    ParseExportControlWorkflow workflow;
    ParseExportControlInput input;
    input.sourceMode = ParseExportSourceMode::File;
    input.hasConfigs = true;
    input.hasFilePath = true;
    input.parsing = true;
    input.hasExportableFrames = true;

    const ParseExportControlView view = workflow.resolve(input);

    QVERIFY(view.progressVisible);
    QCOMPARE(view.progressValue, 0);
    QVERIFY(!view.parseEnabled);
    QVERIFY(!view.exportEnabled);
    QVERIFY(!view.browseConfigEnabled);
    QVERIFY(!view.browseDataFileEnabled);
}

void StreamParserTest::parseExportControlEnablesExportOnlyWithFrames() {
    ParseExportControlWorkflow workflow;

    QMap<QString, QVector<ParsedFrame>> framesByConfig;
    QVERIFY(!workflow.hasExportableFrames(framesByConfig));

    framesByConfig["data"] = {};
    QVERIFY(!workflow.hasExportableFrames(framesByConfig));

    ParsedFrame frame;
    frame.configName = "data";
    framesByConfig["data"].append(frame);
    QVERIFY(workflow.hasExportableFrames(framesByConfig));

    ParseExportControlInput input;
    QVERIFY(!workflow.resolve(input).exportEnabled);

    input.hasExportableFrames = true;
    QVERIFY(workflow.resolve(input).exportEnabled);

    input.parsing = true;
    QVERIFY(!workflow.resolve(input).exportEnabled);
}

void StreamParserTest::frameLayoutComputesFixedSize() {
    const FrameConfig config = makeConfig(0x01, true, FieldType::DATA);
    const QByteArray frame = makeFrame(0x01, 0x10);

    const auto result = FrameLayout::resolveFrameSize(
        config, frame, 0, FrameLayout::IncompleteLengthPolicy::ReturnInvalid);

    QVERIFY(result.valid);
    QCOMPARE(result.size, config.totalFrameSize());
    QVERIFY(!result.usedLengthField);
    QVERIFY(!result.lengthFieldIncomplete);
}

void StreamParserTest::frameLayoutComputesTotalLengthSize() {
    const FrameConfig config = generatedConfigs()[1].config;
    QByteArray frame(config.totalFrameSize(), '\0');
    const int lengthOffset = config.fieldOffset(3);
    QVERIFY(lengthOffset >= 0);
    frame[lengthOffset] = static_cast<char>(0x00);
    frame[lengthOffset + 1] = static_cast<char>(config.totalFrameSize());

    const auto result = FrameLayout::resolveFrameSize(
        config, frame, 0, FrameLayout::IncompleteLengthPolicy::ReturnInvalid);

    QVERIFY(result.valid);
    QCOMPARE(result.size, config.totalFrameSize());
    QVERIFY(result.usedLengthField);
    QVERIFY(!result.lengthFieldIncomplete);
}

void StreamParserTest::frameLayoutComputesPayloadLengthSize() {
    const FrameConfig config = generatedConfigs()[2].config;
    QByteArray frame(config.totalFrameSize(), '\0');
    const int lengthOffset = config.fieldOffset(3);
    QVERIFY(lengthOffset >= 0);
    frame[lengthOffset] = static_cast<char>(0x05);
    frame[lengthOffset + 1] = static_cast<char>(0x00);

    const auto result = FrameLayout::resolveFrameSize(
        config, frame, 0, FrameLayout::IncompleteLengthPolicy::ReturnInvalid);

    QVERIFY(result.valid);
    QCOMPARE(result.size, config.totalFrameSize());
    QVERIFY(result.usedLengthField);
    QVERIFY(!result.lengthFieldIncomplete);
}

void StreamParserTest::frameLayoutPreservesIncompleteLengthPolicies() {
    const FrameConfig config = generatedConfigs()[1].config;
    QByteArray incomplete(config.fieldOffset(3) + 1, '\0');

    const auto invalidResult = FrameLayout::resolveFrameSize(
        config, incomplete, 0, FrameLayout::IncompleteLengthPolicy::ReturnInvalid);
    QVERIFY(!invalidResult.valid);
    QVERIFY(invalidResult.usedLengthField);
    QVERIFY(invalidResult.lengthFieldIncomplete);

    const auto fallbackResult = FrameLayout::resolveFrameSize(
        config, incomplete, 0, FrameLayout::IncompleteLengthPolicy::UseStaticLayoutSize);
    QVERIFY(fallbackResult.valid);
    QCOMPARE(fallbackResult.size, config.totalFrameSize());
    QVERIFY(fallbackResult.usedLengthField);
    QVERIFY(fallbackResult.lengthFieldIncomplete);
}

QMap<QString, QVector<int>> StreamParserTest::exportedRowsByConfig(const QStringList &files) {
    QMap<QString, QVector<int>> result;
    const QStringList names = {
        "test_config_with_length",
        "test_config_multi_padding",
        "test_config_endframe",
        "test_config",
    };

    for (const QString &filePath : files) {
        QString fileName = QFileInfo(filePath).fileName();
        const int dashIndex = fileName.indexOf('-');
        if (dashIndex >= 0)
            fileName = fileName.mid(dashIndex + 1);
        for (const QString &name : names) {
            if (fileName.startsWith(name + "_")) {
                result[name].append(exportedDataRowCount(filePath));
                break;
            }
        }
    }

    for (auto it = result.begin(); it != result.end(); ++it)
        std::sort(it.value().begin(), it.value().end());
    return result;
}

void StreamParserTest::parsesSharedHeaderFormatsRegardlessOfOrder() {
    QVector<ConfigEntry> forward = {
        {"type1", {}, makeConfig(0x01, true, FieldType::DATA), false},
        {"type2", {}, makeConfig(0x02, true, FieldType::DATA), false},
        {"type3", {}, makeConfig(0x03, true, FieldType::DATA), false},
        {"type4", {}, makeConfig(0x04, true, FieldType::DATA), true},
    };
    QVector<ConfigEntry> reverse = {
        forward[3],
        forward[2],
        forward[1],
        forward[0],
    };

    QByteArray stream = bytes({0x00, 0xFF});
    stream.append(makeFrame(0x02, 0x20));
    stream.append(bytes({0x13}));
    stream.append(makeFrame(0x01, 0x10));
    stream.append(makeFrame(0x03, 0x30));
    stream.append(makeFrame(0x04, 0x40));
    stream.append(makeFrame(0x02, 0x21));
    stream.append(bytes({0xEE}));

    auto forwardCounts = parseCounts(forward, stream);
    auto reverseCounts = parseCounts(reverse, stream);

    QCOMPARE(forwardCounts.value("type1"), 1);
    QCOMPARE(forwardCounts.value("type2"), 2);
    QCOMPARE(forwardCounts.value("type3"), 1);
    QCOMPARE(forwardCounts.value("type4"), 1);
    QCOMPARE(reverseCounts, forwardCounts);
}

void StreamParserTest::specificConfigBeatsGenericFirstConfig() {
    QVector<ConfigEntry> configs = {
        {"generic", {}, makeConfig(0x00, false, FieldType::DATA), false},
        {"type2", {}, makeConfig(0x02, true, FieldType::DATA), false},
    };

    QByteArray stream;
    stream.append(makeFrame(0x02, 0x42));

    auto counts = parseCounts(configs, stream);
    QCOMPARE(counts.value("generic"), 0);
    QCOMPARE(counts.value("type2"), 1);
}

void StreamParserTest::manualMultiConfigExportSplitsByEndFrameOffsets() {
    QVector<ConfigEntry> configs = {
        {"data", {}, makeConfig(0x01, true, FieldType::DATA), false},
        {"end", {}, makeConfig(0x04, true, FieldType::DATA), true},
    };

    QByteArray stream;
    stream.append(makeFrame(0x01, 0x10));
    stream.append(makeFrame(0x01, 0x11));
    stream.append(makeFrame(0x04, 0x40));
    stream.append(makeFrame(0x01, 0x12));
    stream.append(makeFrame(0x04, 0x41));

    StreamParser parser;
    for (const auto &config : configs)
        parser.addConfig(config);
    const auto results = parser.parseBatch(stream);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QMap<QString, FrameConfig> configMap;
    for (const auto &config : configs)
        configMap[config.name] = config.config;

    QString errorMsg;
    const QStringList saved = DataExporter::autoSaveMultiConfig(
        dir.path(), results, configMap, "end", &errorMsg);

    QVERIFY2(errorMsg.isEmpty(), qPrintable(errorMsg));
    QCOMPARE(saved.size(), 2);
    QCOMPARE(exportedDataRowCount(saved[0]), 2);
    QCOMPARE(exportedDataRowCount(saved[1]), 1);
}

void StreamParserTest::manualMultiConfigExportUsesConfiguredLeadingSequence() {
    QVector<ConfigEntry> configs = {
        {"data", {}, makeConfig(0x01, true, FieldType::DATA), false},
        {"end", {}, makeConfig(0x04, true, FieldType::DATA), true},
    };

    QByteArray stream;
    stream.append(makeFrame(0x01, 0x10));
    stream.append(makeFrame(0x01, 0x11));
    stream.append(makeFrame(0x04, 0x40));
    stream.append(makeFrame(0x01, 0x12));
    stream.append(makeFrame(0x04, 0x41));

    StreamParser parser;
    for (const auto &config : configs)
        parser.addConfig(config);
    const auto results = parser.parseBatch(stream);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QMap<QString, FrameConfig> configMap;
    for (const auto &config : configs)
        configMap[config.name] = config.config;

    QString errorMsg;
    const QStringList saved = DataExporter::autoSaveMultiConfig(
        dir.path(), results, configMap, "end", -5, &errorMsg);

    QVERIFY2(errorMsg.isEmpty(), qPrintable(errorMsg));
    QCOMPARE(saved.size(), 2);
    const QString firstName = QFileInfo(saved[0]).fileName();
    const QString secondName = QFileInfo(saved[1]).fileName();
    QVERIFY2(firstName.startsWith("-5-data_"), qPrintable(firstName));
    QVERIFY2(secondName.startsWith("-4-data_"), qPrintable(secondName));
    QVERIFY(!firstName.contains("_session"));
    QVERIFY(!secondName.contains("_session"));
    QCOMPARE(exportedDataRowCount(saved[0]), 2);
    QCOMPARE(exportedDataRowCount(saved[1]), 1);
}

void StreamParserTest::streamingAutoSaveSplitsAcrossChunkBoundaries() {
    QVector<ConfigEntry> configs = {
        {"data", {}, makeConfig(0x01, true, FieldType::DATA), false},
        {"end", {}, makeConfig(0x04, true, FieldType::DATA), true},
    };

    QByteArray stream;
    stream.append(makeFrame(0x01, 0x10));
    stream.append(makeFrame(0x01, 0x11));
    stream.append(makeFrame(0x04, 0x40));
    stream.append(makeFrame(0x01, 0x12));
    stream.append(makeFrame(0x04, 0x41));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    StreamParser parser;
    for (const auto &config : configs)
        parser.addConfig(config);
    parser.setAutoSaveDir(dir.path());

    QStringList saved;
    connect(&parser, &StreamParser::autoSaveCompleted, this,
            [&saved](const QStringList &files) { saved.append(files); });

    for (int i = 0; i < stream.size(); ++i)
        parser.feedData(stream.mid(i, 1));

    QCOMPARE(saved.size(), 2);
    QCOMPARE(exportedDataRowCount(saved[0]), 2);
    QCOMPARE(exportedDataRowCount(saved[1]), 1);
}

void StreamParserTest::streamingAutoSaveUsesConfiguredLeadingSequence() {
    QVector<ConfigEntry> configs = {
        {"data", {}, makeConfig(0x01, true, FieldType::DATA), false},
        {"end", {}, makeConfig(0x04, true, FieldType::DATA), true},
    };

    QByteArray stream;
    stream.append(makeFrame(0x01, 0x10));
    stream.append(makeFrame(0x01, 0x11));
    stream.append(makeFrame(0x04, 0x40));
    stream.append(makeFrame(0x01, 0x12));
    stream.append(makeFrame(0x04, 0x41));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    StreamParser parser;
    for (const auto &config : configs)
        parser.addConfig(config);
    parser.setAutoSaveDir(dir.path());
    parser.setAutoSaveSequenceStart(-5);

    QStringList saved;
    connect(&parser, &StreamParser::autoSaveCompleted, this,
            [&saved](const QStringList &files) { saved.append(files); });

    parser.feedData(stream);

    QCOMPARE(saved.size(), 2);
    const QString firstName = QFileInfo(saved[0]).fileName();
    const QString secondName = QFileInfo(saved[1]).fileName();
    QVERIFY2(firstName.startsWith("-5-data_"), qPrintable(firstName));
    QVERIFY2(secondName.startsWith("-4-data_"), qPrintable(secondName));
    QVERIFY(!firstName.contains("_session"));
    QVERIFY(!secondName.contains("_session"));
    QCOMPARE(exportedDataRowCount(saved[0]), 2);
    QCOMPARE(exportedDataRowCount(saved[1]), 1);
}

void StreamParserTest::streamingAutoSaveSequenceIsPerParserInstance() {
    QVector<ConfigEntry> configs = {
        {"data", {}, makeConfig(0x01, true, FieldType::DATA), false},
        {"end", {}, makeConfig(0x04, true, FieldType::DATA), true},
    };

    QByteArray stream;
    stream.append(makeFrame(0x01, 0x10));
    stream.append(makeFrame(0x04, 0x40));

    auto savedFiles = [&](const QString &dirPath) {
        StreamParser parser;
        for (const auto &config : configs)
            parser.addConfig(config);
        parser.setAutoSaveDir(dirPath);
        parser.setAutoSaveSequenceStart(5);

        QStringList saved;
        connect(&parser, &StreamParser::autoSaveCompleted, this,
                [&saved](const QStringList &files) { saved.append(files); });
        parser.feedData(stream);
        return saved;
    };

    QTemporaryDir firstDir;
    QTemporaryDir secondDir;
    QVERIFY(firstDir.isValid());
    QVERIFY(secondDir.isValid());

    const QStringList firstSaved = savedFiles(firstDir.path());
    const QStringList secondSaved = savedFiles(secondDir.path());
    QCOMPARE(firstSaved.size(), 1);
    QCOMPARE(secondSaved.size(), 1);

    const QString firstName = QFileInfo(firstSaved[0]).fileName();
    const QString secondName = QFileInfo(secondSaved[0]).fileName();

    QVERIFY2(firstName.startsWith("5-data_"), qPrintable(firstName));
    QVERIFY2(secondName.startsWith("5-data_"), qPrintable(secondName));
}

void StreamParserTest::generatedFileStreamingMatchesBatchAutoSave() {
    const QString streamPath = findGeneratedStreamPath();
    QVERIFY2(!streamPath.isEmpty(), "test/test_stream_mixed.txt was not found");

    QString errorMsg;
    const QByteArray rawData = FileDataSource::readHexFile(streamPath, &errorMsg);
    QVERIFY2(errorMsg.isEmpty(), qPrintable(errorMsg));
    QVERIFY(!rawData.isEmpty());

    const QVector<ConfigEntry> configs = generatedConfigs();

    QTemporaryDir batchDir;
    QVERIFY(batchDir.isValid());
    StreamParser batchParser;
    for (const auto &config : configs)
        batchParser.addConfig(config);
    batchParser.setAutoSaveDir(batchDir.path());

    QStringList batchSaved;
    connect(&batchParser, &StreamParser::autoSaveCompleted, this,
            [&batchSaved](const QStringList &files) { batchSaved.append(files); });
    const auto batchResults = batchParser.parseBatch(rawData);

    QCOMPARE(batchResults.value("test_config").size(), 5064);
    QCOMPARE(batchResults.value("test_config_with_length").size(), 100000);
    QCOMPARE(batchResults.value("test_config_multi_padding").size(), 20000);
    QCOMPARE(batchResults.value("test_config_endframe").size(), 10);

    QTemporaryDir streamDir;
    QVERIFY(streamDir.isValid());
    StreamParser streamParser;
    for (const auto &config : configs)
        streamParser.addConfig(config);
    streamParser.setAutoSaveDir(streamDir.path());

    QStringList streamSaved;
    connect(&streamParser, &StreamParser::autoSaveCompleted, this,
            [&streamSaved](const QStringList &files) { streamSaved.append(files); });

    for (int offset = 0; offset < rawData.size();) {
        const int chunkSize = 1 + (offset % 257);
        streamParser.feedData(rawData.mid(offset, chunkSize));
        offset += chunkSize;
    }

    QCOMPARE(batchSaved.size(), 30);
    QCOMPARE(streamSaved.size(), batchSaved.size());
    QCOMPARE(exportedRowsByConfig(streamSaved), exportedRowsByConfig(batchSaved));
}

QTEST_MAIN(StreamParserTest)
#include "tst_streamparser.moc"
