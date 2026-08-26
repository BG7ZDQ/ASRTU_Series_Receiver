#pragma once

#include <QDialog>
#include <QVector>

#include "sgp4.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QNetworkAccessManager;
class QPlainTextEdit;
class QPushButton;
class QTimer;

class SatelliteTrackerDialog final : public QDialog
{
public:
    SatelliteTrackerDialog(double longitudeDeg, double latitudeDeg,
                           double altitudeMeters, const QString& preferredSatellite,
                           QWidget* parent = nullptr);
    ~SatelliteTrackerDialog() override;

private:
    struct Satellite {
        QString name;
        sgp4_tle_t tle{};
        sgp4_state_t state{};
    };

    void buildUi();
    void loadSettings();
    void saveSettings();
    void updateTle();
    void downloadNextSource();
    void finishDownloads();
    bool installTle(const QByteArray& data, const QString& source);
    void refreshFrequencyPresets(bool chooseDefault);
    void updateTracking();
    void selectPreferredSatellite();
    void publishDoppler(qint64 targetHz, qint64 correctionHz, bool valid);

    double longitudeDeg_;
    double latitudeDeg_;
    double altitudeMeters_;
    QString preferredSatellite_;
    QVector<Satellite> satellites_;
    QStringList downloadSources_;
    QByteArray downloadedTle_;
    int downloadIndex_ = 0;
    int successfulDownloads_ = 0;

    QComboBox* satellite_ = nullptr;
    QComboBox* frequencyPreset_ = nullptr;
    QDoubleSpinBox* frequencyMHz_ = nullptr;
    QPlainTextEdit* sources_ = nullptr;
    QLabel* received_ = nullptr;
    QLabel* azimuth_ = nullptr;
    QLabel* elevation_ = nullptr;
    QLabel* range_ = nullptr;
    QLabel* correction_ = nullptr;
    QLabel* downlink_ = nullptr;
    QLabel* tleEpoch_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* updateButton_ = nullptr;
    QNetworkAccessManager* network_ = nullptr;
    QTimer* timer_ = nullptr;

#ifdef Q_OS_WIN
    void* mappingHandle_ = nullptr;
    unsigned char* mappingView_ = nullptr;
#endif
};
