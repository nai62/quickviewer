#ifndef KEYCONFIGDIALOG_H
#define KEYCONFIGDIALOG_H

#include <QtCore>
#include <QtWidgets>

#include "qactionmanager.h"

namespace Ui {
class KeyConfigDialog;
}

/**
 * @brief The KeyConfigDialog class
 *
 * a simple Keyboard Configure Dialog
 */
class KeyConfigDialog : public QDialog
{
    Q_OBJECT
public:
    typedef QActionManager<QKeySequence, QKeyCombination, QAction *> KeyActionManager;

    explicit KeyConfigDialog(KeyActionManager &keyActions, QWidget *parent);
    ~KeyConfigDialog();

    bool eventFilter(QObject *obj, QEvent *event);
    void setEditTextWithoutSignal(QString text);
    void resetView();
    KeyActionManager &keyActions() { return m_keyActions; }

signals:

public slots:
    void handleTreeWidgetCurrentItemChanged(QTreeWidgetItem *item, QTreeWidgetItem *previous);
    void handleRecordButtonKeySequenceChanged(QKeySequence key);
    void handleResetButtonClicked();
    void handleShortcutLineEditTextChanged(QString text);
    void handleButtonBoxClicked(QAbstractButton *button);

private:
    Ui::KeyConfigDialog *ui;
    QString m_actionName;
    KeyActionManager m_keyActions;
    QMap<QString, QString> m_actionNameByIconText;
    bool m_ignoreEdited;
    bool m_keyCapturing;
};

#endif // KEYCONFIGDIALOG_H
