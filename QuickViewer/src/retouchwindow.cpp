#include "retouchwindow.h"
#include "ui_retouchwindow.h"

#include <cmath>

namespace {
constexpr float SliderLogScale = 80.0f;
}

RetouchWindow::RetouchWindow(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::RetouchWindow),
      m_ignoreTextChange(false)
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
    return std::pow(10.0f, value / SliderLogScale);
}

int RetouchWindow::floatToSlider(float value)
{
    return static_cast<int>(SliderLogScale * std::log10(value));
}

void RetouchWindow::initializeFromImageView(const ImageView *imageView)
{
    if (!imageView || imageView->renderedPageCount() == 0) {
        return;
    }

    m_retouchParams = imageView->retouchParameters();
    resetSliders();
}

void RetouchWindow::resetSliders()
{
    const QSignalBlocker brightnessSignals(ui->sliderBrightness);
    const QSignalBlocker contrastSignals(ui->sliderContrast);
    const QSignalBlocker gammaSignals(ui->sliderGamma);

    ui->sliderBrightness->setValue(static_cast<int>(m_retouchParams.brightness));
    ui->lineBrightness->setText(QString::number(ui->sliderBrightness->value()));

    ui->sliderContrast->setValue(floatToSlider(m_retouchParams.contrast));
    ui->lineContrast->setText(QString::number(ui->sliderContrast->value()));

    ui->sliderGamma->setValue(floatToSlider(m_retouchParams.gamma));
    ui->lineGamma->setText(QString::number(ui->sliderGamma->value()));
}

void RetouchWindow::handleBrightnessSliderValueChanged(int value)
{
    m_retouchParams.brightness = value;
    if (!m_ignoreTextChange) {
        ui->lineBrightness->setText(QString::number(value));
    }
    emit retouchParametersChanged(m_retouchParams);
}

void RetouchWindow::handleContrastSliderValueChanged(int value)
{
    m_retouchParams.contrast = sliderToFloat(value);
    if (!m_ignoreTextChange) {
        ui->lineContrast->setText(QString::number(value));
    }
    emit retouchParametersChanged(m_retouchParams);
}

void RetouchWindow::handleGammaSliderValueChanged(int value)
{
    m_retouchParams.gamma = sliderToFloat(value);
    if (!m_ignoreTextChange) {
        ui->lineGamma->setText(QString::number(value));
    }
    emit retouchParametersChanged(m_retouchParams);
}

void RetouchWindow::handleBrightnessLineEditTextChanged(QString text)
{
    int value = text.toInt();
    m_ignoreTextChange = true;
    ui->sliderBrightness->setValue(value);
    m_ignoreTextChange = false;
}

void RetouchWindow::handleContrastLineEditTextChanged(QString text)
{
    int value = text.toInt();
    m_ignoreTextChange = true;
    ui->sliderContrast->setValue(value);
    m_ignoreTextChange = false;
}

void RetouchWindow::handleGammaLineEditTextChanged(QString text)
{
    int value = text.toInt();
    m_ignoreTextChange = true;
    ui->sliderGamma->setValue(value);
    m_ignoreTextChange = false;
}

void RetouchWindow::handleResetButtonClicked()
{
    m_retouchParams = ImageRetouch();
    resetSliders();
    emit retouchParametersChanged(m_retouchParams);
}
