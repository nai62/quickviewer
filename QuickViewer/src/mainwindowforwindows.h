#ifndef MAINWINDOWFORWINDOWS_H
#define MAINWINDOWFORWINDOWS_H

#include "mainwindow.h"

class MainWindowForWindows : public MainWindow
{
    Q_OBJECT
public:
    explicit MainWindowForWindows(QWidget *parent = 0);
    bool moveToTrash(QString path) override;
    bool setStayOnTop(bool top) override;
    void setWindowTop(bool signalOnly) override;
    void setMailAttachment(QString path) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // MAINWINDOWFORWINDOWS_H
