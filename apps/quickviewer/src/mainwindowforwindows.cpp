#include <QtCore>

#include <string>

#include "mainwindowforwindows.h"
#include "startupprofiler.h"

#include <Windows.h>
#include <dwmapi.h>
#include <mapi.h>
#include <shobjidl.h>
#include <Shellapi.h>

MainWindowForWindows::MainWindowForWindows(QWidget *parent)
    : ArchiveAwareMainWindow(parent)
{
}

bool MainWindowForWindows::setStartupWindowCloaked(bool cloaked)
{
    if (cloaked) {
        StartupProfiler::mark("startup.cloak.begin");
        StartupProfiler::mark("startup.cloak.before-winid");
    }
    const auto hwnd = reinterpret_cast<HWND>(winId());
    if (cloaked) {
        StartupProfiler::mark("startup.cloak.after-winid");
    }
    const BOOL value = cloaked ? TRUE : FALSE;
    if (!cloaked) {
        ::DwmFlush();
    }
    const HRESULT result = ::DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &value, sizeof(value));
    if (cloaked) {
        StartupProfiler::mark("startup.cloak.end");
    }
    if (FAILED(result)) {
        qWarning() << "Failed to" << (cloaked ? "cloak" : "uncloak")
                   << "the startup window:" << Qt::hex << result;
        return false;
    }
    return true;
}

bool MainWindowForWindows::moveToTrash(QString path)
{
    // IFileOperation is the shell's file operation; the call it replaces,
    // SHFileOperation, needed a fixed-size path buffer built by hand. Qt has
    // already put COM in apartment-threaded mode on this thread.
    IFileOperation *operation = nullptr;
    HRESULT result =
        ::CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&operation));
    if (FAILED(result)) {
        qWarning() << "Could not start a file operation:" << Qt::hex << result;
        return false;
    }

    operation->SetOperationFlags(FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT);

    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    IShellItem *item = nullptr;
    result = ::SHCreateItemFromParsingName(nativePath.c_str(), nullptr, IID_PPV_ARGS(&item));
    if (SUCCEEDED(result)) {
        result = operation->DeleteItem(item, nullptr);
        item->Release();
    }
    BOOL aborted = FALSE;
    if (SUCCEEDED(result)) {
        result = operation->PerformOperations();
    }
    if (SUCCEEDED(result)) {
        operation->GetAnyOperationsAborted(&aborted);
    }
    operation->Release();

    if (FAILED(result) || aborted) {
        qWarning() << "Could not move" << path << "to the Recycle Bin:" << Qt::hex << result;
        return false;
    }
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
    const bool inputAttached = !signalOnly && foregroundThreadId != 0 &&
                               foregroundThreadId != currentThreadId &&
                               ::AttachThreadInput(currentThreadId, foregroundThreadId, TRUE);

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
