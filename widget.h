#ifndef WIDGET_H
#define WIDGET_H

#include "src/config/FrameFieldDef.h"
#include "src/datasource/SerialPortManager.h"
#include "src/parser/FrameParser.h"
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget {
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private Q_SLOTS:
    void onBrowseConfig();
    void onLoadConfig();
    void onSourceChanged();
    void onRefreshPorts();
    void onOpenPort();
    void onClosePort();
    void onBrowseDataFile();
    void onReadFile();
    void onParse();
    void onClearResults();
    void onExport();
    void onSerialDataReceived(const QByteArray &data);
    void onSerialError(const QString &msg);

private:
    void initSerialUI();
    void updateParseButton();
    void displayResults(const QVector<ParsedFrame> &frames);
    void setStatus(const QString &msg);

    Ui::Widget *ui;
    FrameConfig m_config;
    bool m_configLoaded = false;
    QByteArray m_rawData;
    QVector<ParsedFrame> m_parsedFrames;
    FrameParser m_parser;
    SerialPortManager *m_serialManager;
};

#endif // WIDGET_H
