#ifndef BRIGHTNESSWINDOW_H
#define BRIGHTNESSWINDOW_H

#include <QtWidgets>
#include "imageview.h"

namespace Ui {
class BrightnessWindow;
class MainWindow;
}

class BrightnessWindow : public QWidget
{
    Q_OBJECT

public:
    explicit BrightnessWindow(QWidget *parent, Ui::MainWindow *uiMain);
    ~BrightnessWindow();
    float sliderToFloat(int value);
    int floatToSlider(float value);
    void setImageView(ImageView *imageView);
    void resetSliders();

signals:
    void retouchParametersChanged(ImageRetouch params);

public slots:
    void handleBrightnessSliderValueChanged(int value);
    void handleContrastSliderValueChanged(int value);
    void handleGammaSliderValueChanged(int value);
    void handleBrightnessLineEditTextChanged(QString text);
    void handleContrastLineEditTextChanged(QString text);
    void handleGammaLineEditTextChanged(QString text);
    void handleResetButtonClicked();

private:
    Ui::BrightnessWindow *ui;
    ImageView *m_imageView;
    ImageRetouch m_retouchParams;
    bool ignoreTextChange;
};

#endif // BRIGHTNESSWINDOW_H
