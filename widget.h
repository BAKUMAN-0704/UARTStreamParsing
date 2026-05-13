#ifndef WIDGET_H
#define WIDGET_H

#include "src/config/FrameFieldDef.h"
#include "src/datasource/SerialPortManager.h"
#include "src/parser/StreamParser.h"
#include "src/codec/HexTextDecoder.h"
#include "src/workflow/FileParseWorkflow.h"
#include "src/workflow/ParseExportControlWorkflow.h"
#include <QMap>
#include <QThread>
#include <QWidget>

class ParseWorker;

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
    void onRemoveConfig();
    void onConfigSelectionChanged();
    void onEndFrameToggled(bool checked);
    void onBrowseAutoSaveDir();
    void onSourceChanged();
    void onRefreshPorts();
    void onOpenPort();
    void onClosePort();
    void onBrowseDataFile();
    void onParse();
    void onExport();
    void onSerialDataReceived(const QByteArray &data);
    void onSerialError(const QString &msg);
    void onEndFrameDetected();
    void onAutoSaveCompleted(const QStringList &savedFiles);

private:
    void initSerialUI();
    void initStyle();
    void updateParseButton();
    void applyParseExportControls(bool parsing = false);
    void showResultDialog(const QMap<QString, QVector<ParsedFrame>> &framesByConfig);
    void setStatus(const QString &msg);
    void setParsingUi(bool parsing);
    static QString formatFieldValue(const ParsedField &pf);

    Ui::Widget *ui;
    StreamParser *m_streamParser;
    SerialPortManager *m_serialManager;

    QByteArray m_rawData;
    QMap<QString, QVector<ParsedFrame>> m_parsedFramesByConfig;
    bool m_streamingActive = false;

    HexTextDecoder m_hexDecoder;
    FileParseWorkflow m_fileParseWorkflow;
    ParseExportControlWorkflow m_parseExportControlWorkflow;

    // Worker thread for file parsing
    QThread *m_workerThread = nullptr;
    ParseWorker *m_worker = nullptr;
};

#endif // WIDGET_H
