#include "startupsettings.h"

#ifdef Q_OS_WIN
#    include <iterator>
#    include <string>

#    include <Windows.h>

bool StartupSettings::useDirect2D(const QString &settingsFilePath)
{
    const std::wstring path = settingsFilePath.toStdWString();
    // The size is in characters; a longer value is cut to fit and so is not
    // "true".
    WCHAR value[128];
    ::GetPrivateProfileStringW(L"View",
                               L"UseDirect2D",
                               L"false",
                               value,
                               static_cast<DWORD>(std::size(value)),
                               path.c_str());
    return ::lstrcmpW(value, L"true") == 0;
}
#endif
