#include <QtCore>

#include <string>

#include "mainwindowforwindows.h"

#include <Windows.h>
#include <dwmapi.h>
#include <mapi.h>
#include <Shellapi.h>

MainWindowForWindows::MainWindowForWindows(QWidget *parent)
    : MainWindow(parent)
{}

bool MainWindowForWindows::setStartupWindowCloaked(bool cloaked)
{
    using DwmSetWindowAttributeFunction = HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
    using DwmFlushFunction = HRESULT(WINAPI *)();
    QLibrary dwmapi("dwmapi");
    auto setWindowAttribute = reinterpret_cast<DwmSetWindowAttributeFunction>(dwmapi.resolve("DwmSetWindowAttribute"));
    auto flush = reinterpret_cast<DwmFlushFunction>(dwmapi.resolve("DwmFlush"));
    if (!setWindowAttribute) {
        qWarning() << "DwmSetWindowAttribute is unavailable";
        return false;
    }

    const auto hwnd = reinterpret_cast<HWND>(winId());
    const BOOL value = cloaked ? TRUE : FALSE;
    if (!cloaked && flush) {
        flush();
    }
    const HRESULT result = setWindowAttribute(hwnd, DWMWA_CLOAK, &value, sizeof(value));
    if (FAILED(result)) {
        qWarning() << "Failed to" << (cloaked ? "cloak" : "uncloak")
                   << "the startup window:" << Qt::hex << result;
        return false;
    }
    return true;
}

bool MainWindowForWindows::moveToTrash(QString path)
{
    WCHAR from[MAX_PATH + 2048] = {0};
    path.toWCharArray(from);
    SHFILEOPSTRUCT fileop = {0};
    fileop.wFunc = FO_DELETE;
    fileop.pFrom = from;
    fileop.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    int rv = SHFileOperation(&fileop);
    if (0 != rv) {
        qDebug() << rv << QString::number(rv).toInt(nullptr, 8);
        return false;
    }

    qDebug() << rv << path;
    return true;
}

bool MainWindowForWindows::setStayOnTop(bool top)
{
    auto hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) {
        return false;
    }
    const HWND insertAfter = top ? HWND_TOPMOST : HWND_NOTOPMOST;
    return ::SetWindowPos(hwnd, insertAfter, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE) != FALSE;
}

void MainWindowForWindows::setWindowTop(bool signalOnly)
{
    auto hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) {
        return;
    }
    if (isMinimized()) {
        ::ShowWindow(hwnd, SW_RESTORE);
    }
    //    ::SwitchToThisWindow(hwnd, false);
    const DWORD currentThreadId = ::GetCurrentThreadId();
    const DWORD foregroundThreadId = ::GetWindowThreadProcessId(::GetForegroundWindow(), nullptr);
    const bool inputAttached = !signalOnly && foregroundThreadId != 0 && foregroundThreadId != currentThreadId && ::AttachThreadInput(currentThreadId, foregroundThreadId, TRUE);

    ::SetForegroundWindow(hwnd);
    ::SetActiveWindow(hwnd);
    ::SetFocus(hwnd);
    ::SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

    if (inputAttached) {
        ::AttachThreadInput(currentThreadId, foregroundThreadId, FALSE);
    }
}

void MainWindowForWindows::setMailAttachment(QString path)
{
    QLibrary lib("mapi32");
#ifdef _MSC_VER
    if (LPMAPISENDMAILW mapi = LPMAPISENDMAILW(lib.resolve("MAPISendMailW"))) {
        QString filePath = QDir::toNativeSeparators(path);
        QString fileName = QFileInfo(path).fileName();
        //        QString subject = q.queryItemValue( "subject", QUrl::FullyDecoded );
        MapiFileDescW doc = {0, 0, 0, 0, 0, 0};
        doc.nPosition = -1;
        doc.lpszPathName = PWSTR(filePath.utf16());
        doc.lpszFileName = PWSTR(fileName.utf16());
        MapiMessageW message = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        //        message.lpszSubject = PWSTR(subject.utf16());
        //        message.lpszNoteText = L"";
        message.nFileCount = 1;
        message.lpFiles = lpMapiFileDescW(&doc);
        switch (mapi(0, 0, &message, MAPI_LOGON_UI | MAPI_DIALOG, 0)) {
        case SUCCESS_SUCCESS:
        case MAPI_E_USER_ABORT:
        case MAPI_E_LOGIN_FAILURE:
            return;
        default:
            break;
        }
    } else
#endif
        if (LPMAPISENDMAIL mapi = LPMAPISENDMAIL(lib.resolve("MAPISendMail"))) {
        QByteArray filePath = QDir::toNativeSeparators(path).toLocal8Bit();
        QByteArray fileName = QFileInfo(path).fileName().toLocal8Bit();
        //        QByteArray subject = q.queryItemValue( "subject", QUrl::FullyDecoded ).toLocal8Bit();
        MapiFileDesc doc = {0, 0, 0, 0, 0, 0};
        doc.nPosition = -1;
        std::string flpath = filePath.toStdString();
        std::string flname = fileName.toStdString();
        doc.lpszPathName = LPSTR(flpath.c_str());
        doc.lpszFileName = LPSTR(flname.c_str());
        MapiMessage message = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        //        message.lpszSubject = LPSTR(subject.constData());
        //        message.lpszNoteText = "";
        message.nFileCount = 1;
        message.lpFiles = lpMapiFileDesc(&doc);
        switch (mapi(0, 0, &message, MAPI_LOGON_UI | MAPI_DIALOG, 0)) {
        case SUCCESS_SUCCESS:
        case MAPI_E_USER_ABORT:
        case MAPI_E_LOGIN_FAILURE:
            return;
        default:
            break;
        }
    }
}

bool MainWindowForWindows::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Leave) {
        return QObject::eventFilter(obj, event);
    }
    return MainWindow::eventFilter(obj, event);
}
