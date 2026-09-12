#ifndef OPTIONSDIALOG_H
#define OPTIONSDIALOG_H

#include <QtWidgets>

#include "imagestring.h"

#define IRFANVIEW_WINDOWTITLE_FORMAT "%p %2| %p"
#define IRFANVIEW_STATUSBAR_FORMAT "%s %n %m %f / %b %2| %s %m %f / %b"

namespace Ui {
class OptionsDialog;
}

class OptionsDialog : public QDialog
{
    Q_OBJECT
public:
    OptionsDialog(QWidget *parent);
    ~OptionsDialog();
    void reflectResults();
    void resetColorButton(QPushButton *btn, QColor color);
    void resetColorBox();
    void resetWindowTitleSample();
    void resetStatusbarSample();
    void initFormatUsage();

public slots:
    void handlePrimaryColorButtonClicked();
    void handleSecondaryColorButtonClicked();
    void handleCheckeredPatternCheckBoxClicked(bool checked);
    void handleWindowTitleStyleRadioButtonToggled();
    void handleWindowTitleUserDefinedRadioButtonToggled(bool checked);
    void handleStatusBarStyleRadioButtonToggled();
    void handleStatusBarUserDefinedRadioButtonToggled(bool checked);
    void handleWindowTitleUserStyleLineEditTextEdited(QString text);
    void handleStatusBarUserStyleLineEditTextEdited(QString text);
    void handleShowUsageCheckBoxClicked(bool checked);

private:
    Ui::OptionsDialog *ui;

    int m_slideShowWait;
    int m_maxVolumesCache;
    QColor m_backgroundColor;
    QColor m_backgroundColor2;
    bool m_useCheckeredPattern;
    ImageString m_imageString;
};

#endif // OPTIONSDIALOG_H
