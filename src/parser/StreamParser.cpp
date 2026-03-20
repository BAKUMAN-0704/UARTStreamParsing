#include "StreamParser.h"
#include "../export/DataExporter.h"
#include <QDir>
#include <QtEndian>
#include <algorithm>
#include <cstring>

StreamParser::StreamParser(QObject *parent) : QObject(parent) {}

// ─── Config management ───

void StreamParser::addConfig(const ConfigEntry &entry) {
    m_configs.append(entry);
    FrameParser parser;
    parser.setConfig(entry.config);
    m_parsers.append(std::move(parser));
    m_cachedHeaders.append(entry.config.headerBytes());
    m_accumulatedFrames[entry.name] = {};
    m_frameCounters[entry.name] = 0;

    int hs = entry.config.headerSize();
    if (hs > m_maxHeaderSize)
        m_maxHeaderSize = hs;
}

void StreamParser::removeConfig(const QString &name) {
    for (int i = 0; i < m_configs.size(); ++i) {
        if (m_configs[i].name == name) {
            m_configs.remove(i);
            m_parsers.remove(i);
            m_cachedHeaders.remove(i);
            m_accumulatedFrames.remove(name);
            m_frameCounters.remove(name);
            break;
        }
    }
    // Recalculate max header size
    m_maxHeaderSize = 0;
    for (const auto &c : m_configs)
        m_maxHeaderSize = std::max(m_maxHeaderSize, c.config.headerSize());
}

void StreamParser::clearConfigs() {
    m_configs.clear();
    m_parsers.clear();
    m_cachedHeaders.clear();
    m_accumulatedFrames.clear();
    m_frameCounters.clear();
    m_maxHeaderSize = 0;
}

void StreamParser::setEndFrameConfig(const QString &name) {
    for (auto &c : m_configs)
        c.isEndFrame = (c.name == name);
}

void StreamParser::clearAccumulatedFrames() {
    for (auto it = m_accumulatedFrames.begin(); it != m_accumulatedFrames.end(); ++it)
        it.value().clear();
    for (auto it = m_frameCounters.begin(); it != m_frameCounters.end(); ++it)
        it.value() = 0;
}

// ─── Header scanning ───

int StreamParser::findHeaderIn(const QByteArray &data, int offset,
                               const QByteArray &headerBytes) {
    if (headerBytes.isEmpty())
        return offset;
    const char *dp = data.constData();
    const char *hp = headerBytes.constData();
    const int hLen = headerBytes.size();
    const int limit = data.size() - hLen;
    for (int i = offset; i <= limit; ++i) {
        if (std::memcmp(dp + i, hp, hLen) == 0)
            return i;
    }
    return -1;
}

// ─── Frame size calculation ───

int StreamParser::computeFrameSize(const FrameConfig &config, const QByteArray &data,
                                   int offset) {
    if (!config.hasLengthField())
        return config.totalFrameSize();

    for (const auto &f : config.fields) {
        if (f.fieldType != FieldType::LENGTH)
            continue;

        int fieldOff = config.fieldOffset(f.index);
        if (fieldOff < 0 || offset + fieldOff + f.byteCount > data.size())
            return config.totalFrameSize();

        const char *lp = data.constData() + offset + fieldOff;
        uint32_t lengthVal = 0;
        if (f.byteCount == 1) {
            lengthVal = static_cast<uint8_t>(*lp);
        } else if (f.byteCount == 2) {
            lengthVal = (f.endianness == Endianness::BIG) ? qFromBigEndian<uint16_t>(lp)
                                                          : qFromLittleEndian<uint16_t>(lp);
        } else if (f.byteCount == 4) {
            lengthVal = (f.endianness == Endianness::BIG) ? qFromBigEndian<uint32_t>(lp)
                                                          : qFromLittleEndian<uint32_t>(lp);
        }

        if (f.lengthMeaning == LengthMeaning::TOTAL)
            return static_cast<int>(lengthVal);

        int nonDataSize = 0;
        for (const auto &ff : config.fields)
            if (ff.fieldType != FieldType::DATA)
                nonDataSize += ff.byteCount;
        return static_cast<int>(lengthVal) + nonDataSize;
    }
    return config.totalFrameSize();
}

// ─── Streaming mode ───

void StreamParser::feedData(const QByteArray &chunk) {
    // Enable lightweight mode on first feedData call
    if (!m_streamingMode) {
        m_streamingMode = true;
        for (auto &p : m_parsers)
            p.setLightweight(true);
        m_progressTimer.start();
        m_totalStreamFrames = 0;
    }
    m_buffer.append(chunk);
    tryParseBuffer();
}

void StreamParser::resetStream() {
    m_buffer.clear();
    m_streamingMode = false;
    m_totalStreamFrames = 0;
    for (auto &p : m_parsers)
        p.setLightweight(false);
    clearAccumulatedFrames();
}

void StreamParser::emitThrottledProgress() {
    if (!m_progressTimer.isValid() || m_progressTimer.elapsed() >= 100) {
        emit streamProgress(m_totalStreamFrames);
        m_progressTimer.restart();
    }
}

void StreamParser::tryParseBuffer() {
    if (m_configs.isEmpty())
        return;

    int offset = 0;

    while (offset < m_buffer.size()) {
        // Find earliest header match across all configs
        int bestPos = INT_MAX;
        int bestIdx = -1;

        for (int i = 0; i < m_configs.size(); ++i) {
            int pos = findHeaderIn(m_buffer, offset, m_cachedHeaders[i]);
            if (pos >= 0 && pos < bestPos) {
                bestPos = pos;
                bestIdx = i;
            }
        }

        if (bestIdx < 0) {
            // No header found — keep tail bytes for next chunk
            int keep = std::min(m_maxHeaderSize - 1, m_buffer.size() - offset);
            if (keep > 0)
                m_buffer = m_buffer.right(keep);
            else
                m_buffer.clear();
            return;
        }

        // Compute frame size
        int frameSize = computeFrameSize(m_configs[bestIdx].config, m_buffer, bestPos);
        if (bestPos + frameSize > m_buffer.size())
            break; // Not enough data yet

        // Extract and parse one frame
        QByteArray oneFrame = m_buffer.mid(bestPos, frameSize);
        QVector<ParsedFrame> result = m_parsers[bestIdx].parse(oneFrame);

        if (!result.isEmpty()) {
            ParsedFrame frame = std::move(result.first());
            const QString &configName = m_configs[bestIdx].name;
            frame.configName = configName;
            frame.frameIndex = ++m_frameCounters[configName];
            frame.offsetInStream = bestPos;

            if (m_streamingMode) {
                ++m_totalStreamFrames;
                emitThrottledProgress();
            } else {
                emit frameParsed(frame, configName);
            }

            if (m_configs[bestIdx].isEndFrame) {
                emit endFrameDetected();
                performAutoSave();
            } else {
                m_accumulatedFrames[configName].append(std::move(frame));
            }

            offset = bestPos + frameSize;
        } else {
            // Invalid frame at header position, skip past header
            offset = bestPos + m_configs[bestIdx].config.headerSize();
        }
    }

    // Trim consumed data
    if (offset > 0)
        m_buffer = m_buffer.mid(offset);
}

// ─── Auto-save ───

void StreamParser::performAutoSave() {
    if (m_autoSaveDir.isEmpty())
        return;

    QDir dir(m_autoSaveDir);
    if (!dir.exists())
        dir.mkpath(".");

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QStringList savedFiles;

    for (int i = 0; i < m_configs.size(); ++i) {
        const auto &entry = m_configs[i];
        if (entry.isEndFrame)
            continue;

        const auto &frames = m_accumulatedFrames.value(entry.name);
        if (frames.isEmpty())
            continue;

        QString fileName = entry.name + "_" + timestamp + ".txt";
        QString fullPath = dir.absoluteFilePath(fileName);

        QString errorMsg;
        if (DataExporter::exportToTxt(fullPath, frames, entry.config, &errorMsg))
            savedFiles << fullPath;
    }

    clearAccumulatedFrames();
    if (!savedFiles.isEmpty())
        emit autoSaveCompleted(savedFiles);
}

// ─── Batch mode ───

QMap<QString, QVector<ParsedFrame>> StreamParser::parseBatch(const QByteArray &rawData,
                                                             ProgressCallback progressCb) {
    QMap<QString, QVector<ParsedFrame>> results;
    QMap<QString, int> counters;
    for (const auto &c : m_configs) {
        results[c.name] = {};
        counters[c.name] = 0;
    }

    if (m_configs.isEmpty() || rawData.isEmpty())
        return results;

    // Enable lightweight mode for batch parsing (skip rawHex)
    for (auto &p : m_parsers)
        p.setLightweight(true);

    // Cache header bytes to avoid repeated allocation
    QVector<QByteArray> cachedHeaders;
    cachedHeaders.reserve(m_configs.size());
    for (const auto &c : m_configs)
        cachedHeaders.append(c.config.headerBytes());

    // Check if auto-save is configured (has end frame + save dir)
    bool hasEndFrame = false;
    QString endFrameName;
    for (const auto &c : m_configs) {
        if (c.isEndFrame) {
            hasEndFrame = true;
            endFrameName = c.name;
            break;
        }
    }
    bool doAutoSave = hasEndFrame && !m_autoSaveDir.isEmpty();

    // Accumulated frames for auto-save between end frames
    QMap<QString, QVector<ParsedFrame>> sessionFrames;
    if (doAutoSave) {
        for (const auto &c : m_configs)
            sessionFrames[c.name] = {};
    }

    const int totalSize = rawData.size();
    int offset = 0;
    int lastPct = -1;

    while (offset < totalSize) {
        // Find earliest header
        int bestPos = INT_MAX;
        int bestIdx = -1;

        for (int i = 0; i < m_configs.size(); ++i) {
            int pos = findHeaderIn(rawData, offset, cachedHeaders[i]);
            if (pos >= 0 && pos < bestPos) {
                bestPos = pos;
                bestIdx = i;
            }
        }

        if (bestIdx < 0)
            break;

        int frameSize = computeFrameSize(m_configs[bestIdx].config, rawData, bestPos);
        if (bestPos + frameSize > totalSize)
            break;

        QByteArray oneFrame = rawData.mid(bestPos, frameSize);
        QVector<ParsedFrame> parsed = m_parsers[bestIdx].parse(oneFrame);

        if (!parsed.isEmpty()) {
            ParsedFrame frame = std::move(parsed.first());
            const QString &name = m_configs[bestIdx].name;
            frame.configName = name;
            frame.frameIndex = ++counters[name];
            frame.offsetInStream = bestPos;

            if (doAutoSave && m_configs[bestIdx].isEndFrame) {
                // End frame detected: save accumulated session frames
                m_accumulatedFrames = sessionFrames;
                performAutoSave();
                // Reset session
                for (auto it = sessionFrames.begin(); it != sessionFrames.end(); ++it)
                    it.value().clear();
            }

            // Always add to total results
            results[name].append(frame);

            // Also accumulate for next auto-save session
            if (doAutoSave && !m_configs[bestIdx].isEndFrame)
                sessionFrames[name].append(std::move(frame));

            offset = bestPos + frameSize;
        } else {
            offset = bestPos + m_configs[bestIdx].config.headerSize();
        }

        if (progressCb) {
            int pct = static_cast<int>(static_cast<qint64>(offset) * 100 / totalSize);
            if (pct != lastPct) {
                progressCb(pct);
                lastPct = pct;
            }
        }
    }

    // Restore non-lightweight mode
    for (auto &p : m_parsers)
        p.setLightweight(false);

    return results;
}
