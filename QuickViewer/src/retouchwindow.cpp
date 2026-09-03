#include "retouchwindow.h"
#include "ui_retouchwindow.h"
#include "pagemanager.h"

RetouchWindow::RetouchWindow(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::RetouchWindow),
      ignoreTextChange(false)
{
    ui->setupUi(this);
    ui->checkBoxForAll->setVisible(false);
}

RetouchWindow::~RetouchWindow()
{
    delete ui;
}

float RetouchWindow::sliderToFloat(int value)
{
    // -20 ->   0.01
    // -10 ->   0.1
    //   0 ->   1.0
    //  10 ->  10
    //  20 -> 100
    return powf(10, 0.1f / 8 * value);
}

int RetouchWindow::floatToSlider(float value)
{
    //   0.01 -> -20
    //   0.1  -> -10
    //   1.0  ->   0
    //  10    ->  10
    // 100    ->  20
    return (int)(10 * 8 * log10f(value));
}

void RetouchWindow::setImageView(ImageView *imageView)
{
    m_imageView = imageView;
    if (!m_imageView || m_imageView->renderedPageCount() == 0) {
        return;
    }

    m_retouchParams = m_imageView->retouchParameters();
    resetSliders();
}

void RetouchWindow::resetSliders()
{
    ui->sliderBrightness->setValue((int)m_retouchParams.Brightness);
    ui->lineBrightness->setText(QString::number(ui->sliderBrightness->value()));

    ui->sliderContrast->setValue(floatToSlider(m_retouchParams.Contrast));
    ui->lineContrast->setText(QString::number(ui->sliderContrast->value()));

    ui->sliderGamma->setValue(floatToSlider(m_retouchParams.Gamma));
    ui->lineGamma->setText(QString::number(ui->sliderGamma->value()));
}

void RetouchWindow::handleBrightnessSliderValueChanged(int value)
{
    m_retouchParams.Brightness = value;
    if (!ignoreTextChange) {
        ui->lineBrightness->setText(QString::number(value));
    }
    emit retouchParametersChanged(m_retouchParams);
}

void RetouchWindow::handleContrastSliderValueChanged(int value)
{
    m_retouchParams.Contrast = sliderToFloat(value);
    if (!ignoreTextChange) {
        ui->lineContrast->setText(QString::number(value));
    }
    emit retouchParametersChanged(m_retouchParams);
}

void RetouchWindow::handleGammaSliderValueChanged(int value)
{
    m_retouchParams.Gamma = sliderToFloat(value);
    if (!ignoreTextChange) {
        ui->lineGamma->setText(QString::number(value));
    }
    emit retouchParametersChanged(m_retouchParams);
}

void RetouchWindow::handleBrightnessLineEditTextChanged(QString text)
{
    int value = text.toInt();
    ignoreTextChange = true;
    ui->sliderBrightness->setValue((int)value);
    ignoreTextChange = false;
}

void RetouchWindow::handleContrastLineEditTextChanged(QString text)
{
    int value = text.toInt();
    ignoreTextChange = true;
    ui->sliderContrast->setValue((int)value);
    ignoreTextChange = false;
}

void RetouchWindow::handleGammaLineEditTextChanged(QString text)
{
    int value = text.toInt();
    ignoreTextChange = true;
    ui->sliderGamma->setValue((int)value);
    ignoreTextChange = false;
}

void RetouchWindow::handleResetButtonClicked()
{
    m_retouchParams = ImageRetouch();
    resetSliders();
    emit retouchParametersChanged(m_retouchParams);
}
