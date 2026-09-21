
# Constant Macros
!define APPNAME "QuickViewer"
!define AppDir "QuickViewer"
#!define APPVERSION "1.2.6"
!define COMPANYNAME "QuickViewer contributors"
# The staged tree of a build made without QV_PORTABLE: that build keeps its
# settings in the user's data directory, which is what an installed copy does.
!define SrcDir "M:\Home\src\qt\QVproject\QuickViewer-${APPVERSION}\x64"

# rtf or txt file - remember if it is txt, it must be in the DOS text format (\r\n)
LicenseData "${SrcDir}\LICENSE"
# other language resourses
LoadLanguageFile "${NSISDIR}\Contrib\Language files\Japanese.nlf"
# App Name
Name "QuickViewer"
# Installer Exectable
OutFile "..\..\..\${APPNAME}-Installer-${APPVERSION}.exe"
# Installed Directory Path
InstallDir "$PROGRAMFILES64\${AppDir}"
 
!include LogicLib.nsh

# File associations. The application registers the same keys for the current
# user when formats are chosen in its own dialog; a per-machine installation
# registers them for every user, which the application cannot do without
# elevation.
!macro AssocProgId PROGID DESCRIPTION ICON
  WriteRegStr HKLM "Software\Classes\${PROGID}" "" "${DESCRIPTION}"
  !if "${ICON}" != ""
    WriteRegStr HKLM "Software\Classes\${PROGID}\DefaultIcon" "" "$INSTDIR\iconengines\${ICON}"
  !endif
  WriteRegStr HKLM "Software\Classes\${PROGID}\shell\open\command" "" '"$INSTDIR\QuickViewer.exe" "%1"'
!macroend

!macro AssocExtension EXTENSION PROGID
  WriteRegStr HKLM "Software\QuickViewer\Capabilities\FileAssociations" "${EXTENSION}" "${PROGID}"
!macroend

!macro DeleteAssocProgId PROGID
  DeleteRegKey HKLM "Software\Classes\${PROGID}"
!macroend

# Just three pages - license agreement, install location, and installation
page license
page directory
Page instfiles

# Defailt Section(install)
Section
  # Set destination dir and specify files
  SetOutPath "$INSTDIR"
  File "${SrcDir}\QuickViewer.exe"
  File "${SrcDir}\7z.dll"
  File "${SrcDir}\concrt140.dll"
  File "${SrcDir}\heif.dll"
  File "${SrcDir}\libde265.dll"
  File "${SrcDir}\openjp2.dll"
  File "${SrcDir}\LICENSE"
  File "${SrcDir}\msvcp140.dll"
  File "${SrcDir}\Qt6Core.dll"
  File "${SrcDir}\Qt6Gui.dll"
  File "${SrcDir}\Qt6Network.dll"
  File "${SrcDir}\Qt6Sql.dll"
  File "${SrcDir}\Qt6Svg.dll"
  File "${SrcDir}\Qt6Widgets.dll"
  File "${SrcDir}\README.md"
  File "${SrcDir}\vccorlib140.dll"
  File "${SrcDir}\vcruntime140.dll"

  SetOutPath "$INSTDIR\database"
  File "${SrcDir}\database\schema.sql"
  File "${SrcDir}\database\thumbnail.sqlite3.db"

  SetOutPath "$INSTDIR\generic"
  File "${SrcDir}\generic\qtuiotouchplugin.dll"

  SetOutPath "$INSTDIR\iconengines"
  File "${SrcDir}\iconengines\qsvgicon.dll"
  File "${SrcDir}\iconengines\qv_apng.ico"
  File "${SrcDir}\iconengines\qv_bmp.ico"
  File "${SrcDir}\iconengines\qv_dds.ico"
  File "${SrcDir}\iconengines\qv_gif.ico"
  File "${SrcDir}\iconengines\qv_icon.ico"
  File "${SrcDir}\iconengines\qv_jpeg.ico"
  File "${SrcDir}\iconengines\qv_png.ico"
  File "${SrcDir}\iconengines\qv_raw.ico"
  File "${SrcDir}\iconengines\qv_tga.ico"
  File "${SrcDir}\iconengines\qv_tiff.ico"
  File "${SrcDir}\iconengines\qv_webp.ico"

  SetOutPath "$INSTDIR\imageformats"
  File "${SrcDir}\imageformats\qapng.dll"
  File "${SrcDir}\imageformats\qavif6.dll"
  File "${SrcDir}\imageformats\qdds.dll"
  File "${SrcDir}\imageformats\qgif.dll"
  File "${SrcDir}\imageformats\kimg_heif6.dll"
  File "${SrcDir}\imageformats\qicns.dll"
  File "${SrcDir}\imageformats\qico.dll"
  File "${SrcDir}\imageformats\qjp2.dll"
  File "${SrcDir}\imageformats\qjpeg.dll"
  File "${SrcDir}\imageformats\qjpegxr.dll"
  File "${SrcDir}\imageformats\qrawspeed0.dll"
  File "${SrcDir}\imageformats\qsvg.dll"
  File "${SrcDir}\imageformats\qtga.dll"
  File "${SrcDir}\imageformats\qtiff.dll"
  File "${SrcDir}\imageformats\qwbmp.dll"
  File "${SrcDir}\imageformats\qwebp.dll"

  SetOutPath "$INSTDIR\networkinformation"
  File "${SrcDir}\networkinformation\qnetworklistmanager.dll"

  SetOutPath "$INSTDIR\platforms"
  File "${SrcDir}\platforms\qdirect2d.dll"
  File "${SrcDir}\platforms\qwindows.dll"

  SetOutPath "$INSTDIR\sqldrivers"
  File "${SrcDir}\sqldrivers\qsqlibase.dll"
  File "${SrcDir}\sqldrivers\qsqlite.dll"
  File "${SrcDir}\sqldrivers\qsqlmimer.dll"
  File "${SrcDir}\sqldrivers\qsqloci.dll"
  File "${SrcDir}\sqldrivers\qsqlodbc.dll"
  File "${SrcDir}\sqldrivers\qsqlpsql.dll"

  SetOutPath "$INSTDIR\styles"
  File "${SrcDir}\styles\qmodernwindowsstyle.dll"

  SetOutPath "$INSTDIR\tls"
  File "${SrcDir}\tls\qcertonlybackend.dll"
  File "${SrcDir}\tls\qschannelbackend.dll"

  SetOutPath "$INSTDIR\translations"
  File "${SrcDir}\translations\Arabic.txt"
  File "${SrcDir}\translations\Chinese.txt"
  File "${SrcDir}\translations\English.txt"
  File "${SrcDir}\translations\French.txt"
  File "${SrcDir}\translations\Greek.txt"
  File "${SrcDir}\translations\Japanese.txt"
  File "${SrcDir}\translations\languages.ini"
  File "${SrcDir}\translations\qt_ar.qm"
  File "${SrcDir}\translations\qt_bg.qm"
  File "${SrcDir}\translations\qt_ca.qm"
  File "${SrcDir}\translations\qt_cs.qm"
  File "${SrcDir}\translations\qt_da.qm"
  File "${SrcDir}\translations\qt_de.qm"
  File "${SrcDir}\translations\qt_el.qm"
  File "${SrcDir}\translations\qt_en.qm"
  File "${SrcDir}\translations\qt_es.qm"
  File "${SrcDir}\translations\qt_fa.qm"
  File "${SrcDir}\translations\qt_fi.qm"
  File "${SrcDir}\translations\qt_fr.qm"
  File "${SrcDir}\translations\qt_gd.qm"
  File "${SrcDir}\translations\qt_he.qm"
  File "${SrcDir}\translations\qt_hr.qm"
  File "${SrcDir}\translations\qt_hu.qm"
  File "${SrcDir}\translations\qt_it.qm"
  File "${SrcDir}\translations\qt_ja.qm"
  File "${SrcDir}\translations\qt_ka.qm"
  File "${SrcDir}\translations\qt_ko.qm"
  File "${SrcDir}\translations\qt_lv.qm"
  File "${SrcDir}\translations\qt_nl.qm"
  File "${SrcDir}\translations\qt_nn.qm"
  File "${SrcDir}\translations\qt_pl.qm"
  File "${SrcDir}\translations\qt_pt_BR.qm"
  File "${SrcDir}\translations\qt_ru.qm"
  File "${SrcDir}\translations\qt_sk.qm"
  File "${SrcDir}\translations\qt_tr.qm"
  File "${SrcDir}\translations\qt_uk.qm"
  File "${SrcDir}\translations\qt_zh_CN.qm"
  File "${SrcDir}\translations\qt_zh_TW.qm"
  File "${SrcDir}\translations\quickviewer_ar.qm"
  File "${SrcDir}\translations\quickviewer_el.qm"
  File "${SrcDir}\translations\quickviewer_es.qm"
  File "${SrcDir}\translations\quickviewer_fr.qm"
  File "${SrcDir}\translations\quickviewer_ja.qm"
  File "${SrcDir}\translations\quickviewer_ru.qm"
  File "${SrcDir}\translations\quickviewer_zh.qm"
  File "${SrcDir}\translations\Reverse.txt"
  File "${SrcDir}\translations\Russian.txt"
  File "${SrcDir}\translations\Spanish.txt"
  File "${SrcDir}\translations\YourLanguage.txt"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  # Create a shortcut on start menu
  CreateDirectory "$SMPROGRAMS\${AppDir}"
  SetOutPath "$INSTDIR"
  CreateShortcut "$SMPROGRAMS\${AppDir}\QuickViewer.lnk" "$INSTDIR\QuickViewer.exe" ""
  # write registry entries for uninstalling
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "QuickViewer"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayIcon" "$\"$INSTDIR\QuickViewer.exe$\""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Publisher" "${COMPANYNAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayVersion" "${APPVERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" '"$INSTDIR\Uninstall.exe"'

  # Register the formats every user can pick in Default Apps. The entries match
  # the ones the application writes for a single user under HKEY_CURRENT_USER.
  WriteRegStr HKLM "Software\QuickViewer\Capabilities" "ApplicationName" "${APPNAME}"
  WriteRegStr HKLM "Software\QuickViewer\Capabilities" "ApplicationDescription" "Ultra-fast image and comic viewer"
  WriteRegStr HKLM "Software\RegisteredApplications" "${APPNAME}" "Software\QuickViewer\Capabilities"
  WriteRegStr HKLM "Software\Classes\Applications\QuickViewer.exe" "FriendlyAppName" "${APPNAME}"
  WriteRegStr HKLM "Software\Classes\Applications\QuickViewer.exe\shell\open\command" "" '"$INSTDIR\QuickViewer.exe" "%1"'

  !insertmacro AssocProgId "QuickViewer.AssocFile.Jpeg" "JPEG Image" "qv_jpeg.ico"
  !insertmacro AssocExtension ".jpg" "QuickViewer.AssocFile.Jpeg"
  !insertmacro AssocExtension ".jpeg" "QuickViewer.AssocFile.Jpeg"
  !insertmacro AssocExtension ".jpe" "QuickViewer.AssocFile.Jpeg"

  !insertmacro AssocProgId "QuickViewer.AssocFile.Png" "PNG File" "qv_png.ico"
  !insertmacro AssocExtension ".png" "QuickViewer.AssocFile.Png"

  !insertmacro AssocProgId "QuickViewer.AssocFile.Tga" "Truevision Graphics Adapter Format Image" "qv_tga.ico"
  !insertmacro AssocExtension ".tga" "QuickViewer.AssocFile.Tga"

  !insertmacro AssocProgId "QuickViewer.AssocFile.Apng" "Animated PNG File" "qv_apng.ico"
  !insertmacro AssocExtension ".apng" "QuickViewer.AssocFile.Apng"

  !insertmacro AssocProgId "QuickViewer.AssocFile.Bitmap" "Bitmap File" "qv_bmp.ico"
  !insertmacro AssocExtension ".bmp" "QuickViewer.AssocFile.Bitmap"

  !insertmacro AssocProgId "QuickViewer.AssocFile.Dds" "DirectDraw Surface Image" "qv_dds.ico"
  !insertmacro AssocExtension ".dds" "QuickViewer.AssocFile.Dds"

  !insertmacro AssocProgId "QuickViewer.AssocFile.Gif" "GIF Image" "qv_gif.ico"
  !insertmacro AssocExtension ".gif" "QuickViewer.AssocFile.Gif"

  !insertmacro AssocProgId "QuickViewer.AssocFile.Icon" "Windows Icon File" ""
  !insertmacro AssocExtension ".ico" "QuickViewer.AssocFile.Icon"

  !insertmacro AssocProgId "QuickViewer.AssocFile.Tiff" "TIFF image" "qv_tiff.ico"
  !insertmacro AssocExtension ".tif" "QuickViewer.AssocFile.Tiff"
  !insertmacro AssocExtension ".tiff" "QuickViewer.AssocFile.Tiff"

  !insertmacro AssocProgId "QuickViewer.AssocFile.WebP" "WebP Image" "qv_webp.ico"
  !insertmacro AssocExtension ".webp" "QuickViewer.AssocFile.WebP"

  !insertmacro AssocProgId "QuickViewer.AssocFile.Heif" "HEIF Image" ""
  !insertmacro AssocExtension ".heic" "QuickViewer.AssocFile.Heif"
  !insertmacro AssocExtension ".heif" "QuickViewer.AssocFile.Heif"

  !insertmacro AssocProgId "QuickViewer.AssocFile.RawCanon" "Canon RAW format" "qv_raw.ico"
  !insertmacro AssocExtension ".crw" "QuickViewer.AssocFile.RawCanon"
  !insertmacro AssocExtension ".cr2" "QuickViewer.AssocFile.RawCanon"

  !insertmacro AssocProgId "QuickViewer.AssocFile.RawDng" "Adobe Digital Negative Format" "qv_raw.ico"
  !insertmacro AssocExtension ".dng" "QuickViewer.AssocFile.RawDng"

  !insertmacro AssocProgId "QuickViewer.AssocFile.RawNicon" "Nikon RAW format" "qv_raw.ico"
  !insertmacro AssocExtension ".nef" "QuickViewer.AssocFile.RawNicon"

  !insertmacro AssocProgId "QuickViewer.AssocFile.RawSony" "Sony RAW format" "qv_raw.ico"
  !insertmacro AssocExtension ".arw" "QuickViewer.AssocFile.RawSony"
SectionEnd


# uninstall section
Section "Uninstall"
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$INSTDIR\QuickViewer.exe"
  RMDir /r "$INSTDIR"
  # remove start menu shortcut
  Delete "$SMPROGRAMS\${AppDir}\QuickViewer.lnk"
  RMDir "$SMPROGRAMS\${AppDir}"
  # remove registries
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
  # remove the file associations written by the install section
  DeleteRegValue HKLM "Software\RegisteredApplications" "${APPNAME}"
  DeleteRegKey HKLM "Software\QuickViewer"
  DeleteRegKey HKLM "Software\Classes\Applications\QuickViewer.exe"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.Jpeg"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.Png"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.Tga"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.Apng"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.Bitmap"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.Dds"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.Gif"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.Icon"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.Tiff"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.WebP"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.Heif"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.RawCanon"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.RawDng"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.RawNicon"
  !insertmacro DeleteAssocProgId "QuickViewer.AssocFile.RawSony"
SectionEnd
