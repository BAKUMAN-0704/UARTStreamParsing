#ifndef SERIALPORTMANAGER_H
#define SERIALPORTMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>

struct SerialConfig {
    QString portName;
    qint32 baudRate = 115200;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QSerialPort::Parity parity = QSerialPort::NoParity;
};

class SerialPortManager : public QObject {
    Q_OBJECT
public:
    explicit SerialPortManager(QObject *parent = nullptr);
    ~SerialPortManager() override;

    bool openPort(const SerialConfig &config);
    void closePort();
    bool isOpen() const;

    // Get list of available serial ports
    static QStringList availablePorts();

    // Get all received data and clear the buffer
    QByteArray takeBuffer();

Q_SIGNALS:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &msg);
    void portOpened();
    void portClosed();

private Q_SLOTS:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError error);

private:
    QSerialPort *m_serial = nullptr;
    QByteArray m_buffer;
};

#endif // SERIALPORTMANAGER_H
