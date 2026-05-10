#include "src/parser/StreamParser.h"

#include <QtTest/QtTest>

class StreamParserTest : public QObject {
    Q_OBJECT

private slots:
    void parsesSharedHeaderFormatsRegardlessOfOrder();
    void specificConfigBeatsGenericFirstConfig();

private:
    static QByteArray bytes(std::initializer_list<unsigned char> values);
    static FrameConfig makeConfig(unsigned char typeByte, bool fixedType, FieldType typeFieldType);
    static QByteArray makeFrame(unsigned char typeByte, unsigned char payload);
    static QMap<QString, int> parseCounts(const QVector<ConfigEntry> &configs,
                                          const QByteArray &stream);
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

QTEST_MAIN(StreamParserTest)
#include "tst_streamparser.moc"
