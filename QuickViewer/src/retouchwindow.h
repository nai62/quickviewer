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
    void initializeFromImageView(const ImageView *imageView);

signals:
    void retouchParametersChanged(RetouchParameters params);

public slots:
    void handleBrightnessSliderValueChanged(int value);
    void handleContrastSliderValueChanged(int value);
    void handleGammaSliderValueChanged(int value);
    void handleBrightnessLineEditTextChanged(QString text);
    void handleContrastLineEditTextChanged(QString text);
    void handleGammaLineEditTextChanged(QString text);
    void handleResetButtonClicked();

private:
    static float sliderValueToFactor(int sliderValue);
    static int factorToSliderValue(float factor);
    void resetSliders();

    Ui::RetouchWindow *ui;
    RetouchParameters m_retouchParams;
    bool m_ignoreTextChange;
};

#endif // RETOUCHWINDOW_H
