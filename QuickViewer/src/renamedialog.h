#ifndef RENAMEDIALOG_H
#define RENAMEDIALOG_H

#include <QtWidgets>

namespace Ui {
class RenameDialog;
}

class RenameDialog : public QDialog
{
    Q_OBJECT
public:
    RenameDialog(QWidget *parent, QString path, QString filename);
    QString newName();

public slots:
    void handleFilenameLineEditTextChanged(QString text);
    void handleButtonBoxAccepted();

private:
    Ui::RenameDialog *ui;

    QString m_path;
    QString m_filename;
};

#endif // RENAMEDIALOG_H
