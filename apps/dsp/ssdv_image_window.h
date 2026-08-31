#pragma once

#include "ssdv_receiver.h"

#include <QCoreApplication>
#include <QDialog>
#include <QImage>
#include <QVector>

#include <functional>

class QFrame;
class QLabel;
class QProgressBar;
class QPushButton;
class SsdvProgressBar;

class SsdvImageWindow final : public QDialog
{
    Q_DECLARE_TR_FUNCTIONS(ASRTU)

public:
    explicit SsdvImageWindow(QWidget* parent = nullptr);

    void setClearCallback(std::function<void()> callback);
    void updateImage(const SsdvImageUpdate& update);
    void clearDisplay();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void buildUi();
    void refreshPixmap();
    void showGalleryImage(int index);
    void refreshMetadata();

    QLabel* title_label_ = nullptr;
    QLabel* status_badge_ = nullptr;
    QLabel* detail_label_ = nullptr;
    SsdvProgressBar* progress_bar_ = nullptr;
    QLabel* progress_label_ = nullptr;
    QLabel* image_label_ = nullptr;
    QLabel* placeholder_label_ = nullptr;
    QLabel* path_label_ = nullptr;
    QPushButton* open_directory_button_ = nullptr;
    QPushButton* previous_button_ = nullptr;
    QPushButton* next_button_ = nullptr;
    QLabel* gallery_position_label_ = nullptr;
    QImage image_;
    QString image_path_;
    QVector<SsdvImageUpdate> gallery_;
    std::uint64_t minimum_generation_ = 0;
    int gallery_index_ = -1;
    bool image_complete_ = false;
    std::function<void()> clear_callback_;
};
