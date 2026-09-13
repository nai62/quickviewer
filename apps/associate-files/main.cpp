#include "fileassocdialog.h"
#include <QApplication>
#include "languagemanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    LanguageManager selector("quickviewer_", "translations/");
    selector.resetTranslator("");

    FileAssocDialog w;
    w.show();

    return a.exec();
}
