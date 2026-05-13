#include "StreamParser.h"
#include "../export/DataExporter.h"
#include <QDir>
#include <QtEndian>
#include <algorithm>
#include <limits>
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

void StreamParser::setAutoSaveSequenceStart(int start) {
    m_autoSaveSequenceStart = start;
    resetAutoSaveSequence();
}

void StreamParser::resetAutoSaveSequence() {
    m_nextAutoSaveSequence = m_autoSaveSequenceStart;
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

bool StreamParser::configHasActiveCrc(const FrameConfig &config) {
    for (const auto &f : config.fields) {
        if (f.fieldType == FieldType::CRC && f.crcAlgorithm != CrcAlgorithm::NONE)
            return true;
    }
    return false;
}

int StreamParser::fixedSpecificityScore(const FrameConfig &config) {
    int score = 0;
    for (const auto &f : config.fields) {
        if (f.fieldType != FieldType::HEADER)
            score += f.fixedValue.size();
    }
    return score;
}

int StreamParser::findEarliestHeaderPos(const QByteArray &data, int offset) const {
    int bestPos = std::numeric_limits<int>::max();

    for (int i = 0; i < m_configs.size(); ++i) {
        int pos = findHeaderIn(data, offset, m_cachedHeaders[i]);
        if (pos >= 0 && pos < bestPos)
            bestPos = pos;
    }

    return bestPos == std::numeric_limits<int>::max() ? -1 : bestPos;
}

bool StreamParser::selectMatchingConfig(const QByteArray &data, int headerPos,
                                        int &configIdx, int &frameSize,
                                        ParsedFrame &frame, bool &needMoreData) {
    configIdx = -1;
    frameSize = 0;
    needMoreData = false;

    bool found = false;
    int bestCrcScore = -1;
    int bestSpecificity = -1;
    int bestHeaderSize = -1;
    int bestFixedBytes = -1;

    for (int i = 0; i < m_configs.size(); ++i) {
        const QByteArray &header = m_cachedHeaders[i];
        if (header.isEmpty())
            continue;
        if (headerPos + header.size() > data.size())
            continue;
        if (std::memcmp(data.constData() + headerPos, header.constData(), header.size()) != 0)
            continue;

        int candidateSize = computeFrameSize(m_configs[i].config, data, headerPos);
        if (candidateSize <= 0)
            continue;
        if (headerPos + candidateSize > data.size()) {
            needMoreData = true;
            continue;
        }

        ParsedFrame candidateFrame;
        if (!m_parsers[i].parseSingleFrame(data, headerPos, candidateSize, candidateFrame))
            continue;

        const FrameConfig &candidateConfig = m_configs[i].config;
        bool hasActiveCrc = configHasActiveCrc(candidateConfig);
        int crcScore = hasActiveCrc ? (candidateFrame.crcValid ? 2 : 0) : 1;
        int specificity = fixedSpecificityScore(candidateConfig);
        int headerSize = header.size();
        int fixedBytes = specificity + headerSize;

        bool better = !found || crcScore > bestCrcScore
                      || (crcScore == bestCrcScore && specificity > bestSpecificity)
                      || (crcScore == bestCrcScore && specificity == bestSpecificity
                          && headerSize > bestHeaderSize)
                      || (crcScore == bestCrcScore && specificity == bestSpecificity
                          && headerSize == bestHeaderSize && fixedBytes > bestFixedBytes);

        if (better) {
            found = true;
            bestCrcScore = crcScore;
            bestSpecificity = specificity;
            bestHeaderSize = headerSize;
            bestFixedBytes = fixedBytes;
            configIdx = i;
            frameSize = candidateSize;
            frame = std::move(candidateFrame);
        }
    }

    return found;
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
    resetAutoSaveSequence();
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
        int bestPos = findEarliestHeaderPos(m_buffer, offset);
        if (bestPos < 0) {
            // No header found — keep tail bytes for next chunk
            int keep = std::min(m_maxHeaderSize - 1, m_buffer.size() - offset);
            if (keep > 0)
                m_buffer = m_buffer.right(keep);
            else
                m_buffer.clear();
            return;
        }

        int bestIdx = -1;
        int frameSize = 0;
        ParsedFrame frame;
        bool needMoreData = false;

        if (selectMatchingConfig(m_buffer, bestPos, bestIdx, frameSize, frame, needMoreData)) {
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
        } else if (needMoreData) {
            break;
        } else {
            // False positive header, move one byte forward and rescan.
            offset = bestPos + 1;
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

    QStringList saved = saveFramesToDir(m_accumulatedFrames);
    clearAccumulatedFrames();
    if (!saved.isEmpty())
        emit autoSaveCompleted(saved);
}

void StreamParser::flushAndSave() {
    performAutoSave();
}

QStringList StreamParser::saveFramesToDir(
    const QMap<QString, QVector<ParsedFrame>> &frames) {
    const int sequence = m_nextAutoSaveSequence++;

    QDir dir(m_autoSaveDir);
    if (!dir.exists())
        dir.mkpath(".");

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QStringList savedFiles;

    for (const auto &entry : m_configs) {
        if (entry.isEndFrame)
            continue;

        const auto &flist = frames.value(entry.name);
        if (flist.isEmpty())
            continue;

        QString fileName = QString("%1-%2_%3.txt").arg(sequence).arg(entry.name).arg(timestamp);
        QString fullPath = dir.absoluteFilePath(fileName);

        QString errorMsg;
        if (DataExporter::exportToTxt(fullPath, flist, entry.config, &errorMsg))
            savedFiles << fullPath;
    }

    return savedFiles;
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

    // Check if auto-save is configured (has end frame + save dir)
    bool hasEndFrame = false;
    for (const auto &c : m_configs) {
        if (c.isEndFrame) {
            hasEndFrame = true;
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
        int bestPos = findEarliestHeaderPos(rawData, offset);
        if (bestPos < 0)
            break;

        int bestIdx = -1;
        int frameSize = 0;
        ParsedFrame frame;
        bool needMoreData = false;

        if (selectMatchingConfig(rawData, bestPos, bestIdx, frameSize, frame, needMoreData)) {
            const QString &name = m_configs[bestIdx].name;
            frame.configName = name;
            frame.frameIndex = ++counters[name];
            frame.offsetInStream = bestPos;

            if (doAutoSave && m_configs[bestIdx].isEndFrame) {
                // End frame detected: save accumulated session frames directly
                QStringList saved = saveFramesToDir(sessionFrames);
                if (!saved.isEmpty())
                    emit autoSaveCompleted(saved);
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
        } else if (needMoreData) {
            break;
        } else {
            offset = bestPos + 1;
        }

        if (progressCb) {
            int pct = static_cast<int>(static_cast<qint64>(offset) * 100 / totalSize);
            if (pct != lastPct) {
                progressCb(pct);
                lastPct = pct;
            }
        }
    }

    // Save remaining frames after last end frame
    if (doAutoSave) {
        bool hasRemaining = false;
        for (auto it = sessionFrames.constBegin(); it != sessionFrames.constEnd(); ++it) {
            if (!it.value().isEmpty()) {
                hasRemaining = true;
                break;
            }
        }
        if (hasRemaining) {
            QStringList saved = saveFramesToDir(sessionFrames);
            if (!saved.isEmpty())
                emit autoSaveCompleted(saved);
        }
    }

    // Restore non-lightweight mode
    for (auto &p : m_parsers)
        p.setLightweight(false);

    return results;
}
