#ifndef FILEASSOCDIALOG_H
#define FILEASSOCDIALOG_H

#include <QtWidgets>

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
    void registerFormat(const QString &format);
    void unregisterFormat(const QString &format);
    void writeCapabilities(const QStringList &formats);
    void openAssociationSettings();

    Ui::FileAssocDialog *ui;
    QMap<QString, AssocInfo> m_assocs;
    QMap<QString, QCheckBox *> m_assocOfActions;
};

#endif // FILEASSOCDIALOG_H
