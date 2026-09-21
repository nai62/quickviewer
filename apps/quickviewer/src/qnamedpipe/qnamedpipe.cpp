#include "qnamedpipe.h"

#include <QLocalServer>
#include <QLocalSocket>

namespace {

// How long a client waits for the process that holds the channel, and how long
// a process that found the channel taken waits for an answer from it.
constexpr int ConnectionTimeoutMilliseconds = 1000;

// The name of a local socket is machine-wide on Windows and shared by everyone
// who can write to the temporary directory elsewhere, so the account name keeps
// two users' viewers apart.
QString channelName(const QString &name)
{
    QString user = qEnvironmentVariable("USERNAME");
    if (user.isEmpty()) {
        user = qEnvironmentVariable("USER");
    }
    return QStringLiteral("%1-%2").arg(user, name);
}

} // namespace

QNamedPipe::QNamedPipe(QString name, bool valid, QObject *parent)
    : QObject(parent),
      m_server(new QLocalServer(this)),
      m_name(channelName(name)),
      m_valid(valid),
      m_serverMode(!valid)
{
    if (!m_valid) {
        // Nothing to coordinate: every process runs on its own.
        return;
    }

    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (startListening()) {
        return;
    }

    if (m_server->serverError() == QAbstractSocket::AddressInUseError) {
        // Another process holds the channel, or held it and ended without
        // cleaning up after itself. Probing tells the two apart before this
        // process decides to hand over its arguments and stop.
        QLocalSocket probe;
        probe.connectToServer(m_name, QIODevice::ReadOnly);
        if (probe.waitForConnected(ConnectionTimeoutMilliseconds)) {
            probe.disconnectFromServer();
            return;
        }
        // Nothing answered, so what is in the way is stale and this process
        // takes the channel over.
        QLocalServer::removeServer(m_name);
        if (startListening()) {
            return;
        }
    }

    // Without a channel this process still runs; it just cannot be told about
    // the volumes another process opens.
    qWarning() << "Could not hold the channel for other QuickViewer processes:"
               << m_server->errorString();
    m_serverMode = true;
}

QNamedPipe::~QNamedPipe() = default;

bool QNamedPipe::startListening()
{
    if (!m_server->listen(m_name)) {
        return false;
    }
    m_serverMode = true;
    connect(m_server, &QLocalServer::newConnection, this, &QNamedPipe::handleNewConnection);
    return true;
}

void QNamedPipe::handleNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        // A message arrives in as many pieces as the socket hands over, so the
        // bytes are collected until the client closes the connection it sent
        // them on.
        auto *bytes = new QByteArray;
        connect(socket, &QObject::destroyed, socket, [bytes] { delete bytes; });
        connect(socket, &QLocalSocket::readyRead, socket, [socket, bytes] {
            bytes->append(socket->readAll());
        });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket, bytes] {
            bytes->append(socket->readAll());
            if (!bytes->isEmpty()) {
                emit received(*bytes);
            }
            socket->deleteLater();
        });
    }
}

void QNamedPipe::send(QByteArray bytes)
{
    if (!m_valid || m_serverMode) {
        return;
    }

    QLocalSocket socket;
    socket.connectToServer(m_name, QIODevice::WriteOnly);
    if (!socket.waitForConnected(ConnectionTimeoutMilliseconds)) {
        qWarning() << "Could not reach the running QuickViewer:" << socket.errorString();
        return;
    }
    socket.write(bytes);
    socket.flush();
    if (!socket.waitForBytesWritten(ConnectionTimeoutMilliseconds)) {
        qWarning() << "Could not hand the message over:" << socket.errorString();
    }
    socket.disconnectFromServer();
}

bool QNamedPipe::isServerMode()
{
    return m_serverMode;
}
