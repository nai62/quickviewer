#include <QtTest>

#include <utility>

#include <QAbstractButton>
#include <QAction>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QTextEdit>
#include <QTranslator>

#include "catalogwindow.h"
#include "exifdialog.h"
#include "folderwindow.h"
#include "mainwindow.h"
#include "models/qvapplication.h"
#include "retouchwindow.h"

class DecoratingTranslator : public QTranslator
{
public:
    DecoratingTranslator(QString prefix, QString suffix)
        : m_prefix(std::move(prefix)),
          m_suffix(std::move(suffix))
    {}

    QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr, int n = -1) const override
    {
        Q_UNUSED(context);
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        if (sourceText == nullptr || sourceText[0] == '\0') {
            return QString();
        }
        return m_prefix + QString::fromUtf8(sourceText) + m_suffix;
    }

private:
    QString m_prefix;
    QString m_suffix;
};

class TranslatorGuard
{
public:
    ~TranslatorGuard() { clear(); }

    bool set(QTranslator *translator)
    {
        clear();
        if (translator == nullptr) {
            return true;
        }
        if (!qApp->installTranslator(translator)) {
            return false;
        }
        m_translator = translator;
        return true;
    }

    void clear()
    {
        if (m_translator != nullptr) {
            qApp->removeTranslator(m_translator);
            m_translator = nullptr;
        }
    }

private:
    QTranslator *m_translator = nullptr;
};

class LanguageSwitchWindow : public MainWindow
{
public:
    Ui::MainWindow *mainUi() const { return ui; }
    QToolButton *fullscreenButton() const { return m_fullscreenButton; }
};

static QString marker(const QString &source)
{
    return QStringLiteral("[[%1]]").arg(source);
}

static bool emitLanguageChanged(const QString &language)
{
    return QMetaObject::invokeMethod(
        qApp->languageSelector(),
        "languageChanged",
        Qt::DirectConnection,
        Q_ARG(QString, language));
}

static bool containsJapanese(const QString &text)
{
    for (const QChar ch : text) {
        const ushort code = ch.unicode();
        if ((code >= 0x3040 && code <= 0x30ff) ||
            (code >= 0x3400 && code <= 0x4dbf) ||
            (code >= 0x4e00 && code <= 0x9fff)) {
            return true;
        }
    }
    return false;
}

static bool isContentDerivedLabel(const QLabel *label)
{
    const QString name = label->objectName();
    return name == QStringLiteral("pageLabel") || name == QStringLiteral("pathLabel");
}

static void appendText(QStringList &texts, QObject *object, const QString &property, const QString &text)
{
    if (text.isEmpty()) {
        return;
    }
    const QString owner = object->objectName().isEmpty()
                              ? QString::fromLatin1(object->metaObject()->className())
                              : object->objectName();
    texts << QStringLiteral("%1.%2: %3").arg(owner, property, text);
}

static QStringList relevantUiText(QObject *root)
{
    QStringList texts;
    QList<QObject *> objects;
    objects << root;
    objects.append(root->findChildren<QObject *>());

    for (QObject *object : objects) {
        if (auto *action = qobject_cast<QAction *>(object)) {
            appendText(texts, action, QStringLiteral("text"), action->text());
            appendText(texts, action, QStringLiteral("iconText"), action->iconText());
            appendText(texts, action, QStringLiteral("toolTip"), action->toolTip());
            appendText(texts, action, QStringLiteral("statusTip"), action->statusTip());
            appendText(texts, action, QStringLiteral("whatsThis"), action->whatsThis());
        }
        if (auto *widget = qobject_cast<QWidget *>(object)) {
            appendText(texts, widget, QStringLiteral("toolTip"), widget->toolTip());
            appendText(texts, widget, QStringLiteral("statusTip"), widget->statusTip());
            appendText(texts, widget, QStringLiteral("whatsThis"), widget->whatsThis());
            if (widget->isWindow()) {
                appendText(texts, widget, QStringLiteral("windowTitle"), widget->windowTitle());
            }
        }
        if (auto *button = qobject_cast<QAbstractButton *>(object)) {
            appendText(texts, button, QStringLiteral("text"), button->text());
        }
        if (auto *label = qobject_cast<QLabel *>(object)) {
            if (!isContentDerivedLabel(label)) {
                appendText(texts, label, QStringLiteral("text"), label->text());
            }
        }
        if (auto *lineEdit = qobject_cast<QLineEdit *>(object)) {
            appendText(texts, lineEdit, QStringLiteral("placeholderText"), lineEdit->placeholderText());
        }
        if (auto *menu = qobject_cast<QMenu *>(object)) {
            appendText(texts, menu, QStringLiteral("title"), menu->title());
        }
        if (auto *groupBox = qobject_cast<QGroupBox *>(object)) {
            appendText(texts, groupBox, QStringLiteral("title"), groupBox->title());
        }
        if (auto *combo = qobject_cast<QComboBox *>(object)) {
            if (!combo->isEditable()) {
                for (int i = 0; i < combo->count(); ++i) {
                    appendText(texts, combo, QStringLiteral("itemText"), combo->itemText(i));
                }
            }
        }
    }
    return texts;
}

class LanguageSwitchTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        qApp->keyActions().clearActionGroups();
        qApp->mouseActions().clearActionGroups();
        qApp->setCatalogViewModeSetting(qvEnums::List);
        qApp->setShowTagBar(true);
    }

    void loadBookmarkRetranslatesAfterLanguageSwitch()
    {
        LanguageSwitchWindow viewer;
        auto *loadAction = viewer.findChild<QAction *>(QStringLiteral("actionLoadBookmark"));
        auto *loadMenu = viewer.findChild<QMenu *>(QStringLiteral("menuLoadBookmark"));
        QVERIFY(loadAction != nullptr);
        QVERIFY(loadMenu != nullptr);

        DecoratingTranslator first(QStringLiteral("{{"), QStringLiteral("}}"));
        DecoratingTranslator second(QStringLiteral("[["), QStringLiteral("]]"));
        TranslatorGuard translator;

        QVERIFY(translator.set(&first));
        QVERIFY(emitLanguageChanged(QStringLiteral("First test language")));
        QCOMPARE(loadAction->text(), QStringLiteral("{{Load bookmark}}"));
        QCOMPARE(loadMenu->title(), QStringLiteral("{{Load bookmark}}"));

        QVERIFY(translator.set(&second));
        QVERIFY(emitLanguageChanged(QStringLiteral("Second test language")));
        QCOMPARE(loadAction->text(), marker(QStringLiteral("Load bookmark")));
        // The removed regression translated the stale source "LoadBookmark" here.
        QCOMPARE(loadMenu->title(), marker(QStringLiteral("Load bookmark")));

        translator.clear();
        QVERIFY(emitLanguageChanged(QStringLiteral("English")));
    }

    void existingUiSurfacesRetranslateWithDeterministicTranslator()
    {
        LanguageSwitchWindow viewer;
        FolderWindow folder(nullptr, nullptr);
        CatalogWindow catalog(nullptr, viewer.mainUi());
        ExifDialog exif;
        RetouchWindow retouch(nullptr);

        ImageContent content;
        content.path = QStringLiteral("/tmp/language-switch.jpg");
        content.originalSize = QSize(640, 480);
        content.exifInfo.ImageWidth = 640;
        content.exifInfo.ImageHeight = 480;
        content.exifInfo.Orientation = 1;
        exif.setExif(content);

        auto *loadAction = viewer.findChild<QAction *>(QStringLiteral("actionLoadBookmark"));
        auto *fileMenu = viewer.findChild<QMenu *>(QStringLiteral("menuFile"));
        auto *statusLabel = viewer.findChild<QLabel *>(QStringLiteral("statusLabel"));
        QVERIFY(loadAction != nullptr);
        QVERIFY(fileMenu != nullptr);
        QVERIFY(statusLabel != nullptr);
        QVERIFY(viewer.fullscreenButton() != nullptr);

        auto *folderLabel = folder.findChild<QLabel *>(QStringLiteral("label"));
        auto *folderHome = folder.findChild<QAbstractButton *>(QStringLiteral("homeButton"));
        auto *catalogSearch = catalog.findChild<QComboBox *>(QStringLiteral("searchCombo"));
        auto *catalogStatus = catalog.findChild<QLabel *>(QStringLiteral("statusLabel"));
        auto *catalogViewMenu = catalog.findChild<QMenu *>(QStringLiteral("menu_View"));
        auto *exifClipboard = exif.findChild<QAbstractButton *>(QStringLiteral("btnClipboard"));
        auto *exifText = exif.findChild<QTextEdit *>(QStringLiteral("textEdit"));
        auto *retouchBrightness = retouch.findChild<QLabel *>(QStringLiteral("label"));
        auto *retouchReset = retouch.findChild<QAbstractButton *>(QStringLiteral("resetButton"));
        QVERIFY(folderLabel != nullptr);
        QVERIFY(folderHome != nullptr);
        QVERIFY(catalogSearch != nullptr);
        QVERIFY(catalogSearch->lineEdit() != nullptr);
        QVERIFY(catalogStatus != nullptr);
        QVERIFY(catalogViewMenu != nullptr);
        QVERIFY(exifClipboard != nullptr);
        QVERIFY(exifText != nullptr);
        QVERIFY(retouchBrightness != nullptr);
        QVERIFY(retouchReset != nullptr);

        DecoratingTranslator markerTranslator(QStringLiteral("[["), QStringLiteral("]]"));
        TranslatorGuard translator;
        QVERIFY(translator.set(&markerTranslator));
        QVERIFY(emitLanguageChanged(QStringLiteral("Marker")));

        QCOMPARE(loadAction->text(), marker(QStringLiteral("Load bookmark")));
        QCOMPARE(fileMenu->title(), marker(QStringLiteral("&File")));
        QCOMPARE(statusLabel->text(), marker(QStringLiteral("No folder or archive is loaded.")));
        QCOMPARE(viewer.fullscreenButton()->toolTip(), marker(QStringLiteral("&Fullscreen")));

        const QList<QString> keyGroups = qApp->keyActions().nameByGroups().uniqueKeys();
        const QList<QString> mouseGroups = qApp->mouseActions().nameByGroups().uniqueKeys();
        QVERIFY(keyGroups.contains(marker(QStringLiteral("Bookmark"))));
        QVERIFY(mouseGroups.contains(marker(QStringLiteral("Bookmark"))));
        QVERIFY(!keyGroups.contains(QStringLiteral("Bookmark")));
        QVERIFY(!mouseGroups.contains(QStringLiteral("Bookmark")));

        QCOMPARE(folder.windowTitle(), marker(QStringLiteral("Folders")));
        QCOMPARE(folderLabel->text(), marker(QStringLiteral("Current folder")));
        QCOMPARE(folderHome->toolTip(), marker(QStringLiteral("Go to home folder")));

        QCOMPARE(catalog.windowTitle(), marker(QStringLiteral("Catalog")));
        QCOMPARE(catalogViewMenu->title(), marker(QStringLiteral("&View")));
        QCOMPARE(
            catalogSearch->lineEdit()->placeholderText(),
            marker(QStringLiteral("Enter a search term and press Enter to search by title.")));
        QCOMPARE(
            catalogStatus->text(),
            marker(QStringLiteral("Drop an image folder here to create a catalog.")));

        QCOMPARE(exif.windowTitle(), marker(QStringLiteral("Exif Information")));
        QCOMPARE(exifClipboard->text(), marker(QStringLiteral("Copy to clipboard")));
        QVERIFY(exifText->toPlainText().contains(marker(QStringLiteral("Filename"))));
        QVERIFY(exifText->toPlainText().contains(marker(QStringLiteral("Horizontal (normal)"))));

        QCOMPARE(retouch.windowTitle(), marker(QStringLiteral("Image adjustments")));
        QCOMPARE(retouchBrightness->text(), marker(QStringLiteral("Brightness")));
        QCOMPARE(retouchReset->text(), marker(QStringLiteral("Reset")));

        translator.clear();
        QVERIFY(emitLanguageChanged(QStringLiteral("English")));
    }

    void japaneseToEnglishLeavesNoStaleJapaneseUiText()
    {
        LanguageSwitchWindow viewer;
        FolderWindow folder(nullptr, nullptr);
        CatalogWindow catalog(nullptr, viewer.mainUi());
        ExifDialog exif;
        RetouchWindow retouch(nullptr);

        DecoratingTranslator japanese(QStringLiteral("日本語［"), QStringLiteral("］"));
        TranslatorGuard translator;
        QVERIFY(translator.set(&japanese));
        QVERIFY(emitLanguageChanged(QStringLiteral("Japanese")));

        const QList<QObject *> roots = {&viewer, &folder, &catalog, &exif, &retouch};
        bool sawJapanese = false;
        for (QObject *root : roots) {
            const QStringList texts = relevantUiText(root);
            for (const QString &text : texts) {
                if (containsJapanese(text)) {
                    sawJapanese = true;
                    break;
                }
            }
        }
        QVERIFY2(sawJapanese, "The Japanese phase did not seed any translatable UI text.");

        translator.clear();
        QVERIFY(emitLanguageChanged(QStringLiteral("English")));

        QStringList staleJapanese;
        for (QObject *root : roots) {
            const QStringList texts = relevantUiText(root);
            for (const QString &text : texts) {
                if (containsJapanese(text)) {
                    staleJapanese << text;
                }
            }
        }
        const QByteArray message = QStringLiteral("Stale Japanese UI text after switching to English:\n%1")
                                       .arg(staleJapanese.join(QLatin1Char('\n')))
                                       .toUtf8();
        QVERIFY2(staleJapanese.isEmpty(), message.constData());
    }
};

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    int applicationArgc = 1;
    char *applicationArgv[] = {argv[0], nullptr};
    QVApplication application(applicationArgc, applicationArgv);
    LanguageSwitchTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_languageswitch.moc"
