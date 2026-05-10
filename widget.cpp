#include "widget.h"
#if __has_include("ui_widget.h")
#include "ui_widget.h"
#elif __has_include("build/UARTStreamParsing_autogen/include/ui_widget.h")
#include "build/UARTStreamParsing_autogen/include/ui_widget.h"
#else
#error "ui_widget.h not found. Run CMake configure/build to generate it."
#endif

#include "src/config/ConfigParser.h"
#include "src/export/DataExporter.h"
#include "src/worker/ParseWorker.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSerialPort>

Widget::Widget(QWidget *parent) : QWidget(parent), ui(new Ui::Widget) {
    ui->setupUi(this);
    initStyle();

    m_serialManager = new SerialPortManager(this);
    m_streamParser = new StreamParser(this);

    // Config management
    connect(ui->btnBrowseConfig, &QPushButton::clicked, this, &Widget::onBrowseConfig);
    connect(ui->btnRemoveConfig, &QPushButton::clicked, this, &Widget::onRemoveConfig);
    connect(ui->listConfigs, &QListWidget::currentRowChanged, this,
            &Widget::onConfigSelectionChanged);
    connect(ui->chkEndFrame, &QCheckBox::toggled, this, &Widget::onEndFrameToggled);
    connect(ui->btnBrowseAutoSaveDir, &QPushButton::clicked, this,
            &Widget::onBrowseAutoSaveDir);

    // Data source
    connect(ui->radioSerial, &QRadioButton::toggled, this, &Widget::onSourceChanged);
    connect(ui->radioFile, &QRadioButton::toggled, this, &Widget::onSourceChanged);
    connect(ui->btnRefreshPorts, &QPushButton::clicked, this, &Widget::onRefreshPorts);
    connect(ui->btnOpenPort, &QPushButton::clicked, this, &Widget::onOpenPort);
    connect(ui->btnClosePort, &QPushButton::clicked, this, &Widget::onClosePort);
    connect(ui->btnBrowseDataFile, &QPushButton::clicked, this, &Widget::onBrowseDataFile);
    connect(m_serialManager, &SerialPortManager::dataReceived, this,
            &Widget::onSerialDataReceived);
    connect(m_serialManager, &SerialPortManager::errorOccurred, this,
            &Widget::onSerialError);

    // Parse & export
    connect(ui->btnParse, &QPushButton::clicked, this, &Widget::onParse);
    connect(ui->btnExport, &QPushButton::clicked, this, &Widget::onExport);

    // StreamParser signals
    connect(m_streamParser, &StreamParser::streamProgress, this,
            [this](int total) {
                setStatus(QString("实时解析中... 已解析 %1 帧").arg(total));
            });
    connect(m_streamParser, &StreamParser::endFrameDetected, this,
            &Widget::onEndFrameDetected);
    connect(m_streamParser, &StreamParser::autoSaveCompleted, this,
            &Widget::onAutoSaveCompleted);

    initSerialUI();
    onSourceChanged();
}

Widget::~Widget() { delete ui; }

void Widget::initStyle() {
    setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            border: 1px solid #c0c0c0;
            border-radius: 6px;
            margin-top: 10px;
            padding-top: 14px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
        }
        QLineEdit {
            padding: 4px 8px;
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            background: white;
        }
        QLineEdit:focus {
            border-color: #0078d4;
        }
        QPushButton {
            padding: 4px 12px;
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            background: #f0f0f0;
        }
        QPushButton:hover {
            background: #e0e0e0;
            border-color: #0078d4;
        }
        QPushButton:pressed {
            background: #d0d0d0;
        }
        QPushButton:disabled {
            color: #a0a0a0;
            background: #f5f5f5;
        }
        QPushButton#btnParse {
            background: #0078d4;
            color: white;
            font-weight: bold;
            border: none;
        }
        QPushButton#btnParse:hover {
            background: #106ebe;
        }
        QPushButton#btnParse:pressed {
            background: #005a9e;
        }
        QPushButton#btnParse:disabled {
            background: #a0c4e8;
            color: #e0e0e0;
        }
        QPushButton#btnExport {
            background: #107c10;
            color: white;
            font-weight: bold;
            border: none;
        }
        QPushButton#btnExport:hover {
            background: #0e6b0e;
        }
        QPushButton#btnExport:pressed {
            background: #0b5a0b;
        }
        QPushButton#btnExport:disabled {
            background: #a0c8a0;
            color: #e0e0e0;
        }
        QProgressBar {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            text-align: center;
            height: 20px;
        }
        QProgressBar::chunk {
            background: #0078d4;
            border-radius: 3px;
        }
        QLabel#labelStatus {
            color: #606060;
            padding: 4px;
        }
        QComboBox {
            padding: 3px 8px;
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            background: white;
        }
        QRadioButton {
            spacing: 6px;
        }
        QListWidget {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            background: white;
            font-size: 12px;
        }
        QListWidget::item {
            padding: 3px 6px;
        }
        QListWidget::item:selected {
            background: #0078d4;
            color: white;
        }
    )");
}

void Widget::initSerialUI() {
    ui->comboBaud->addItems(
        {"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"});
    ui->comboBaud->setCurrentText("115200");
    ui->comboDataBits->addItems({"5", "6", "7", "8"});
    ui->comboDataBits->setCurrentText("8");
    ui->comboStopBits->addItems({"1", "1.5", "2"});
    ui->comboStopBits->setCurrentText("1");
    ui->comboParity->addItems({"None", "Even", "Odd", "Space", "Mark"});
    ui->comboParity->setCurrentText("None");
    onRefreshPorts();
}

// ─── Config management ───

void Widget::onBrowseConfig() {
    QStringList filePaths = QFileDialog::getOpenFileNames(
        this, "选择配置文件 (可多选)", QString(),
        "配置文件 (*.xlsx *.csv);;Excel文件 (*.xlsx);;CSV文件 (*.csv)");
    if (filePaths.isEmpty())
        return;

    // Load each selected file directly
    int added = 0;
    QStringList errors;
    for (const QString &path : filePaths) {
        QString name = QFileInfo(path).baseName();

        // Skip duplicates
        bool exists = false;
        for (const auto &c : m_streamParser->configs()) {
            if (c.name == name) {
                exists = true;
                break;
            }
        }
        if (exists)
            continue;

        QString errorMsg;
        FrameConfig config = ConfigParser::loadConfig(path, &errorMsg);
        if (!errorMsg.isEmpty()) {
            errors << QString("%1: %2").arg(name, errorMsg);
            continue;
        }

        ConfigEntry entry{name, path, config, false};
        m_streamParser->addConfig(entry);
        ui->listConfigs->addItem(name);
        ++added;
    }

    ui->editConfigPath->clear();
    updateParseButton();

    if (!errors.isEmpty())
        QMessageBox::warning(this, "部分加载失败", errors.join("\n"));

    if (added > 0)
        setStatus(QString("已添加 %1 个配置").arg(added));
}

void Widget::onRemoveConfig() {
    auto *item = ui->listConfigs->currentItem();
    if (!item)
        return;
    QString name = item->text().split(" ").first(); // strip suffix like " [结束帧]"
    m_streamParser->removeConfig(name);
    delete ui->listConfigs->takeItem(ui->listConfigs->currentRow());
    updateParseButton();
    setStatus(QString("已移除配置: %1").arg(name));
}

void Widget::onConfigSelectionChanged() {
    auto *item = ui->listConfigs->currentItem();
    if (!item) {
        ui->chkEndFrame->setEnabled(false);
        ui->chkEndFrame->setChecked(false);
        return;
    }
    ui->chkEndFrame->setEnabled(true);
    QString name = item->text().split(" ").first();
    for (const auto &c : m_streamParser->configs()) {
        if (c.name == name) {
            ui->chkEndFrame->blockSignals(true);
            ui->chkEndFrame->setChecked(c.isEndFrame);
            ui->chkEndFrame->blockSignals(false);
            return;
        }
    }
}

void Widget::onEndFrameToggled(bool checked) {
    auto *item = ui->listConfigs->currentItem();
    if (!item)
        return;
    QString name = item->text().split(" ").first();

    if (checked) {
        m_streamParser->setEndFrameConfig(name);
        // Update all list items display
        for (int i = 0; i < ui->listConfigs->count(); ++i) {
            auto *it = ui->listConfigs->item(i);
            QString n = it->text().split(" ").first();
            it->setText(n == name ? name + " [结束帧]" : n);
        }
    } else {
        m_streamParser->setEndFrameConfig("");
        item->setText(name);
    }
}

void Widget::onBrowseAutoSaveDir() {
    QString dir =
        QFileDialog::getExistingDirectory(this, "选择自动保存目录", m_streamParser->autoSaveDir());
    if (!dir.isEmpty()) {
        ui->editAutoSaveDir->setText(dir);
        m_streamParser->setAutoSaveDir(dir);
    }
}

// ─── Data source ───

void Widget::onSourceChanged() {
    if (ui->radioSerial->isChecked())
        ui->stackedSource->setCurrentIndex(1);
    else
        ui->stackedSource->setCurrentIndex(0);
    updateParseButton();
}

void Widget::onRefreshPorts() {
    ui->comboPort->clear();
    ui->comboPort->addItems(SerialPortManager::availablePorts());
}

void Widget::onOpenPort() {
    SerialConfig config;
    config.portName = ui->comboPort->currentText();
    config.baudRate = ui->comboBaud->currentText().toInt();
    config.dataBits =
        static_cast<QSerialPort::DataBits>(ui->comboDataBits->currentText().toInt());

    QString stopBits = ui->comboStopBits->currentText();
    if (stopBits == "1")
        config.stopBits = QSerialPort::OneStop;
    else if (stopBits == "1.5")
        config.stopBits = QSerialPort::OneAndHalfStop;
    else
        config.stopBits = QSerialPort::TwoStop;

    QString parity = ui->comboParity->currentText();
    if (parity == "Even")
        config.parity = QSerialPort::EvenParity;
    else if (parity == "Odd")
        config.parity = QSerialPort::OddParity;
    else if (parity == "Space")
        config.parity = QSerialPort::SpaceParity;
    else if (parity == "Mark")
        config.parity = QSerialPort::MarkParity;
    else
        config.parity = QSerialPort::NoParity;

    // Warn if auto-save directory is not set
    if (ui->editAutoSaveDir->text().trimmed().isEmpty()) {
        auto ret = QMessageBox::warning(this, "提示",
            "未设置自动保存目录，解析到结束帧时数据不会自动保存。\n是否继续？",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::No)
            return;
    }

    if (m_serialManager->openPort(config)) {
        ui->btnOpenPort->setEnabled(false);
        ui->btnClosePort->setEnabled(true);
        ui->labelSerialStatus->setText("已连接 - " + config.portName);
        m_rawData.clear();
        m_streamParser->resetStream();

        // Sync auto-save dir from UI before streaming starts
        QString autoSaveDir = ui->editAutoSaveDir->text().trimmed();
        if (!autoSaveDir.isEmpty())
            m_streamParser->setAutoSaveDir(autoSaveDir);

        // Serial always uses real-time streaming when configs are loaded
        bool hasConfigs = !m_streamParser->configs().isEmpty();
        m_streamingActive = hasConfigs;

        // Reset HEX text conversion state
        m_pendingNibble = -1;
        m_hexSkipNextX = false;

        if (m_streamingActive)
            setStatus("实时解析已启动 - " + config.portName);
        else
            setStatus("串口已打开 (未加载配置) - " + config.portName);
        updateParseButton();
    }
}

void Widget::onClosePort() {
    // Save any remaining accumulated frames before closing
    if (m_streamingActive && !m_streamParser->autoSaveDir().isEmpty()) {
        const auto &remaining = m_streamParser->accumulatedFrames();
        bool hasData = false;
        for (auto it = remaining.constBegin(); it != remaining.constEnd(); ++it) {
            if (!it.value().isEmpty()) {
                hasData = true;
                break;
            }
        }
        if (hasData) {
            m_streamParser->flushAndSave();
            setStatus("串口关闭前已保存剩余数据");
        }
    }

    m_serialManager->closePort();
    m_streamingActive = false;
    ui->btnOpenPort->setEnabled(true);
    ui->btnClosePort->setEnabled(false);
    ui->labelSerialStatus->setText("未连接");
    setStatus("串口已关闭");
}

void Widget::onBrowseDataFile() {
    QString filePath = QFileDialog::getOpenFileName(
        this, "选择数据文件", QString(), "文本文件 (*.txt);;所有文件 (*.*)");
    if (!filePath.isEmpty()) {
        ui->editDataFilePath->setText(filePath);
        updateParseButton();
    }
}

// ─── Serial data ───

static inline int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

QByteArray Widget::convertHexTextToBinary(const QByteArray &hexText) {
    QByteArray result;
    result.reserve(hexText.size() / 2);
    const char *p = hexText.constData();
    int len = hexText.size();

    for (int i = 0; i < len; ++i) {
        char c = p[i];

        // Skip separators
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == ';') {
            // Separator: flush pending nibble as a single-digit byte
            if (m_pendingNibble >= 0) {
                result.append(static_cast<char>(m_pendingNibble));
                m_pendingNibble = -1;
            }
            m_hexSkipNextX = false;
            continue;
        }

        // Handle "0x" prefix
        if (m_hexSkipNextX) {
            m_hexSkipNextX = false;
            if (c == 'x' || c == 'X')
                continue;
            // Not 'x', so '0' was a real hex digit
            m_pendingNibble = 0;
        }

        int val = hexVal(c);
        if (val < 0) {
            // Non-hex char: flush pending and skip
            if (m_pendingNibble >= 0) {
                result.append(static_cast<char>(m_pendingNibble));
                m_pendingNibble = -1;
            }
            continue;
        }

        // Check for "0x" prefix: if this is '0' and no pending nibble
        if (val == 0 && m_pendingNibble < 0 && i + 1 < len &&
            (p[i + 1] == 'x' || p[i + 1] == 'X')) {
            m_hexSkipNextX = true;
            continue;
        }

        if (m_pendingNibble < 0) {
            m_pendingNibble = val;
        } else {
            result.append(static_cast<char>((m_pendingNibble << 4) | val));
            m_pendingNibble = -1;
        }
    }

    return result;
}

void Widget::onSerialDataReceived(const QByteArray &data) {
    if (m_streamingActive && !m_streamParser->configs().isEmpty()) {
        // Check if HEX text mode is enabled
        bool hexMode = ui->chkHexMode->isChecked();
        if (hexMode) {
            QByteArray binary = convertHexTextToBinary(data);
            if (!binary.isEmpty())
                m_streamParser->feedData(binary);
        } else {
            m_streamParser->feedData(data);
        }
    } else {
        m_rawData.append(data);
        updateParseButton();
        setStatus(QString("串口接收中... 已缓存 %1 字节").arg(m_rawData.size()));
    }
}

void Widget::onSerialError(const QString &msg) {
    QMessageBox::warning(this, "串口错误", msg);
}

// ─── StreamParser signals ───

void Widget::onEndFrameDetected() {
    setStatus("检测到结束帧，正在自动保存...");
}

void Widget::onAutoSaveCompleted(const QStringList &savedFiles) {
    setStatus(QString("自动保存完成: %1 个文件").arg(savedFiles.size()));
}

// ─── Parse ───

void Widget::onParse() {
    if (m_streamParser->configs().isEmpty()) {
        QMessageBox::warning(this, "提示", "请先添加配置文件");
        return;
    }

    bool isFileMode = ui->radioFile->isChecked();

    if (isFileMode) {
        QString path = ui->editDataFilePath->text();
        if (path.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先选择数据文件");
            return;
        }
        if (m_workerThread) {
            QMessageBox::warning(this, "提示", "解析正在进行中，请稍候");
            return;
        }

        setParsingUi(true);

        m_workerThread = new QThread(this);
        m_worker = new ParseWorker();
        m_worker->setConfigs(m_streamParser->configs());
        m_worker->setAutoSaveDir(m_streamParser->autoSaveDir());
        m_worker->moveToThread(m_workerThread);

        connect(m_workerThread, &QThread::started, m_worker,
                [worker = m_worker, path]() { worker->process(path); });

        connect(m_worker, &ParseWorker::progress, this,
                [this](int pct, const QString &status) {
                    ui->progressBar->setValue(pct);
                    setStatus(status);
                });

        connect(m_worker, &ParseWorker::finished, this,
                [this](bool success, const QString &errorMsg) {
                    if (success) {
                        m_rawData = std::move(m_worker->m_rawData);
                        m_parsedFramesByConfig = std::move(m_worker->m_framesByConfig);
                        QStringList autoSaved = m_worker->m_autoSavedFiles;

                        int total = 0;
                        for (const auto &f : m_parsedFramesByConfig)
                            total += f.size();

                        QString statusMsg =
                            QString("解析完成: %1 帧, 共 %2 字节")
                                .arg(total)
                                .arg(m_rawData.size());
                        if (!autoSaved.isEmpty())
                            statusMsg +=
                                QString(", 自动保存 %1 个文件").arg(autoSaved.size());
                        setStatus(statusMsg);
                        ui->btnExport->setEnabled(total > 0);

                        if (total == 0) {
                            QMessageBox::information(this, "解析结果", "未找到有效帧");
                        } else {
                            showResultDialog(m_parsedFramesByConfig);
                        }
                    } else {
                        QMessageBox::critical(this, "解析失败", errorMsg);
                    }

                    m_workerThread->quit();
                    m_workerThread->wait();
                    m_worker->deleteLater();
                    m_workerThread->deleteLater();
                    m_worker = nullptr;
                    m_workerThread = nullptr;
                    setParsingUi(false);
                });

        m_workerThread->start();

    } else {
        // Serial non-streaming mode
        if (m_serialManager->isOpen())
            m_rawData = m_serialManager->takeBuffer();

        if (m_rawData.isEmpty()) {
            QMessageBox::warning(this, "提示", "没有可解析的数据");
            return;
        }

        m_parsedFramesByConfig = m_streamParser->parseBatch(m_rawData);

        int total = 0;
        for (const auto &f : m_parsedFramesByConfig)
            total += f.size();

        setStatus(QString("解析完成: %1 帧, 共 %2 字节").arg(total).arg(m_rawData.size()));
        ui->btnExport->setEnabled(total > 0);

        if (total == 0) {
            QMessageBox::information(this, "解析结果", "未找到有效帧");
        } else {
            showResultDialog(m_parsedFramesByConfig);
        }
    }
}

// ─── Export ───

void Widget::onExport() {
    int total = 0;
    for (const auto &f : m_parsedFramesByConfig)
        total += f.size();

    if (total == 0) {
        QMessageBox::warning(this, "提示", "没有可导出的数据,请先解析");
        return;
    }

    const auto &configs = m_streamParser->configs();

    // Count configs with actual frames
    int configsWithFrames = 0;
    QString singleConfigName;
    for (auto it = m_parsedFramesByConfig.constBegin(); it != m_parsedFramesByConfig.constEnd();
         ++it) {
        if (!it.value().isEmpty()) {
            configsWithFrames++;
            singleConfigName = it.key();
        }
    }

    if (configsWithFrames == 1) {
        // Single config: save as single file
        QString filePath =
            QFileDialog::getSaveFileName(this, "导出结果", QString(), "文本文件 (*.txt)");
        if (filePath.isEmpty())
            return;

        FrameConfig fc;
        for (const auto &c : configs)
            if (c.name == singleConfigName) {
                fc = c.config;
                break;
            }

        QString errorMsg;
        if (DataExporter::exportToTxt(filePath, m_parsedFramesByConfig[singleConfigName], fc,
                                      &errorMsg)) {
            setStatus("导出成功: " + filePath);
            QMessageBox::information(this, "导出成功",
                                     QString("已导出 %1 帧到:\n%2").arg(total).arg(filePath));
        } else {
            QMessageBox::critical(this, "导出失败", errorMsg);
        }
    } else {
        // Multiple configs: save to directory
        QString dir = QFileDialog::getExistingDirectory(this, "选择导出目录");
        if (dir.isEmpty())
            return;

        QMap<QString, FrameConfig> configMap;
        QString endFrameName;
        for (const auto &c : configs) {
            configMap[c.name] = c.config;
            if (c.isEndFrame)
                endFrameName = c.name;
        }

        QString errorMsg;
        QStringList saved = DataExporter::autoSaveMultiConfig(
            dir, m_parsedFramesByConfig, configMap, endFrameName, &errorMsg);

        if (!errorMsg.isEmpty()) {
            QMessageBox::critical(this, "导出失败", errorMsg);
        } else {
            setStatus(QString("导出成功: %1 个文件").arg(saved.size()));
            QMessageBox::information(
                this, "导出成功",
                QString("已导出 %1 帧到 %2 个文件:\n%3")
                    .arg(total)
                    .arg(saved.size())
                    .arg(saved.join("\n")));
        }
    }
}

// ─── UI helpers ───

void Widget::updateParseButton() {
    bool hasConfigs = !m_streamParser->configs().isEmpty();
    bool hasData = !m_rawData.isEmpty() || m_serialManager->isOpen() ||
                   (ui->radioFile->isChecked() && !ui->editDataFilePath->text().isEmpty());
    bool isStreaming = m_streamingActive && ui->radioSerial->isChecked();
    ui->btnParse->setEnabled(hasConfigs && hasData && !isStreaming && !m_workerThread);
}

QString Widget::formatFieldValue(const ParsedField &pf) {
    switch (pf.dataType) {
    case DataType::FLOAT:
    case DataType::DOUBLE:
        return QString::number(pf.value.toDouble(), 'f', 6);
    case DataType::UINT8:
    case DataType::UINT16:
    case DataType::UINT32:
        return QString::number(pf.value.toUInt());
    case DataType::INT8:
    case DataType::INT16:
    case DataType::INT32:
        return QString::number(pf.value.toInt());
    default:
        return pf.value.toString();
    }
}

void Widget::showResultDialog(const QMap<QString, QVector<ParsedFrame>> &framesByConfig) {
    const auto &configs = m_streamParser->configs();
    int grandTotal = 0;
    QString msg;

    for (auto it = framesByConfig.constBegin(); it != framesByConfig.constEnd(); ++it) {
        const QString &configName = it.key();
        const auto &frames = it.value();
        if (frames.isEmpty())
            continue;
        grandTotal += frames.size();

        msg += QString("[%1] %2 帧\n").arg(configName).arg(frames.size());

        // Show first frame example
        const auto &frame = frames.first();
        for (const auto &entry : configs) {
            if (entry.name != configName)
                continue;
            for (const auto &field : entry.config.fields) {
                if (field.fieldType != FieldType::DATA)
                    continue;
                for (const auto &pf : frame.fields) {
                    if (pf.name == field.name && pf.fieldType == FieldType::DATA) {
                        msg += QString("  %1 = %2\n").arg(field.name, formatFieldValue(pf));
                        break;
                    }
                }
            }
            break;
        }
        msg += "\n";
    }

    QMessageBox::information(
        this, "解析结果",
        QString("解析完成，共 %1 帧\n\n%2请使用「导出」按钮将结果保存为 TXT 文件。")
            .arg(grandTotal)
            .arg(msg));
}

void Widget::setStatus(const QString &msg) { ui->labelStatus->setText(msg); }

void Widget::setParsingUi(bool parsing) {
    ui->progressBar->setVisible(parsing);
    ui->progressBar->setValue(0);
    ui->btnParse->setEnabled(!parsing);
    ui->btnExport->setEnabled(!parsing);
    ui->btnBrowseConfig->setEnabled(!parsing);
    ui->btnBrowseDataFile->setEnabled(!parsing);
}
