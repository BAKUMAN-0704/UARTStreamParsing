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
#include <QMessageBox>
#include <QSerialPort>
#include <QTimer>

Widget::Widget(QWidget *parent) : QWidget(parent), ui(new Ui::Widget) {
    ui->setupUi(this);
    initStyle();

    m_serialManager = new SerialPortManager(this);

    connect(ui->btnBrowseConfig, &QPushButton::clicked, this, &Widget::onBrowseConfig);
    connect(ui->btnLoadConfig, &QPushButton::clicked, this, &Widget::onLoadConfig);
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
    connect(ui->btnParse, &QPushButton::clicked, this, &Widget::onParse);
    connect(ui->btnExport, &QPushButton::clicked, this, &Widget::onExport);

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

void Widget::onBrowseConfig() {
    QString filePath = QFileDialog::getOpenFileName(
        this, "选择配置文件", QString(),
        "配置文件 (*.xlsx *.csv);;Excel文件 (*.xlsx);;CSV文件 (*.csv)");
    if (!filePath.isEmpty())
        ui->editConfigPath->setText(filePath);
}

void Widget::onLoadConfig() {
    QString path = ui->editConfigPath->text();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择配置文件");
        return;
    }

    QString errorMsg;
    m_config = ConfigParser::loadConfig(path, &errorMsg);
    if (!errorMsg.isEmpty()) {
        QMessageBox::critical(this, "加载失败", errorMsg);
        m_configLoaded = false;
        updateParseButton();
        return;
    }

    m_configLoaded = true;
    m_parser.setConfig(m_config);
    updateParseButton();
    setStatus(QString("配置已加载: %1 个字段, 帧大小 %2 字节")
                  .arg(m_config.fields.size())
                  .arg(m_config.totalFrameSize()));
}

void Widget::onSourceChanged() {
    if (ui->radioSerial->isChecked())
        ui->stackedSource->setCurrentIndex(1);
    else
        ui->stackedSource->setCurrentIndex(0);
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

    if (m_serialManager->openPort(config)) {
        ui->btnOpenPort->setEnabled(false);
        ui->btnClosePort->setEnabled(true);
        ui->labelSerialStatus->setText("已连接 - " + config.portName);
        m_rawData.clear();
        updateParseButton();
        setStatus("串口已打开: " + config.portName);
    }
}

void Widget::onClosePort() {
    m_serialManager->closePort();
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

void Widget::onParse() {
    if (!m_configLoaded) {
        QMessageBox::warning(this, "提示", "请先加载配置文件");
        return;
    }

    bool isFileMode = ui->radioFile->isChecked();

    if (isFileMode) {
        QString path = ui->editDataFilePath->text();
        if (path.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先选择数据文件");
            return;
        }

        // Prevent re-entry while a worker is running
        if (m_workerThread) {
            QMessageBox::warning(this, "提示", "解析正在进行中，请稍候");
            return;
        }

        setParsingUi(true);

        // Create worker + thread
        m_workerThread = new QThread(this);
        m_worker = new ParseWorker();
        m_worker->setConfig(m_config);
        m_worker->moveToThread(m_workerThread);

        // Start work when thread starts
        connect(m_workerThread, &QThread::started, m_worker,
                [worker = m_worker, path]() { worker->process(path); });

        // Progress updates (queued connection, safe for UI)
        connect(m_worker, &ParseWorker::progress, this,
                [this](int pct, const QString &status) {
                    ui->progressBar->setValue(pct);
                    setStatus(status);
                });

        // Finished handler
        connect(m_worker, &ParseWorker::finished, this,
                [this](bool success, const QString &errorMsg) {
                    if (success) {
                        // Move results from worker to widget
                        m_rawData = std::move(m_worker->m_rawData);
                        m_parsedFrames = std::move(m_worker->m_frames);

                        setStatus(QString("解析完成: %1 帧, 共 %2 字节")
                                      .arg(m_parsedFrames.size())
                                      .arg(m_rawData.size()));
                        ui->btnExport->setEnabled(!m_parsedFrames.isEmpty());

                        if (m_parsedFrames.isEmpty()) {
                            QMessageBox::information(this, "解析结果",
                                                     "未找到有效帧");
                        } else {
                            showResultDialog(m_parsedFrames);
                        }
                    } else {
                        QMessageBox::critical(this, "解析失败", errorMsg);
                    }

                    // Clean up thread
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
        // Serial mode: parse directly in UI thread (data is small)
        if (m_serialManager->isOpen())
            m_rawData = m_serialManager->takeBuffer();

        if (m_rawData.isEmpty()) {
            QMessageBox::warning(this, "提示", "没有可解析的数据");
            return;
        }

        m_parsedFrames = m_parser.parse(m_rawData);
        setStatus(QString("解析完成: %1 帧, 共 %2 字节")
                      .arg(m_parsedFrames.size())
                      .arg(m_rawData.size()));

        ui->btnExport->setEnabled(!m_parsedFrames.isEmpty());

        if (m_parsedFrames.isEmpty()) {
            QMessageBox::information(this, "解析结果", "未找到有效帧");
        } else {
            showResultDialog(m_parsedFrames);
        }
    }
}

void Widget::setParsingUi(bool parsing) {
    ui->progressBar->setVisible(parsing);
    ui->progressBar->setValue(0);
    ui->btnParse->setEnabled(!parsing);
    ui->btnExport->setEnabled(!parsing && !m_parsedFrames.isEmpty());
    ui->btnLoadConfig->setEnabled(!parsing);
    ui->btnBrowseConfig->setEnabled(!parsing);
    ui->btnBrowseDataFile->setEnabled(!parsing);
}

void Widget::onExport() {
    if (m_parsedFrames.isEmpty()) {
        QMessageBox::warning(this, "提示", "没有可导出的数据,请先解析");
        return;
    }

    QString filePath =
        QFileDialog::getSaveFileName(this, "导出结果", QString(), "文本文件 (*.txt)");
    if (filePath.isEmpty())
        return;

    QString errorMsg;
    if (DataExporter::exportToTxt(filePath, m_parsedFrames, m_config, &errorMsg)) {
        setStatus("导出成功: " + filePath);
        QMessageBox::information(
            this, "导出成功",
            QString("已导出 %1 帧到:\n%2").arg(m_parsedFrames.size()).arg(filePath));
    } else {
        QMessageBox::critical(this, "导出失败", errorMsg);
    }
}

void Widget::onSerialDataReceived(const QByteArray &data) {
    m_rawData.append(data);
    updateParseButton();
    setStatus(QString("串口接收中... 已缓存 %1 字节").arg(m_rawData.size()));
}

void Widget::onSerialError(const QString &msg) {
    QMessageBox::warning(this, "串口错误", msg);
}

void Widget::updateParseButton() {
    bool hasData = !m_rawData.isEmpty() || m_serialManager->isOpen() ||
                   (ui->radioFile->isChecked() && !ui->editDataFilePath->text().isEmpty());
    ui->btnParse->setEnabled(m_configLoaded && hasData);
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

void Widget::showResultDialog(const QVector<ParsedFrame> &frames) {
    // Collect DATA field names in config order
    QStringList dataFieldNames;
    for (const auto &field : m_config.fields) {
        if (field.fieldType == FieldType::DATA)
            dataFieldNames << field.name;
    }

    // Build example text from first frame only
    const auto &frame = frames.first();
    QString detail;
    for (const auto &name : dataFieldNames) {
        for (const auto &pf : frame.fields) {
            if (pf.name == name && pf.fieldType == FieldType::DATA) {
                detail += QString("  %1 = %2\n").arg(name, formatFieldValue(pf));
                break;
            }
        }
    }

    QString msg = QString("解析完成，共 %1 帧\n\n"
                          "--- 第 1 帧示例 ---\n%2\n"
                          "请使用「导出」按钮将全部结果保存为 TXT 文件。")
                      .arg(frames.size())
                      .arg(detail);

    QMessageBox::information(this, "解析结果", msg);
}

void Widget::setStatus(const QString &msg) { ui->labelStatus->setText(msg); }
