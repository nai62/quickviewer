#ifndef FILEASSOCDIALOG_H
#define FILEASSOCDIALOG_H

#include <QtGui>
#if QT_VERSION_MAJOR >= 5
#    include <QtWidgets>
#endif

namespace Ui {
class FileAssocDialog;
}

class AssocInfo
{
public:
    QString Name;
    QString Description;
    QString IconName;
    QStringList Extensions;
};

class FileAssocDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileAssocDialog(QWidget *parent = nullptr);
    ~FileAssocDialog();
    //    void closeEvent(QCloseEvent *event) override;
    QStringList enumrateFormats();
    void registerEntries(QStringList formats);
    void unregisterEntries();
    QString getExecuteApplication();
    QString getIconPath(QString iconName);

    static QSettings::Format RegFormat;

public slots:
    void handleAllOnButtonClicked();
    void handleAllOffButtonClicked();
    void handleButtonBoxAccepted();

signals:
    void closed();

private:
    Ui::FileAssocDialog *ui;
    QMap<QString, AssocInfo> m_assocs;
    QMap<QString, QCheckBox *> m_assocOfActions;
};

#endif // FILEASSOCDIALOG_H
