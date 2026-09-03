#ifndef RETOUCHWINDOW_H
#define RETOUCHWINDOW_H

#include <QtWidgets>
#include "imageview.h"

namespace Ui {
class RetouchWindow;
}

class RetouchWindow : public QWidget
{
    Q_OBJECT

public:
    explicit RetouchWindow(QWidget *parent);
    ~RetouchWindow() override;
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
    Ui::RetouchWindow *ui;
    ImageView *m_imageView;
    ImageRetouch m_retouchParams;
    bool ignoreTextChange;
};

#endif // RETOUCHWINDOW_H
