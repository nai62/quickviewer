#ifndef EXIFDIALOG_H
#define EXIFDIALOG_H

#include <QDialog>
#include "exif.h"
#include "imagecontent.h"

namespace Ui {
class ExifDialog;
}

class ExifDialog : public QWidget
{
    Q_OBJECT

public:
    explicit ExifDialog(QWidget *parent = nullptr);
    ~ExifDialog();

    void setExif(const ImageContent &content);
    void closeEvent(QCloseEvent *event) override;

signals:
    void closed();

public slots:
    void handleClipboardButtonClicked();

private:
    Ui::ExifDialog *ui;
    bool m_hasContent;
    QString m_exif;
    QString m_exifPath;
    QSize m_originalSize;
    easyexif::EXIFInfo m_exifInfo;
    void updateExifText();
    QString generateFlash(char flash);
    //    QString generateFlashMode(unsigned short mode);
    //    QString generateFlashReturnedLight(unsigned short light);
    QString generateOrientation(unsigned short orient);
};

#endif // EXIFDIALOG_H
