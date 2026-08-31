#include "ssdv_image_window.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr int kMaxGalleryImages = 32;

QString kTitleStyle = QStringLiteral(
    "font-size:16px; font-weight:700; color:#17202a;");

QString kDetailStyle = QStringLiteral(
    "font-size:12px; color:#5b6b7d;");

QString kProgressTextStyle = QStringLiteral(
    "font-size:12px; color:#334155;");

QString kPathStyle = QStringLiteral(
    "font-size:11px; color:#7b8794;");

QString kBadgeBaseStyle = QStringLiteral(
    "font-weight:700; font-size:12px; color:#ffffff; padding:4px 12px; "
    "border-radius:11px;");

QString kPlaceholderStyle = QStringLiteral(
    "color:#8a97a6; font-size:14px;");

QString kButtonStyle = QStringLiteral(
    "QPushButton { min-height:30px; padding:0 14px; color:#334155; "
    "background:#ffffff; border:1px solid #cbd5e1; border-radius:6px; "
    "font-size:12px; }"
    "QPushButton:hover { background:#f1f5fb; border-color:#94a8c2; }"
    "QPushButton:pressed { background:#e6edf5; }"
    "QPushButton:disabled { color:#a7b3c2; background:#f5f7fa; "
    "border-color:#e2e8f0; }");

QString kPrimaryButtonStyle = QStringLiteral(
    "QPushButton { min-height:30px; padding:0 16px; color:#ffffff; "
    "background:#2b7de9; border:1px solid #2b7de9; border-radius:6px; "
    "font-size:12px; font-weight:600; }"
    "QPushButton:hover { background:#1f6fd6; border-color:#1f6fd6; }"
    "QPushButton:pressed { background:#1a5fba; }"
    "QPushButton:disabled { color:#c6d6ea; background:#9db9dd; "
    "border-color:#9db9dd; }");

QString kGalleryButtonStyle = QStringLiteral(
    "QPushButton { min-width:32px; min-height:30px; max-width:32px; "
    "color:#334155; background:#ffffff; border:1px solid #cbd5e1; "
    "border-radius:6px; font-size:12px; }"
    "QPushButton:hover { background:#f1f5fb; border-color:#94a8c2; }"
    "QPushButton:pressed { background:#e6edf5; }"
    "QPushButton:disabled { color:#a7b3c2; background:#f5f7fa; "
    "border-color:#e2e8f0; }");

QString kDangerButtonStyle = QStringLiteral(
    "QPushButton { min-height:30px; padding:0 14px; color:#ffffff; "
    "background:#d43d3d; border:1px solid #d43d3d; border-radius:6px; "
    "font-size:12px; font-weight:600; }"
    "QPushButton:hover { background:#c03333; border-color:#c03333; }"
    "QPushButton:pressed { background:#a52c2c; }"
    "QPushButton:disabled { color:#e8c6c6; background:#dd9a9a; "
    "border-color:#dd9a9a; }");

} // namespace

// Packet reception map. It intentionally does not pretend that the final
// packet count is known before EOI: blue passed CRC, yellow was retained for
// local recovery despite failing the standard CRC, coral is a confirmed hole,
// and the neutral track is still unknown.
class SsdvProgressBar final : public QWidget
{
public:
    explicit SsdvProgressBar(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(24);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setPackets(const std::vector<std::uint16_t>& received,
                    const std::vector<std::uint16_t>& crcFailed,
                    const std::vector<std::uint16_t>& missing,
                    int lastPacket,
                    bool complete)
    {
        received_ = received;
        crc_failed_ = crcFailed;
        missing_ = missing;
        last_packet_ = lastPacket;
        complete_ = complete;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setPen(Qt::NoPen);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRect bar(0, 2, width() - 1, height() - 4);
        painter.setBrush(QColor(226, 232, 240));
        painter.drawRoundedRect(bar, 6, 6);
        if (last_packet_ < 0)
            return;

        const int knownCount = last_packet_ + 1;
        const int displayCount = complete_
                                     ? knownCount
                                     : std::max(knownCount + 1,
                                                int(std::ceil(knownCount * 1.12)));
        const double scale = double(bar.width()) / std::max(1, displayCount);
        auto paintPacket = [&](std::uint16_t id, const QColor& color) {
            const int x0 = std::clamp(int(std::floor(id * scale)), 0, bar.width());
            const int x1 = std::clamp(int(std::ceil((id + 1) * scale)), 0, bar.width());
            if (x1 > x0)
                painter.fillRect(bar.left() + x0, bar.top(), x1 - x0,
                                 bar.height(), color);
        };
        for (const auto id : received_)
            paintPacket(id, QColor(43, 125, 233));
        for (const auto id : crc_failed_)
            paintPacket(id, QColor(234, 179, 8));
        for (const auto id : missing_)
            paintPacket(id, QColor(224, 82, 82));
    }

private:
    std::vector<std::uint16_t> received_;
    std::vector<std::uint16_t> crc_failed_;
    std::vector<std::uint16_t> missing_;
    int last_packet_ = -1;
    bool complete_ = false;
};

SsdvImageWindow::SsdvImageWindow(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("SSDV图像接收"));
    setWindowFlag(Qt::Window, true);
    resize(760, 680);
    setMinimumSize(520, 480);
    setStyleSheet(QStringLiteral(
        "QDialog { background:#f7faff; color:#17202a; }"
        "QFrame#ssdvCard { background:#ffffff; border:1px solid #d9e5f2; "
        "border-radius:9px; }"));
    buildUi();
}

void SsdvImageWindow::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 14);
    root->setSpacing(12);

    // ---- Reception summary card --------------------------------------
    auto* metaCard = new QFrame(this);
    metaCard->setObjectName(QStringLiteral("ssdvCard"));
    auto* metaLayout = new QVBoxLayout(metaCard);
    metaLayout->setContentsMargins(14, 12, 14, 12);
    metaLayout->setSpacing(8);

    auto* titleRow = new QHBoxLayout;
    title_label_ = new QLabel(tr("等待SSDV图像数据"), metaCard);
    title_label_->setStyleSheet(kTitleStyle);
    status_badge_ = new QLabel(tr("接收中"), metaCard);
    status_badge_->setStyleSheet(kBadgeBaseStyle + QStringLiteral(
        "background:#d97706;"));
    status_badge_->setAlignment(Qt::AlignCenter);
    titleRow->addWidget(title_label_, 1);
    titleRow->addWidget(status_badge_);
    metaLayout->addLayout(titleRow);

    detail_label_ = new QLabel(tr("等待SSDV图像数据"), metaCard);
    detail_label_->setStyleSheet(kDetailStyle);
    detail_label_->setWordWrap(true);
    metaLayout->addWidget(detail_label_);

    auto* legendRow = new QHBoxLayout;
    legendRow->setSpacing(12);
    auto makeLegend = [metaCard](const QColor& color, const QString& text) {
        auto* item = new QLabel(QStringLiteral("●  ") + text, metaCard);
        item->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                .arg(color.name()));
        return item;
    };
    legendRow->addWidget(makeLegend(QColor(43, 125, 233), tr("已接收")));
    legendRow->addWidget(makeLegend(QColor(234, 179, 8), tr("CRC未通过")));
    legendRow->addWidget(makeLegend(QColor(224, 82, 82), tr("确认缺失")));
    legendRow->addWidget(makeLegend(QColor(148, 163, 184), tr("尚未确定")));
    legendRow->addStretch(1);
    metaLayout->addLayout(legendRow);

    auto* progressRow = new QVBoxLayout;
    progressRow->setSpacing(5);
    progress_bar_ = new SsdvProgressBar(metaCard);
    progress_label_ = new QLabel(tr("尚未接收到数据包"), metaCard);
    progress_label_->setStyleSheet(kProgressTextStyle);
    progressRow->addWidget(progress_bar_);
    progressRow->addWidget(progress_label_);
    metaLayout->addLayout(progressRow);

    root->addWidget(metaCard);

    // ---- Image card ---------------------------------------------------
    auto* imageCard = new QFrame(this);
    imageCard->setStyleSheet(QStringLiteral(
        "QFrame { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 #1f252d, stop:1 #2b3440); border:1px solid #1a222c; "
        "border-radius:10px; }"));
    auto* imageLayout = new QVBoxLayout(imageCard);
    imageLayout->setContentsMargins(10, 10, 10, 10);

    image_label_ = new QLabel(imageCard);
    image_label_->setAlignment(Qt::AlignCenter);
    image_label_->setMinimumSize(320, 260);
    image_label_->hide();

    placeholder_label_ = new QLabel(tr("等待SSDV图像数据"), imageCard);
    placeholder_label_->setAlignment(Qt::AlignCenter);
    placeholder_label_->setStyleSheet(kPlaceholderStyle);
    placeholder_label_->setMinimumSize(320, 260);

    imageLayout->addWidget(image_label_, 1);
    imageLayout->addWidget(placeholder_label_, 1);
    root->addWidget(imageCard, 1);

    // ---- Button row ----------------------------------------------------
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    previous_button_ = new QPushButton(QStringLiteral("◀"), this);
    previous_button_->setToolTip(tr("上一张图像"));
    previous_button_->setEnabled(false);
    previous_button_->setStyleSheet(kGalleryButtonStyle);
    gallery_position_label_ = new QLabel(QStringLiteral("0 / 0"), this);
    gallery_position_label_->setAlignment(Qt::AlignCenter);
    gallery_position_label_->setMinimumWidth(56);
    gallery_position_label_->setStyleSheet(
        QStringLiteral("color:#5b6b7d; font-size:12px;"));
    next_button_ = new QPushButton(QStringLiteral("▶"), this);
    next_button_->setToolTip(tr("下一张图像"));
    next_button_->setEnabled(false);
    next_button_->setStyleSheet(kGalleryButtonStyle);
    buttons->addWidget(previous_button_);
    buttons->addWidget(gallery_position_label_);
    buttons->addWidget(next_button_);
    buttons->addStretch(1);

    auto* copyButton = new QPushButton(tr("复制图像"), this);
    copyButton->setStyleSheet(kButtonStyle);
    open_directory_button_ = new QPushButton(tr("打开目录"), this);
    open_directory_button_->setEnabled(false);
    open_directory_button_->setStyleSheet(kPrimaryButtonStyle);
    auto* clearButton = new QPushButton(tr("清除图像"), this);
    clearButton->setStyleSheet(kDangerButtonStyle);
    buttons->addWidget(copyButton);
    buttons->addWidget(open_directory_button_);
    buttons->addWidget(clearButton);
    root->addLayout(buttons);

    // ---- Path label -----------------------------------------------------
    path_label_ = new QLabel(tr("保存路径：—"), this);
    path_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    path_label_->setWordWrap(true);
    path_label_->setStyleSheet(kPathStyle);
    root->addWidget(path_label_);

    connect(copyButton, &QPushButton::clicked, this, [this] {
        if (!image_.isNull())
            QApplication::clipboard()->setImage(image_);
    });
    connect(previous_button_, &QPushButton::clicked, this, [this] {
        showGalleryImage(gallery_index_ - 1);
    });
    connect(next_button_, &QPushButton::clicked, this, [this] {
        showGalleryImage(gallery_index_ + 1);
    });
    connect(open_directory_button_, &QPushButton::clicked, this, [this] {
        if (!image_path_.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(
                QFileInfo(image_path_).absolutePath()));
    });
    connect(clearButton, &QPushButton::clicked, this, [this] {
        if (clear_callback_)
            clear_callback_();
        clearDisplay();
    });
}

void SsdvImageWindow::setClearCallback(std::function<void()> callback)
{
    clear_callback_ = std::move(callback);
}

void SsdvImageWindow::updateImage(const SsdvImageUpdate& update)
{
    if (update.generation < minimum_generation_)
        return;

    int index = -1;
    for (int i = 0; i < gallery_.size(); ++i) {
        if (gallery_.at(i).path == update.path) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        gallery_.append(update);
        index = gallery_.size() - 1;
    } else {
        gallery_[index] = update;
    }
    if (gallery_.size() > kMaxGalleryImages) {
        gallery_.removeFirst();
        --index;
        if (gallery_index_ > 0)
            --gallery_index_;
    }
    // Follow the newest image while it is arriving. If the operator is
    // browsing an older image, keep that selection stable.
    if (gallery_index_ < 0 || gallery_index_ == index ||
        gallery_index_ == gallery_.size() - 2)
        showGalleryImage(index);
    else
        showGalleryImage(gallery_index_);
    if (!isVisible()) {
        show();
        raise();
    }
}


void SsdvImageWindow::showGalleryImage(int index)
{
    if (index < 0 || index >= gallery_.size())
        return;
    gallery_index_ = index;
    const auto& update = gallery_.at(index);
    image_ = update.image;
    image_complete_ = update.complete;
    image_path_ = update.path;
    refreshMetadata();
    refreshPixmap();
}

void SsdvImageWindow::refreshMetadata()
{
    if (gallery_index_ < 0 || gallery_index_ >= gallery_.size())
        return;
    const auto& update = gallery_.at(gallery_index_);

    title_label_->setText(tr("%1 · SSDV 图像 %2")
                              .arg(update.satellite)
                              .arg(update.image_id));

    detail_label_->setText(
        tr("分辨率 %1 × %2　·　质量 %3")
            .arg(update.width)
            .arg(update.height)
            .arg(update.quality));

    progress_bar_->setPackets(update.received_packet_ids,
                              update.crc_failed_packet_ids,
                              update.missing_packet_ids,
                              update.last_packet,
                              update.complete);
    if (update.complete) {
        const int totalPackets = std::max(1, update.last_packet + 1);
        const double integrity = 100.0 * update.received_packets / totalPackets;
        progress_label_->setText(
            tr("已接收 %1 / %2 包　·　CRC未通过 %3 包　·　缺失 %4 包　·　完整率 %5%")
                .arg(update.received_packets)
                .arg(totalPackets)
                .arg(update.crc_failed_packets)
                .arg(update.missing_packets)
                .arg(integrity, 0, 'f', 1));
    } else {
        progress_label_->setText(
            tr("已接收 %1 包　·　CRC未通过 %2 包　·　确认缺失 %3 包　·　最新包序号 %4　·　总包数待确定")
                .arg(update.received_packets)
                .arg(update.crc_failed_packets)
                .arg(update.missing_packets)
                .arg(update.last_packet));
    }

    if (update.complete) {
        status_badge_->setText(tr("完成"));
        status_badge_->setStyleSheet(kBadgeBaseStyle +
                                     QStringLiteral("background:#159447;"));
    } else {
        status_badge_->setText(tr("接收中"));
        status_badge_->setStyleSheet(kBadgeBaseStyle +
                                     QStringLiteral("background:#d97706;"));
    }

    path_label_->setText(tr("保存路径：%1").arg(update.path));
    open_directory_button_->setEnabled(!image_path_.isEmpty());
    gallery_position_label_->setText(
        QStringLiteral("%1 / %2").arg(gallery_index_ + 1).arg(gallery_.size()));
    previous_button_->setEnabled(gallery_index_ > 0);
    next_button_->setEnabled(gallery_index_ + 1 < gallery_.size());
}

void SsdvImageWindow::clearDisplay()
{
    ++minimum_generation_;
    image_ = {};
    image_complete_ = false;
    image_path_.clear();
    gallery_.clear();
    gallery_index_ = -1;
    image_label_->clear();
    image_label_->hide();
    placeholder_label_->show();
    title_label_->setText(tr("等待SSDV图像数据"));
    detail_label_->setText(tr("等待SSDV图像数据"));
    progress_bar_->setPackets({}, {}, {}, -1, false);
    progress_label_->setText(tr("尚未接收到数据包"));
    status_badge_->setText(tr("接收中"));
    status_badge_->setStyleSheet(kBadgeBaseStyle +
                                 QStringLiteral("background:#d97706;"));
    path_label_->setText(tr("保存路径：—"));
    open_directory_button_->setEnabled(false);
    gallery_position_label_->setText(QStringLiteral("0 / 0"));
    previous_button_->setEnabled(false);
    next_button_->setEnabled(false);
}

void SsdvImageWindow::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    refreshPixmap();
}

void SsdvImageWindow::refreshPixmap()
{
    if (image_.isNull()) {
        image_label_->clear();
        image_label_->hide();
        placeholder_label_->show();
        return;
    }
    placeholder_label_->hide();
    image_label_->show();
    const QSize target = image_label_->size().expandedTo(QSize(160, 120));
    image_label_->setPixmap(QPixmap::fromImage(image_).scaled(
        target, Qt::KeepAspectRatio,
        image_complete_ ? Qt::SmoothTransformation : Qt::FastTransformation));
}
