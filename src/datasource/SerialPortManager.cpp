#include "SerialPortManager.h"

SerialPortManager::SerialPortManager(QObject *parent) : QObject(parent) {
    m_serial = new QSerialPort(this);
    connect(m_serial, &QSerialPort::readyRead, this,
            &SerialPortManager::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this,
            &SerialPortManager::onError);
}

SerialPortManager::~SerialPortManager() { closePort(); }

bool SerialPortManager::openPort(const SerialConfig &config) {
    if (m_serial->isOpen())
        closePort();

    m_serial->setPortName(config.portName);
    m_serial->setBaudRate(config.baudRate);
    m_serial->setDataBits(config.dataBits);
    m_serial->setStopBits(config.stopBits);
    m_serial->setParity(config.parity);

    if (!m_serial->open(QIODevice::ReadOnly)) {
        Q_EMIT errorOccurred(
            QString("无法打开串口 %1: %2").arg(config.portName, m_serial->errorString()));
        return false;
    }

    m_buffer.clear();
    Q_EMIT portOpened();
    return true;
}

void SerialPortManager::closePort() {
    if (m_serial->isOpen()) {
        m_serial->close();
        Q_EMIT portClosed();
    }
}

bool SerialPortManager::isOpen() const { return m_serial->isOpen(); }

QStringList SerialPortManager::availablePorts() {
    QStringList ports;
    for (const auto &info : QSerialPortInfo::availablePorts())
        ports.append(info.portName());
    return ports;
}

QByteArray SerialPortManager::takeBuffer() {
    QByteArray data = m_buffer;
    m_buffer.clear();
    return data;
}

void SerialPortManager::onReadyRead() {
    QByteArray data = m_serial->readAll();
    m_buffer.append(data);
    Q_EMIT dataReceived(data);
}

void SerialPortManager::onError(QSerialPort::SerialPortError error) {
    if (error != QSerialPort::NoError) {
        Q_EMIT errorOccurred(
            QString("串口错误: %1").arg(m_serial->errorString()));
    }
}
