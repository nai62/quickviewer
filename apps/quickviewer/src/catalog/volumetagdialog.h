#ifndef VOLUMETAGDIALOG_H
#define VOLUMETAGDIALOG_H

#include <QDialog>
#include <QStringList>

#include "catalogrecords.h"

namespace Ui {
class VolumeTagDialog;
}

/**
 * Sets the title a catalog shows for one volume, and the tags that volume
 * carries. The tags a volume name suggested are only the starting point: this
 * is where the user accepts, removes or adds them.
 */
class VolumeTagDialog : public QDialog
{
    Q_OBJECT
public:
    explicit VolumeTagDialog(QWidget *parent = nullptr);
    ~VolumeTagDialog() override;

    /**
     * Fills the dialog for \a volume, offering the tag names \a knownTagNames
     * to pick from and checking the ones in \a volumeTagNames.
     */
    void setVolume(const VolumeThumbRecord &volume,
                   const QStringList &knownTagNames,
                   const QStringList &volumeTagNames);

    QString displayName() const;
    QStringList tags() const;

private slots:
    void handleAddTagButtonClicked();

private:
    Ui::VolumeTagDialog *ui;
};

#endif // VOLUMETAGDIALOG_H
