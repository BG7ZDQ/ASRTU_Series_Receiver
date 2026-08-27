#include "ssdv_image_window.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QUrl>

SsdvImageWindow::SsdvImageWindow(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QCoreApplication::translate("ASRTU", "SSDV图像接收"));
    setWindowFlag(Qt::Window, true);
    resize(620, 560);
    setMinimumSize(420, 380);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->setSpacing(10);

    metadata_label_ = new QLabel(
        QCoreApplication::translate("ASRTU", "等待SSDV图像数据"), this);
    metadata_label_->setWordWrap(true);
    metadata_label_->setStyleSheet(QStringLiteral(
        "font-weight:600; color:#243447; padding:8px; background:#f1f6fb; "
        "border:1px solid #d9e5f2; border-radius:6px;"));
    layout->addWidget(metadata_label_);

    image_label_ = new QLabel(this);
    image_label_->setAlignment(Qt::AlignCenter);
    image_label_->setMinimumSize(320, 240);
    image_label_->setStyleSheet(QStringLiteral(
        "background:#20252b; border:1px solid #c7d4e2; border-radius:4px;"));
    layout->addWidget(image_label_, 1);

    auto* buttons = new QHBoxLayout;
    previous_button_ = new QPushButton(QStringLiteral("◀"), this);
    previous_button_->setToolTip(
        QCoreApplication::translate("ASRTU", "上一张图像"));
    previous_button_->setEnabled(false);
    gallery_position_label_ = new QLabel(QStringLiteral("0 / 0"), this);
    gallery_position_label_->setAlignment(Qt::AlignCenter);
    gallery_position_label_->setMinimumWidth(64);
    next_button_ = new QPushButton(QStringLiteral("▶"), this);
    next_button_->setToolTip(
        QCoreApplication::translate("ASRTU", "下一张图像"));
    next_button_->setEnabled(false);
    buttons->addWidget(previous_button_);
    buttons->addWidget(gallery_position_label_);
    buttons->addWidget(next_button_);
    buttons->addStretch(1);
    auto* copyButton = new QPushButton(
        QCoreApplication::translate("ASRTU", "复制图像"), this);
    open_directory_button_ = new QPushButton(
        QCoreApplication::translate("ASRTU", "打开目录"), this);
    open_directory_button_->setEnabled(false);
    auto* clearButton = new QPushButton(
        QCoreApplication::translate("ASRTU", "清除图像"), this);
    buttons->addWidget(copyButton);
    buttons->addWidget(open_directory_button_);
    buttons->addWidget(clearButton);
    layout->addLayout(buttons);

    path_label_ = new QLabel(
        QCoreApplication::translate("ASRTU", "保存路径：—"), this);
    path_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    path_label_->setWordWrap(true);
    path_label_->setStyleSheet(QStringLiteral("color:#66788a;"));
    layout->addWidget(path_label_);

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
    // Follow the newest image while it is arriving. If the operator is
    // browsing an older image, keep that selection stable.
    if (gallery_index_ < 0 || gallery_index_ == index ||
        gallery_index_ == gallery_.size() - 2)
        showGalleryImage(index);
    else
        showGalleryImage(gallery_index_);
    if (!isVisible())
        show();
    raise();
    activateWindow();
}

void SsdvImageWindow::showGalleryImage(int index)
{
    if (index < 0 || index >= gallery_.size())
        return;
    gallery_index_ = index;
    const auto& update = gallery_.at(index);
    image_ = update.image;
    image_path_ = update.path;
    refreshMetadata();
    refreshPixmap();
}

void SsdvImageWindow::refreshMetadata()
{
    if (gallery_index_ < 0 || gallery_index_ >= gallery_.size())
        return;
    const auto& update = gallery_.at(gallery_index_);
    metadata_label_->setText(
        QCoreApplication::translate(
            "ASRTU",
            "来源卫星：%1　图像ID：%2\n"
            "分辨率：%3×%4　质量：%5\n"
            "数据包：%6（%7–%8，区间缺失%9）　状态：%10")
            .arg(update.satellite)
            .arg(update.image_id)
            .arg(update.width)
            .arg(update.height)
            .arg(update.quality)
            .arg(update.received_packets)
            .arg(update.first_packet)
            .arg(update.last_packet)
            .arg(update.missing_packets)
            .arg(update.complete
                     ? QCoreApplication::translate("ASRTU", "完成")
                     : QCoreApplication::translate("ASRTU", "接收中")));
    path_label_->setText(
        QCoreApplication::translate("ASRTU", "保存路径：%1").arg(update.path));
    open_directory_button_->setEnabled(!image_path_.isEmpty());
    gallery_position_label_->setText(
        QStringLiteral("%1 / %2").arg(gallery_index_ + 1).arg(gallery_.size()));
    previous_button_->setEnabled(gallery_index_ > 0);
    next_button_->setEnabled(gallery_index_ + 1 < gallery_.size());
}

void SsdvImageWindow::clearDisplay()
{
    image_ = {};
    image_path_.clear();
    gallery_.clear();
    gallery_index_ = -1;
    image_label_->clear();
    metadata_label_->setText(
        QCoreApplication::translate("ASRTU", "等待SSDV图像数据"));
    path_label_->setText(
        QCoreApplication::translate("ASRTU", "保存路径：—"));
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
    if (image_.isNull())
        return;
    image_label_->setPixmap(QPixmap::fromImage(image_).scaled(
        image_label_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
