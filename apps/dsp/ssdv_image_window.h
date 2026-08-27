#pragma once

#include "ssdv_receiver.h"

#include <QDialog>
#include <QImage>
#include <QVector>

#include <functional>

class QLabel;
class QPushButton;

class SsdvImageWindow final : public QDialog
{
public:
    explicit SsdvImageWindow(QWidget* parent = nullptr);

    void setClearCallback(std::function<void()> callback);
    void updateImage(const SsdvImageUpdate& update);
    void clearDisplay();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void refreshPixmap();
    void showGalleryImage(int index);
    void refreshMetadata();

    QLabel* image_label_ = nullptr;
    QLabel* metadata_label_ = nullptr;
    QLabel* path_label_ = nullptr;
    QPushButton* open_directory_button_ = nullptr;
    QPushButton* previous_button_ = nullptr;
    QPushButton* next_button_ = nullptr;
    QLabel* gallery_position_label_ = nullptr;
    QImage image_;
    QString image_path_;
    QVector<SsdvImageUpdate> gallery_;
    int gallery_index_ = -1;
    std::function<void()> clear_callback_;
};
