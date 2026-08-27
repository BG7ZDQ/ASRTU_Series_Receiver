#include "satellite_tracker_dialog.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QTextOption>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRotationRadSec = 7.2921150e-5;
constexpr double kLightKmSec = 299792.458;
constexpr quint32 kDopplerMagic = 0x504f4441U;

QString normalizedSatellite(QString value)
{
    value = value.toUpper();
    value.remove(QRegularExpression(QStringLiteral("[^A-Z0-9]")));
    value.remove(QStringLiteral("UV"));
    return value;
}

QString cachePath()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(root);
    return QDir(root).filePath(QStringLiteral("all_sources.tle"));
}
}

SatelliteTrackerDialog::SatelliteTrackerDialog(
    double longitudeDeg, double latitudeDeg, double altitudeMeters,
    const QString& preferredSatellite, QWidget* parent)
    : QDialog(parent), longitudeDeg_(longitudeDeg), latitudeDeg_(latitudeDeg),
      altitudeMeters_(altitudeMeters), preferredSatellite_(preferredSatellite)
{
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                   Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint |
                   Qt::WindowCloseButtonHint);
    setWindowTitle(QCoreApplication::translate("ASRTU", "阿斯图系列卫星跟踪与多普勒"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/win98_doppler.png")));
    setAttribute(Qt::WA_DeleteOnClose, true);
    buildUi();
    loadSettings();
    adjustSize();
    const QSize comfortable(460, 620);
    setMinimumSize(comfortable);
    resize(comfortable);

#ifdef Q_OS_WIN
    mappingHandle_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                        0, 64, L"Local\\ASRTU_DOPPLER_CONTROL_V1");
    if (mappingHandle_)
        mappingView_ = static_cast<unsigned char*>(
            MapViewOfFile(mappingHandle_, FILE_MAP_ALL_ACCESS, 0, 0, 64));
#endif

    network_ = new QNetworkAccessManager(this);
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, [this] { updateTracking(); });
    timer_->start(500);

    QFile cache(cachePath());
    if (cache.open(QIODevice::ReadOnly) && installTle(cache.readAll(), QCoreApplication::translate("ASRTU", "本地缓存")))
        status_->setText(QCoreApplication::translate("ASRTU", "已载入本地 TLE 缓存，正在在线更新…"));
    QTimer::singleShot(0, this, [this] { updateTle(); });
}

SatelliteTrackerDialog::~SatelliteTrackerDialog()
{
    saveSettings();
    publishDoppler(0, 0, false);
#ifdef Q_OS_WIN
    if (mappingView_)
        UnmapViewOfFile(mappingView_);
    if (mappingHandle_)
        CloseHandle(mappingHandle_);
#endif
}

void SatelliteTrackerDialog::buildUi()
{
    setStyleSheet(QStringLiteral(
        "QDialog { background:#f5f8fc; color:#17202a; font-size:10pt; }"
        "QGroupBox { background:white; border:1px solid #dce5ef; border-radius:8px; "
        "margin-top:10px; padding-top:10px; font-weight:600; }"
        "QComboBox,QDoubleSpinBox,QPlainTextEdit { background:white; border:1px solid #cbd5e1; "
        "border-radius:5px; padding:5px; }"
        "QComboBox::drop-down { border:0; width:25px; }"
        "QComboBox::down-arrow { image:url(:/launcher/combo_arrow.png); "
        "width:16px; height:16px; }"
        "QPushButton { min-height:32px; border:1px solid #a9c9ef; border-radius:6px; "
        "background:#edf5ff; color:#145ca8; padding:0 14px; }"
        "QLabel#value { color:#075db3; font-weight:600; }"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    auto* receivedBox = new QGroupBox(QCoreApplication::translate("ASRTU", "实时跟踪数据"), this);
    auto* receivedLayout = new QVBoxLayout(receivedBox);
    received_ = new QLabel(QCoreApplication::translate("ASRTU", "等待 TLE 数据"), receivedBox);
    received_->setWordWrap(false);
    received_->setAlignment(Qt::AlignCenter);
    received_->setObjectName(QStringLiteral("value"));
    receivedLayout->addWidget(received_);
    auto* values = new QGridLayout;
    auto addValue = [values, receivedBox](int row, int column, const QString& name,
                                          QLabel*& output) {
        values->addWidget(new QLabel(name, receivedBox), row, column * 2);
        output = new QLabel(QStringLiteral("--"), receivedBox);
        output->setObjectName(QStringLiteral("value"));
        values->addWidget(output, row, column * 2 + 1);
    };
    addValue(0, 0, QCoreApplication::translate("ASRTU", "方位角"), azimuth_);
    addValue(0, 1, QCoreApplication::translate("ASRTU", "仰角"), elevation_);
    addValue(1, 0, QCoreApplication::translate("ASRTU", "距离"), range_);
    addValue(1, 1, QCoreApplication::translate("ASRTU", "多普勒"), correction_);
    addValue(2, 0, QCoreApplication::translate("ASRTU", "接收频率"), downlink_);
    addValue(2, 1, QCoreApplication::translate("ASRTU", "TLE 历元"), tleEpoch_);
    receivedLayout->addLayout(values);
    root->addWidget(receivedBox);

    auto* selectionBox = new QGroupBox(QCoreApplication::translate("ASRTU", "卫星与频率"), this);
    auto* selection = new QFormLayout(selectionBox);
    satellite_ = new QComboBox(selectionBox);
    frequencyPreset_ = new QComboBox(selectionBox);
    frequencyMHz_ = new QDoubleSpinBox(selectionBox);
    frequencyMHz_->setRange(1.0, 10000.0);
    frequencyMHz_->setDecimals(6);
    frequencyMHz_->setSingleStep(0.001);
    frequencyMHz_->setSuffix(QStringLiteral(" MHz"));
    frequencyMHz_->setValue(437.430034);
    selection->addRow(QCoreApplication::translate("ASRTU", "卫星"), satellite_);
    selection->addRow(QCoreApplication::translate("ASRTU", "频率预设"), frequencyPreset_);
    selection->addRow(QCoreApplication::translate("ASRTU", "标称下行"), frequencyMHz_);
    root->addWidget(selectionBox);

    auto* sourceBox = new QGroupBox(QCoreApplication::translate("ASRTU", "TLE 来源（每行一个，全部下载并合并）"), this);
    auto* sourceLayout = new QVBoxLayout(sourceBox);
    sources_ = new QPlainTextEdit(sourceBox);
    sources_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    sources_->setWordWrapMode(QTextOption::WrapAnywhere);
    sources_->setMaximumHeight(120);
    sourceLayout->addWidget(sources_);
    auto* sourceActions = new QHBoxLayout;
    status_ = new QLabel(QCoreApplication::translate("ASRTU", "等待更新"), sourceBox);
    status_->setWordWrap(false);
    updateButton_ = new QPushButton(QCoreApplication::translate("ASRTU", "立即更新 TLE"), sourceBox);
    auto* openTleButton = new QPushButton(QCoreApplication::translate("ASRTU", "打开星历目录"), sourceBox);
    sourceActions->addWidget(status_, 1);
    sourceActions->addWidget(openTleButton);
    sourceActions->addWidget(updateButton_);
    sourceLayout->addLayout(sourceActions);
    root->addWidget(sourceBox);

    auto* station = new QLabel(
        QCoreApplication::translate("ASRTU", "地面站：%1°, %2°，%3 m")
            .arg(longitudeDeg_, 0, 'f', 5)
            .arg(latitudeDeg_, 0, 'f', 5)
            .arg(altitudeMeters_, 0, 'f', 1), this);
    station->setStyleSheet(QStringLiteral("color:#667788;"));
    root->addWidget(station);

    connect(updateButton_, &QPushButton::clicked, this, [this] { updateTle(); });
    connect(openTleButton, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(QFileInfo(cachePath()).absolutePath()));
    });
    connect(satellite_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this] {
                refreshFrequencyPresets(true);
                updateTracking();
            });
    connect(frequencyPreset_, qOverload<int>(&QComboBox::activated),
            this, [this](int index) {
                const QVariant value = frequencyPreset_->itemData(index);
                if (value.isValid())
                    frequencyMHz_->setValue(value.toDouble());
            });
    connect(frequencyMHz_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this] { updateTracking(); });
}

void SatelliteTrackerDialog::loadSettings()
{
    QSettings settings(QStringLiteral("ASRTU"), QStringLiteral("AstroSeriesTracker"));
    const QStringList defaultSources{
        QStringLiteral("https://8104.satellites.ac.cn/latest.tle"),
        QStringLiteral("https://celestrak.org/NORAD/elements/gp.php?CATNR=61781&FORMAT=TLE")
    };
    QStringList configuredSources =
        settings.value(QStringLiteral("tle_sources"), defaultSources).toStringList();
    for (const QString& defaultSource : defaultSources) {
        if (!configuredSources.contains(defaultSource))
            configuredSources.append(defaultSource);
    }
    sources_->setPlainText(configuredSources.join(QLatin1Char('\n')));
    frequencyMHz_->setValue(settings.value(QStringLiteral("frequency_mhz"), 437.430034).toDouble());
    preferredSatellite_ = settings.value(QStringLiteral("satellite"), preferredSatellite_).toString();
}

void SatelliteTrackerDialog::saveSettings()
{
    QSettings settings(QStringLiteral("ASRTU"), QStringLiteral("AstroSeriesTracker"));
    settings.setValue(QStringLiteral("tle_sources"),
                      sources_->toPlainText().split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                                   Qt::SkipEmptyParts));
    settings.setValue(QStringLiteral("frequency_mhz"), frequencyMHz_->value());
    settings.setValue(QStringLiteral("satellite"), satellite_->currentText());
}

void SatelliteTrackerDialog::updateTle()
{
    saveSettings();
    downloadSources_ = sources_->toPlainText().split(
        QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
    for (QString& source : downloadSources_)
        source = source.trimmed();
    downloadIndex_ = 0;
    downloadedTle_.clear();
    successfulDownloads_ = 0;
    updateButton_->setEnabled(false);
    downloadNextSource();
}

void SatelliteTrackerDialog::downloadNextSource()
{
    if (downloadIndex_ >= downloadSources_.size()) {
        finishDownloads();
        return;
    }
    const QString source = downloadSources_.at(downloadIndex_++);
    status_->setText(
        QCoreApplication::translate("ASRTU", "正在下载第 %1/%2 个 TLE 来源")
            .arg(downloadIndex_)
            .arg(downloadSources_.size()));
    QNetworkRequest request{QUrl(source)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setRawHeader("User-Agent", "Astro-Series-Satellite-Tracker/1.0");
    request.setTransferTimeout(15000);
    QNetworkReply* reply = network_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, source] {
        const QByteArray data = reply->readAll();
        const QByteArray trimmed = data.trimmed();
        const bool hasLine1 = trimmed.startsWith("1 ") || trimmed.contains("\n1 ");
        const bool hasLine2 = trimmed.startsWith("2 ") || trimmed.contains("\n2 ");
        const bool success = reply->error() == QNetworkReply::NoError &&
                             hasLine1 && hasLine2;
        reply->deleteLater();
        if (success) {
            if (!downloadedTle_.isEmpty() && !downloadedTle_.endsWith('\n'))
                downloadedTle_.append('\n');
            downloadedTle_.append(data);
            downloadedTle_.append('\n');
            ++successfulDownloads_;
        }
        downloadNextSource();
    });
}

void SatelliteTrackerDialog::finishDownloads()
{
    updateButton_->setEnabled(true);
    if (successfulDownloads_ == 0 || !installTle(downloadedTle_, QCoreApplication::translate("ASRTU", "在线合并"))) {
        status_->setText(satellites_.isEmpty()
                             ? QCoreApplication::translate("ASRTU", "所有 TLE 来源均下载失败")
                             : QCoreApplication::translate("ASRTU", "在线更新失败，继续使用本地 TLE 数据"));
        return;
    }
    QFile cache(cachePath());
    if (cache.open(QIODevice::WriteOnly | QIODevice::Truncate))
        cache.write(downloadedTle_);
    status_->setText(QCoreApplication::translate("ASRTU", "TLE 更新完成：%1/%2 个来源")
                         .arg(successfulDownloads_)
                         .arg(downloadSources_.size()));
}

bool SatelliteTrackerDialog::installTle(const QByteArray& data, const QString& source)
{
    Q_UNUSED(source)
    const QList<QByteArray> rawLines = data.split('\n');
    QVector<QByteArray> lines;
    for (QByteArray line : rawLines) {
        line = line.trimmed();
        if (!line.isEmpty())
            lines.push_back(line);
    }
    QVector<Satellite> parsed;
    QSet<QString> catalogNumbers;
    for (int i = 0; i + 1 < lines.size(); ++i) {
        QByteArray name;
        QByteArray line1;
        QByteArray line2;
        if (lines[i].startsWith("1 ") && lines[i + 1].startsWith("2 ")) {
            line1 = lines[i];
            line2 = lines[i + 1];
            name = QByteArray("NORAD ") + line1.mid(2, 5);
            i += 1;
        } else if (i + 2 < lines.size() && lines[i + 1].startsWith("1 ") &&
                   lines[i + 2].startsWith("2 ")) {
            name = lines[i];
            line1 = lines[i + 1];
            line2 = lines[i + 2];
            i += 2;
        } else {
            continue;
        }
        Satellite sat;
        const QByteArray clippedName = name.left(SGP4_TLE_NAME_LEN - 1);
        const sgp4_error_t parseResult = sgp4_parse_tle_3line(
            clippedName.constData(), line1.constData(), line2.constData(), &sat.tle);
        sgp4_elements_t elements{};
        if (parseResult != SGP4_SUCCESS ||
            sgp4_tle_to_elements(&sat.tle, &elements) != SGP4_SUCCESS ||
            sgp4_init(&sat.state, &elements) != SGP4_SUCCESS)
            continue;
        const QString catalogNumber = QString::fromLatin1(line1.mid(2, 5)).trimmed();
        if (catalogNumbers.contains(catalogNumber))
            continue;
        catalogNumbers.insert(catalogNumber);
        sat.name = QString::fromUtf8(name);
        parsed.push_back(sat);
    }
    if (parsed.isEmpty())
        return false;
    const QString previous = satellite_->currentText();
    satellites_ = parsed;
    satellite_->blockSignals(true);
    satellite_->clear();
    for (const Satellite& sat : satellites_)
        satellite_->addItem(sat.name);
    satellite_->blockSignals(false);
    preferredSatellite_ = previous.isEmpty() ? preferredSatellite_ : previous;
    selectPreferredSatellite();
    refreshFrequencyPresets(true);
    updateTracking();
    return true;
}

void SatelliteTrackerDialog::refreshFrequencyPresets(bool chooseDefault)
{
    const QString name = normalizedSatellite(satellite_->currentText());
    const double current = frequencyMHz_->value();
    frequencyPreset_->blockSignals(true);
    frequencyPreset_->clear();
    if (name.contains(QStringLiteral("ASRTU1")) ||
        name.contains(QStringLiteral("AO123"))) {
        frequencyPreset_->addItem(QStringLiteral("AO-123 435.400 MHz"), 435.400);
        frequencyPreset_->addItem(QStringLiteral("AO-123 436.210 MHz"), 436.210);
    } else if (name.contains(QStringLiteral("BY04")) ||
               name.contains(QStringLiteral("8104"))) {
        frequencyPreset_->addItem(QStringLiteral("BY04 437.443 MHz"), 437.443);
    } else if (name.contains(QStringLiteral("JAMX"))) {
        frequencyPreset_->addItem(QStringLiteral("JAMX 435.500 MHz"), 435.500);
        frequencyPreset_->addItem(QStringLiteral("JAMX 435.075 MHz"), 435.075);
    }
    frequencyPreset_->addItem(QCoreApplication::translate("ASRTU", "自定义"));
    if (chooseDefault && frequencyPreset_->count() > 1) {
        frequencyPreset_->setCurrentIndex(0);
        frequencyMHz_->setValue(frequencyPreset_->itemData(0).toDouble());
    } else {
        frequencyPreset_->setCurrentIndex(frequencyPreset_->count() - 1);
        frequencyMHz_->setValue(current);
    }
    frequencyPreset_->blockSignals(false);
}

void SatelliteTrackerDialog::selectPreferredSatellite()
{
    const QString wanted = normalizedSatellite(preferredSatellite_);
    for (int i = 0; i < satellite_->count(); ++i) {
        const QString candidate = normalizedSatellite(satellite_->itemText(i));
        if (!wanted.isEmpty() && (candidate.contains(wanted) || wanted.contains(candidate))) {
            satellite_->setCurrentIndex(i);
            return;
        }
    }
    if (satellite_->count() > 0)
        satellite_->setCurrentIndex(0);
}

void SatelliteTrackerDialog::updateTracking()
{
    const int index = satellite_->currentIndex();
    if (index < 0 || index >= satellites_.size()) {
        publishDoppler(0, 0, false);
        return;
    }
    const Satellite& sat = satellites_.at(index);
    const double unixSeconds = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() / 1000.0;
    const double jd = sgp4_unix_to_jd(unixSeconds);
    sgp4_result_t result{};
    if (sgp4_propagate(&sat.state, (jd - sat.tle.epoch_jd) * 1440.0, &result) !=
        SGP4_SUCCESS) {
        status_->setText(QCoreApplication::translate("ASRTU", "SGP4 计算失败"));
        publishDoppler(0, 0, false);
        return;
    }
    const double gst = sgp4_gstime(jd);
    const sgp4_vec3_t positionEci{result.r[0], result.r[1], result.r[2]};
    const sgp4_vec3_t velocityEci{result.v[0], result.v[1], result.v[2]};
    sgp4_vec3_t positionEcef{};
    sgp4_vec3_t velocityEcef{};
    sgp4_eci_to_ecef(&positionEci, gst, &positionEcef);
    sgp4_eci_to_ecef(&velocityEci, gst, &velocityEcef);
    velocityEcef.x += kEarthRotationRadSec * positionEcef.y;
    velocityEcef.y -= kEarthRotationRadSec * positionEcef.x;

    const sgp4_geodetic_t observer{
        latitudeDeg_ * kPi / 180.0,
        longitudeDeg_ * kPi / 180.0,
        altitudeMeters_ / 1000.0
    };
    sgp4_vec3_t observerEcef{};
    sgp4_geodetic_to_ecef(&observer, &observerEcef);
    sgp4_look_angles_t look{};
    sgp4_look_angles(&positionEcef, &observer, &look);
    const sgp4_vec3_t delta{positionEcef.x - observerEcef.x,
                            positionEcef.y - observerEcef.y,
                            positionEcef.z - observerEcef.z};
    const double rangeRateKmSec =
        (delta.x * velocityEcef.x + delta.y * velocityEcef.y +
         delta.z * velocityEcef.z) / std::max(1e-9, look.range_km);
    const double nominalHz = frequencyMHz_->value() * 1e6;
    const qint64 correctionHz = qRound64(-nominalHz * rangeRateKmSec / kLightKmSec);
    const qint64 targetHz = qRound64(nominalHz) + correctionHz;

    received_->setText(QStringLiteral("SN %1   DN %2 Hz   RR %3 km/s")
                           .arg(sat.name).arg(targetHz)
                           .arg(rangeRateKmSec, 0, 'f', 4));
    azimuth_->setText(QStringLiteral("%1°").arg(sgp4_rad_to_deg(look.azimuth_rad), 0, 'f', 1));
    elevation_->setText(QStringLiteral("%1°").arg(sgp4_rad_to_deg(look.elevation_rad), 0, 'f', 1));
    range_->setText(QStringLiteral("%1 km").arg(look.range_km, 0, 'f', 1));
    correction_->setText(QStringLiteral("%1 Hz").arg(correctionHz));
    downlink_->setText(QStringLiteral("%1 MHz").arg(targetHz / 1e6, 0, 'f', 6));
    tleEpoch_->setText(QDateTime::fromMSecsSinceEpoch(
                          qRound64(sgp4_jd_to_unix(sat.tle.epoch_jd) * 1000.0), Qt::UTC)
                          .toString(QStringLiteral("MM-dd hh:mm")));
    publishDoppler(targetHz, correctionHz, true);
}

void SatelliteTrackerDialog::publishDoppler(qint64 targetHz, qint64 correctionHz,
                                             bool valid)
{
#ifdef Q_OS_WIN
    if (!mappingView_)
        return;
    std::memset(mappingView_, 0, 64);
    *reinterpret_cast<quint32*>(mappingView_ + 0) = kDopplerMagic;
    *reinterpret_cast<quint32*>(mappingView_ + 4) = 1;
    *reinterpret_cast<qint64*>(mappingView_ + 8) = targetHz;
    *reinterpret_cast<qint64*>(mappingView_ + 16) = correctionHz;
    *reinterpret_cast<qint64*>(mappingView_ + 24) = QDateTime::currentMSecsSinceEpoch();
    *reinterpret_cast<qint32*>(mappingView_ + 32) = valid ? 1 : 0;
    MemoryBarrier();
#else
    Q_UNUSED(targetHz)
    Q_UNUSED(correctionHz)
    Q_UNUSED(valid)
#endif
}
