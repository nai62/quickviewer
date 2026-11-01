#ifndef TESTTEMPORARYDIR_H
#define TESTTEMPORARYDIR_H

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

/**
 * A fixture directory can outlive the viewer that used it: queued QObject
 * destruction and running page loads still hold archives open on Windows.
 * Give that work a bounded chance to release its files before removing them.
 */
class TestTemporaryDir final : public QTemporaryDir
{
public:
    using QTemporaryDir::QTemporaryDir;

    ~TestTemporaryDir()
    {
        if (!isValid() || !autoRemove()) {
            return;
        }
        setAutoRemove(false);
        const QString directory = path();
        if (!QTest::qWaitFor(
                [&directory] {
                    return !QFileInfo::exists(directory) || QDir(directory).removeRecursively();
                },
                5000)) {
            const QByteArray message =
                QStringLiteral("Could not remove test directory after asynchronous cleanup: %1")
                    .arg(directory)
                    .toUtf8();
            QTest::qFail(message.constData(), __FILE__, __LINE__);
        }
    }
};

#endif // TESTTEMPORARYDIR_H
