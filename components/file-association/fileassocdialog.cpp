#include "fileassocdialog.h"
#include "ui_fileassocdialog.h"
#include <QDesktopServices>
#include <QOperatingSystemVersion>
#include <windows.h>

// Registration is per user: Windows lets a program register the formats it can
// handle without asking for elevation, and the machine-wide entries of the
// installed package are written by its installer instead.
#define APPLICATION_ID "QuickViewer"
#define REGKEYFORMAT_ASSOCFILE APPLICATION_ID ".AssocFile.%1"
#define REGKEY_SOFTWARE "HKEY_CURRENT_USER\\Software"
#define REGKEY_CLASSES REGKEY_SOFTWARE "\\Classes"
#define REGKEYFORMAT_ASSOCPATH REGKEY_CLASSES "\\" APPLICATION_ID ".AssocFile.%1"
#define REGKEY_REGISTEREDAPPLICATIONS REGKEY_SOFTWARE "\\RegisteredApplications"
#define REGKEY_APPLICATION REGKEY_SOFTWARE "\\" APPLICATION_ID
#define REGKEY_APPLICATION_INAPP REGKEY_CLASSES "\\Applications\\" APPLICATION_ID ".exe"

#ifdef WIN64
QSettings::Format FileAssocDialog::RegFormat = QSettings::Registry64Format;
#else
QSettings::Format FileAssocDialog::RegFormat = QSettings::Registry32Format;
#endif

FileAssocDialog::FileAssocDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::FileAssocDialog)
{
    ui->setupUi(this);

    m_assocOfActions["Jpeg"] = ui->checkBoxJpeg;
    m_assocs["Jpeg"] = AssocInfo{"Jpeg",
                                 tr("JPEG Image", "description of File format on Explorer(.jpeg)"),
                                 "qv_jpeg.ico",
                                 {".jpg", ".jpeg", ".jpe"}};

    m_assocOfActions["Png"] = ui->checkBoxPng;
    m_assocs["Png"] = AssocInfo{"Png",
                                tr("PNG File", "description of File format on Explorer(.png)"),
                                "qv_png.ico",
                                {".png"}};

    m_assocOfActions["Tga"] = ui->checkBoxTga;
    m_assocs["Tga"] = AssocInfo{"Tga",
                                tr("Truevision Graphics Adapter Format Image",
                                   "description of File format on Explorer(.tga)"),
                                "qv_tga.ico",
                                {".tga"}};

    m_assocOfActions["Apng"] = ui->checkBoxApng;
    m_assocs["Apng"] =
        AssocInfo{"Apng",
                  tr("Animated PNG File", "description of File format on Explorer(.apng)"),
                  "qv_apng.ico",
                  {".apng"}};

    m_assocOfActions["Bitmap"] = ui->checkBoxBmp;
    m_assocs["Bitmap"] =
        AssocInfo{"Bitmap",
                  tr("Bitmap File", "description of File format on Explorer(.bmp)"),
                  "qv_bmp.ico",
                  {".bmp"}};

    m_assocOfActions["Dds"] = ui->checkBoxDds;
    m_assocs["Dds"] =
        AssocInfo{"Dds",
                  tr("DirectDraw Surface Image", "description of File format on Explorer(.dds)"),
                  "qv_dds.ico",
                  {".dds"}};

    m_assocOfActions["Gif"] = ui->checkBoxGif;
    m_assocs["Gif"] = AssocInfo{"Gif",
                                tr("GIF Image", "description of File format on Explorer(.gif)"),
                                "qv_gif.ico",
                                {".gif"}};

    m_assocOfActions["Icon"] = ui->checkBoxIcon;
    m_assocs["Icon"] =
        AssocInfo{"Icon",
                  tr("Windows Icon File", "description of File format on Explorer(.ico)"),
                  "",
                  {".ico"}};

    m_assocOfActions["Tiff"] = ui->checkBoxTiff;
    m_assocs["Tiff"] = AssocInfo{"Tiff",
                                 tr("TIFF image", "description of File format on Explorer(.tiff)"),
                                 "qv_tiff.ico",
                                 {".tif", ".tiff"}};

    m_assocOfActions["WebP"] = ui->checkBoxWebp;
    m_assocs["WebP"] = AssocInfo{"WebP",
                                 tr("WebP Image", "description of File format on Explorer(.webp)"),
                                 "qv_webp.ico",
                                 {".webp"}};

    m_assocOfActions["Heif"] = ui->checkBoxHeif;
    m_assocs["Heif"] =
        AssocInfo{"Heif",
                  tr("HEIF Image", "description of File format on Explorer(.heic, .heif)"),
                  "",
                  {".heic", ".heif"}};

    m_assocOfActions["RawCanon"] = ui->checkBoxRawCanon;
    m_assocs["RawCanon"] =
        AssocInfo{"RawCanon",
                  tr("Canon RAW format", "description of File format on Explorer(.cr2)"),
                  "qv_raw.ico",
                  {".crw", ".cr2"}};

    m_assocOfActions["RawDng"] = ui->checkBoxRawDng;
    m_assocs["RawDng"] = AssocInfo{
        "RawDng",
        tr("Adobe Digital Negative Format", "description of File format on Explorer(.dng)"),
        "qv_raw.ico",
        {".dng"}};

    m_assocOfActions["RawNicon"] = ui->checkBoxRawNicon;
    m_assocs["RawNicon"] =
        AssocInfo{"RawNicon",
                  tr("Nikon RAW format", "description of File format on Explorer(.nef)"),
                  "qv_raw.ico",
                  {".nef"}};

    m_assocOfActions["RawSony"] = ui->checkBoxRawSony;
    m_assocs["RawSony"] =
        AssocInfo{"RawSony",
                  tr("Sony RAW format", "description of File format on Explorer(.arw)"),
                  "qv_raw.ico",
                  {".arw"}};

    {
        // check on if assoiation exists for each extension
        for (const QString &fmt : m_assocOfActions.keys()) {
            QSettings settings(REGKEY_CLASSES, RegFormat);
            settings.beginGroup(QString(REGKEYFORMAT_ASSOCFILE).arg(fmt));
            if (!settings.allKeys().isEmpty()) {
                m_assocOfActions[fmt]->setChecked(true);
            }
            settings.endGroup();
        }
    }
}

FileAssocDialog::~FileAssocDialog()
{
    delete ui;
}

//void FileAssocDialog::closeEvent(QCloseEvent *event)
//{
//    QDialog::closeEvent(event);
//    if(result() == QDialog::Accepted) {
//        auto formats = enumrateFormats();
//        if(formats.isEmpty())
//            unregisterEntries();
//        else
//            registerEntries(formats);
//    }

//    emit closed();
//}

QStringList FileAssocDialog::enumrateFormats()
{
    QStringList result;
    for (const QString &fmt : m_assocOfActions.keys()) {
        QCheckBox *c = m_assocOfActions[fmt];
        if (c && c->isChecked()) {
            result << fmt;
        }
    }

    return result;
}

void FileAssocDialog::handleAllOnButtonClicked()
{
    for (QCheckBox *c : m_assocOfActions.values()) {
        if (c) {
            c->setChecked(true);
        }
    }
}

void FileAssocDialog::handleAllOffButtonClicked()
{
    for (QCheckBox *c : m_assocOfActions.values()) {
        if (c) {
            c->setChecked(false);
        }
    }
}

void FileAssocDialog::handleButtonBoxAccepted()
{
    auto formats = enumrateFormats();
    if (formats.isEmpty()) {
        unregisterEntries();
    } else {
        registerEntries(formats);
    }
    accept();
    return;
}

void FileAssocDialog::registerEntries(QStringList formats)
{
    // The selection is the wanted state of the registration, so a format the
    // user cleared is removed here instead of being left registered.
    for (const QString &fmt : m_assocs.keys()) {
        if (!formats.contains(fmt)) {
            unregisterFormat(fmt);
        }
    }
    for (const QString &fmt : formats) {
        registerFormat(fmt);
    }
    writeCapabilities(formats);
    openAssociationSettings();
}

void FileAssocDialog::registerFormat(const QString &fmt)
{
    const AssocInfo &info = m_assocs[fmt];

    // Association for one extension: its ProgID carries the description, the
    // icon and the command that Explorer shows.
    QSettings settings(REGKEY_CLASSES, RegFormat);
    settings.beginGroup(QString(REGKEYFORMAT_ASSOCFILE).arg(fmt));
    settings.setValue(".", info.Description);
    if (!info.IconName.isEmpty()) {
        settings.beginGroup("DefaultIcon");
        settings.setValue(".", getIconPath(info.IconName));
        settings.endGroup();
    }
    settings.beginGroup("shell");
    settings.beginGroup("open");
    settings.setValue(
        ".",
        tr("&View with QuickViewer", "Menu displayed when right clicking on file in Explorer"));
    settings.beginGroup("command");
    settings.setValue(".", getExecuteApplication());
    settings.endGroup();
    settings.endGroup();
    settings.endGroup();
    settings.endGroup();
    settings.sync();
}

void FileAssocDialog::unregisterFormat(const QString &fmt)
{
    QSettings settings(QString(REGKEYFORMAT_ASSOCPATH).arg(fmt), RegFormat);
    settings.clear();
    settings.sync();
}

void FileAssocDialog::writeCapabilities(const QStringList &formats)
{
    {
        // QuickViewer Capabilities: the list of extensions is rebuilt, so the
        // ones the user dropped stop being offered.
        QSettings settings(REGKEY_APPLICATION, RegFormat);
        settings.remove("Capabilities");
        settings.beginGroup("Capabilities");
        settings.setValue("ApplicationDescription", "Ultra-fast image and comic viewer");
        settings.setValue("ApplicationName", APPLICATION_ID);
        settings.beginGroup("FileAssociations");
        for (const QString &fmt : formats) {
            for (const QString &ext : m_assocs[fmt].Extensions) {
                settings.setValue(ext, QString(REGKEYFORMAT_ASSOCFILE).arg(fmt));
            }
        }
        settings.endGroup();
        settings.endGroup();
        settings.sync();
    }
    {
        // RegisteredApplications
        QSettings settings2(REGKEY_REGISTEREDAPPLICATIONS, RegFormat);
        settings2.setValue(APPLICATION_ID, "Software\\" APPLICATION_ID "\\Capabilities");
        settings2.sync();
    }
    {
        // assoiation for application
        QSettings settings(REGKEY_APPLICATION_INAPP, RegFormat);
        settings.setValue("FriendlyAppName", APPLICATION_ID);

        settings.beginGroup("shell");
        settings.beginGroup("open");
        settings.setValue(
            ".",
            tr("&View with QuickViewer", "Menu displayed when right clicking on file in Explorer"));
        settings.beginGroup("command");
        settings.setValue(".", getExecuteApplication());
        settings.endGroup();
        settings.endGroup();
        settings.endGroup();
        settings.sync();
    }
}

void FileAssocDialog::unregisterEntries()
{
    for (const QString &fmt : m_assocs.keys()) {
        unregisterFormat(fmt);
    }
    {
        // QuickViewer Capabilities
        QSettings settings(REGKEY_APPLICATION, RegFormat);
        settings.clear();
        settings.sync();
    }
    {
        // RegisteredApplications
        QSettings settings(REGKEY_REGISTEREDAPPLICATIONS, RegFormat);
        settings.remove(APPLICATION_ID);
        settings.sync();
    }
    {
        // assoiation for application
        QSettings settings(REGKEY_APPLICATION_INAPP, RegFormat);
        settings.clear();
        settings.sync();
    }
}

void FileAssocDialog::openAssociationSettings()
{
    // Windows 10 ignores the association UI that older builds opened and only
    // lets the user change a format through Settings. Windows 11 can open the
    // page of one registered application directly; the page itself works on
    // both, so it is the fallback.
    QString settings = QStringLiteral("ms-settings:defaultapps");
    const QOperatingSystemVersion windows11(QOperatingSystemVersion::Windows, 10, 0, 22000);
    if (QOperatingSystemVersion::current() >= windows11) {
        settings += QStringLiteral("?registeredAppUser=" APPLICATION_ID);
    }
    QDesktopServices::openUrl(QUrl(settings));
}

QString FileAssocDialog::getExecuteApplication()
{
    return QString("\"%1\\" APPLICATION_ID ".exe\" \"%2\"")
        .arg(QDir::toNativeSeparators(qApp->applicationDirPath()))
        .arg("%1");
}

QString FileAssocDialog::getIconPath(QString iconName)
{
    return QString("\"%1\\iconengines\\%2\"")
        .arg(QDir::toNativeSeparators(qApp->applicationDirPath()))
        .arg(iconName);
}
