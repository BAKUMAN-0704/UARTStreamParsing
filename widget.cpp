#include "widget.h"
#if __has_include("ui_widget.h")
#include "ui_widget.h"
#elif __has_include("build/UARTStreamParsing_autogen/include/ui_widget.h")
#include "build/UARTStreamParsing_autogen/include/ui_widget.h"
#else
#error "ui_widget.h not found. Run CMake configure/build to generate it."
#endif

#include "src/config/ConfigParser.h"
#include "src/datasource/FileDataSource.h"
#include "src/export/DataExporter.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSerialPort>

Widget::Widget(QWidget *parent) : QWidget(parent), ui(new Ui::Widget) {
    ui->setupUi(this);

    m_serialManager = new SerialPortManager(this);

    // Config buttons
    connect(ui->btnBrowseConfig, &QPushButton::clicked, this, &Widget::onBrowseConfig);
    connect(ui->btnLoadConfig, &QPushButton::clicked, this, &Widget::onLoadConfig);

    // Source selection
    connect(ui->radioSerial, &QRadioButton::toggled, this, &Widget::onSourceChanged);
    connect(ui->radioFile, &QRadioButton::toggled, this, &Widget::onSourceChanged);

    // Serial
    connect(ui->btnRefreshPorts, &QPushButton::clicked, this, &Widget::onRefreshPorts);
    connect(ui->btnOpenPort, &QPushButton::clicked, this, &Widget::onOpenPort);
    connect(ui->btnClosePort, &QPushButton::clicked, this, &Widget::onClosePort);
    connect(m_serialManager, &SerialPortManager::dataReceived, this, &Widget::onSerialDataReceived);
    connect(m_serialManager, &SerialPortManager::errorOccurred, this, &Widget::onSerialError);

    // File
    connect(ui->btnBrowseDataFile, &QPushButton::clicked, this, &Widget::onBrowseDataFile);
    connect(ui->btnReadFile, &QPushButton::clicked, this, &Widget::onReadFile);

    // Actions
    connect(ui->btnParse, &QPushButton::clicked, this, &Widget::onParse);
    connect(ui->btnClearResults, &QPushButton::clicked, this, &Widget::onClearResults);
    connect(ui->btnExport, &QPushButton::clicked, this, &Widget::onExport);

    initSerialUI();
    onSourceChanged();
}

Widget::~Widget() {
    delete ui;
}

void Widget::initSerialUI() {
    // Baud rates
    ui->comboBaud->addItems(
        {"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"});
    ui->comboBaud->setCurrentText("115200");

    // Data bits
    ui->comboDataBits->addItems({"5", "6", "7", "8"});
    ui->comboDataBits->setCurrentText("8");

    // Stop bits
    ui->comboStopBits->addItems({"1", "1.5", "2"});
    ui->comboStopBits->setCurrentText("1");

    // Parity
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
        ui->stackedSource->setCurrentIndex(0);
    else
        ui->stackedSource->setCurrentIndex(1);
}

void Widget::onRefreshPorts() {
    ui->comboPort->clear();
    ui->comboPort->addItems(SerialPortManager::availablePorts());
}

void Widget::onOpenPort() {
    SerialConfig config;
    config.portName = ui->comboPort->currentText();
    config.baudRate = ui->comboBaud->currentText().toInt();

    int dataBits = ui->comboDataBits->currentText().toInt();
    config.dataBits = static_cast<QSerialPort::DataBits>(dataBits);

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
        ui->labelSerialStatus->setText("状态: 已连接 - " + config.portName);
        m_rawData.clear();
        updateParseButton();
        setStatus("串口已打开: " + config.portName);
    }
}

void Widget::onClosePort() {
    m_serialManager->closePort();
    ui->btnOpenPort->setEnabled(true);
    ui->btnClosePort->setEnabled(false);
    ui->labelSerialStatus->setText("状态: 未连接");
    setStatus("串口已关闭");
}

void Widget::onBrowseDataFile() {
    QString filePath = QFileDialog::getOpenFileName(this, "选择数据文件", QString(),
                                                    "文本文件 (*.txt);;所有文件 (*.*)");
    if (!filePath.isEmpty())
        ui->editDataFilePath->setText(filePath);
}

void Widget::onReadFile() {
    QString path = ui->editDataFilePath->text();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择数据文件");
        return;
    }

    QString errorMsg;
    m_rawData = FileDataSource::readHexFile(path, &errorMsg);
    if (!errorMsg.isEmpty()) {
        QMessageBox::critical(this, "读取失败", errorMsg);
        return;
    }

    updateParseButton();
    setStatus(QString("已读取 %1 字节数据").arg(m_rawData.size()));
}

void Widget::onParse() {
    if (!m_configLoaded) {
        QMessageBox::warning(this, "提示", "请先加载配置文件");
        return;
    }
    if (m_rawData.isEmpty()) {
        // If serial is open, take its buffer
        if (m_serialManager->isOpen()) {
            m_rawData = m_serialManager->takeBuffer();
        }
        if (m_rawData.isEmpty()) {
            QMessageBox::warning(this, "提示", "没有可解析的数据");
            return;
        }
    }

    m_parsedFrames = m_parser.parse(m_rawData);
    displayResults(m_parsedFrames);
    ui->btnExport->setEnabled(!m_parsedFrames.isEmpty());
    setStatus(
        QString("解析完成: %1 帧, 共 %2 字节").arg(m_parsedFrames.size()).arg(m_rawData.size()));
}

void Widget::onClearResults() {
    ui->tableResults->clear();
    ui->tableResults->setRowCount(0);
    ui->tableResults->setColumnCount(0);
    m_parsedFrames.clear();
    m_rawData.clear();
    ui->btnExport->setEnabled(false);
    setStatus("结果已清空");
}

void Widget::onExport() {
    if (m_parsedFrames.isEmpty()) {
        QMessageBox::warning(this, "提示", "没有可导出的数据");
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
            this, "成功", QString("已导出 %1 帧到:\n%2").arg(m_parsedFrames.size()).arg(filePath));
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
    bool hasData = !m_rawData.isEmpty() || (m_serialManager->isOpen());
    ui->btnParse->setEnabled(m_configLoaded && hasData);
}

void Widget::displayResults(const QVector<ParsedFrame> &frames) {
    ui->tableResults->clear();

    if (frames.isEmpty()) {
        ui->tableResults->setRowCount(0);
        ui->tableResults->setColumnCount(0);
        return;
    }

    // Build column list: fixed columns + DATA field columns
    QStringList headers;
    headers << "帧序号" << "帧偏移" << "CRC";

    QVector<int> dataFieldIndices; // indices into FrameConfig::fields for DATA fields
    for (int i = 0; i < m_config.fields.size(); ++i) {
        if (m_config.fields[i].fieldType == FieldType::DATA) {
            headers << m_config.fields[i].name;
            dataFieldIndices.append(i);
        }
    }

    ui->tableResults->setColumnCount(headers.size());
    ui->tableResults->setHorizontalHeaderLabels(headers);
    ui->tableResults->setRowCount(frames.size());

    for (int row = 0; row < frames.size(); ++row) {
        const auto &frame = frames[row];

        ui->tableResults->setItem(row, 0, new QTableWidgetItem(QString::number(frame.frameIndex)));
        ui->tableResults->setItem(row, 1,
                                  new QTableWidgetItem(QString::number(frame.offsetInStream)));
        ui->tableResults->setItem(row, 2, new QTableWidgetItem(frame.crcValid ? "OK" : "FAIL"));

        // Fill DATA fields
        int col = 3;
        for (int fi : dataFieldIndices) {
            QString valueStr;
            // Find corresponding parsed field
            for (const auto &pf : frame.fields) {
                if (pf.name == m_config.fields[fi].name && pf.fieldType == FieldType::DATA) {
                    switch (pf.dataType) {
                    case DataType::FLOAT:
                    case DataType::DOUBLE:
                        valueStr = QString::number(pf.value.toDouble(), 'f', 6);
                        break;
                    case DataType::UINT8:
                    case DataType::UINT16:
                    case DataType::UINT32:
                        valueStr = QString::number(pf.value.toUInt());
                        break;
                    case DataType::INT8:
                    case DataType::INT16:
                    case DataType::INT32:
                        valueStr = QString::number(pf.value.toInt());
                        break;
                    default:
                        valueStr = pf.value.toString();
                        break;
                    }
                    break;
                }
            }
            ui->tableResults->setItem(row, col, new QTableWidgetItem(valueStr));
            ++col;
        }
    }

    ui->tableResults->resizeColumnsToContents();
}

void Widget::setStatus(const QString &msg) {
    ui->labelStatus->setText(msg);
}
