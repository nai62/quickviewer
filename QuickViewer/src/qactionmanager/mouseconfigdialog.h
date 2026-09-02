#ifndef MOUSECONFIGDIALOG_H
#define MOUSECONFIGDIALOG_H

#include <QtCore>
#include <QtWidgets>

#include "qactionmanager.h"
#include "qmousesequence.h"

namespace Ui {
class KeyConfigDialog;
}

/**
 * @brief The MouseConfigDialog class
 *
 * a simple Mouse Configure Dialog
 */
class MouseConfigDialog : public QDialog
{
    Q_OBJECT
public:
    typedef QActionManager<QMouseSequence, QMouseValue, QAction *> MouseActionManager;

    MouseConfigDialog(MouseActionManager &mouseActions, QWidget *parent);
    ~MouseConfigDialog();

    void setEditTextWithoutSignal(QString text);
    //    void revertMouseChanges();
    void resetMouseCheckBox();
    void resetView();
    MouseActionManager &mouseActions() { return m_mouseActions; }

public slots:
    void handleTreeWidgetCurrentItemChanged(QTreeWidgetItem *item, QTreeWidgetItem *previous);
    void handleRecordButtonKeySequenceChanged(QMouseSequence key);
    void handleAddSequenceButtonClicked();
    void handleResetButtonClicked();
    void handleShortcutLineEditTextChanged(QString text);
    void handleInputOptionToggled();
    void handleButtonBoxClicked(QAbstractButton *button);

private:
    Ui::KeyConfigDialog *ui;
    bool m_keyCapturing;
    QString m_actionName;
    //    QMap<QString, QMouseSequence> m_prevKeyConfigs;
    MouseActionManager m_mouseActions;
    bool m_ignoreEdited;
    QMap<QString, QString> m_actionNameByIconText;
};

#endif // MOUSECONFIGDIALOG_H
