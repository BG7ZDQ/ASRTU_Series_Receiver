#pragma once

#include <QByteArray>
#include <QMainWindow>
#include <QQueue>
#include <QString>
#include <QTimer>

class QNetworkAccessManager;
class QPlainTextEdit;

namespace asrtu
{

struct SatnogsUploaderConfig {
    QString zmqAddress = QStringLiteral("tcp://127.0.0.1:5555");
    QString apiAddress = QStringLiteral("https://db.satnogs.org/api/telemetry/");
    QString satellite;
    QString source;
    int noradId = 0;
    double longitude = 0.0;
    double latitude = 0.0;
    double altitude = 0.0;
};

bool validateSatnogsUploaderConfig(const SatnogsUploaderConfig& config,
                                   QString* error);
QString formatTelemetryFrame(const QByteArray& frame);
bool isRetriableSatnogsUpload(int networkError, int httpStatus);
int satnogsRetryDelayMs(int attempt, int retryAfterSeconds = -1);

class SatnogsUploaderWindow final : public QMainWindow
{
public:
    explicit SatnogsUploaderWindow(SatnogsUploaderConfig config,
                                   QWidget* parent = nullptr);
    ~SatnogsUploaderWindow() override;

    bool start(QString* error);
    void showPreviewFrame(const QByteArray& frame);

private:
    struct PendingFrame {
        QByteArray bytes;
        QString timestamp;
        int attempts = 0;
    };

    void buildUi();
    void receiveFrames();
    void enqueueFrame(const QByteArray& frame);
    void submitPending();
    void submitFrame(PendingFrame frame);
    void appendFeedback(const QString& message);
    QString configurationText() const;

    SatnogsUploaderConfig config_;
    void* zmqContext_ = nullptr;
    void* zmqSocket_ = nullptr;
    QTimer receiveTimer_;
    QTimer retryTimer_;
    QNetworkAccessManager* network_ = nullptr;
    QPlainTextEdit* frameView_ = nullptr;
    QPlainTextEdit* stationView_ = nullptr;
    QPlainTextEdit* feedbackView_ = nullptr;
    QQueue<PendingFrame> pendingFrames_;
    int inFlight_ = 0;
    quint64 receivedFrames_ = 0;
};

} // namespace asrtu
