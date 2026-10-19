#include <QtTest>

#include "startupsettings.h"

namespace {

bool writeSettings(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

} // namespace

class StartupSettingsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void useDirect2D_data();
    void useDirect2D();
    void missingFileDoesNotUseDirect2D();
};

void StartupSettingsTest::useDirect2D_data()
{
    QTest::addColumn<QByteArray>("contents");
    QTest::addColumn<bool>("expected");

    QTest::newRow("true") << QByteArray("[View]\nUseDirect2D=true\n") << true;
    QTest::newRow("false") << QByteArray("[View]\nUseDirect2D=false\n") << false;
    QTest::newRow("absent") << QByteArray("[View]\nFitting=true\n") << false;
    // A value longer than the buffer once ran past it and broke the stack.
    QTest::newRow("long value") << QByteArray("[View]\nUseDirect2D=" + QByteArray(1000, 'x') + "\n")
                                << false;
    QTest::newRow("true followed by a long value")
        << QByteArray("[View]\nUseDirect2D=true" + QByteArray(1000, 'x') + "\n") << false;
}

void StartupSettingsTest::useDirect2D()
{
    QFETCH(QByteArray, contents);
    QFETCH(bool, expected);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("quickviewer.ini"));
    QVERIFY(writeSettings(path, contents));

    QCOMPARE(StartupSettings::useDirect2D(path), expected);
}

void StartupSettingsTest::missingFileDoesNotUseDirect2D()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QVERIFY(!StartupSettings::useDirect2D(directory.filePath(QStringLiteral("quickviewer.ini"))));
}

QTEST_GUILESS_MAIN(StartupSettingsTest)

#include "tst_startupsettingstest.moc"
