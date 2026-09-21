# Windows 10 version 1809 (build 17763) is the oldest Windows QuickViewer
# supports, and Qt 6 has the same floor. The Windows headers pick their API
# surface from these macros, so the projects that build Windows code share one
# definition instead of each choosing their own.
win32 {
    DEFINES += _WIN32_WINNT=0x0A00 NTDDI_VERSION=NTDDI_WIN10_RS5
}
