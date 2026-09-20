#include "filemanager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

#ifdef Q_OS_WIN
#    include <windows.h>
#    include <shellapi.h>
#endif

QString explorerArgument(const QString &path)
{
    if (path.isEmpty()) {
        return QString();
    }
    const QFileInfo info(path);
    // canonicalFilePath() is empty for a path that does not exist, and Explorer
    // opens somewhere else entirely for one it cannot resolve.
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty()) {
        return QString();
    }
    // Explorer selects the entry named after /select, and opens the folder
    // itself when it is given one without it.
    const QString native = QDir::toNativeSeparators(canonical);
    if (info.isDir()) {
        return QLatin1Char('"') + native + QLatin1Char('"');
    }
    return QStringLiteral("/select,") + QLatin1Char('"') + native + QLatin1Char('"');
}

void showInFileManager(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
#ifdef Q_OS_WIN
    const QString argument = explorerArgument(path);
    if (argument.isEmpty()) {
        return;
    }
    // QProcess wraps a program string that holds a space in quotes, and Explorer
    // wants the quotes around the path alone, so the shell is asked directly.
    ::ShellExecuteW(nullptr,
                    L"open",
                    L"explorer.exe",
                    reinterpret_cast<const wchar_t *>(argument.utf16()),
                    nullptr,
                    SW_SHOWNORMAL);
#else
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir() ? canonical : info.absolutePath()));
#endif
}
