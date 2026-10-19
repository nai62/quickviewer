#ifndef STARTUPSETTINGS_H
#define STARTUPSETTINGS_H

#include <QString>

#ifdef Q_OS_WIN
/**
 * Settings main() needs before QApplication exists, read from quickviewer.ini
 * with the Windows profile API because QSettings is not usable yet.
 */
namespace StartupSettings {

/**
 * Whether the settings file asks for the direct2d QPA plug-in.
 */
bool useDirect2D(const QString &settingsFilePath);

} // namespace StartupSettings
#endif

#endif // STARTUPSETTINGS_H
