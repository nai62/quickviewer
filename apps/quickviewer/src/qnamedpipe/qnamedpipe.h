#ifndef QNAMEDPIPE_H
#define QNAMEDPIPE_H

#include <QtCore>

class QLocalServer;

/**
 * @brief The QNamedPipe class
 * QNamedPipe provides simple interprocess communication in local.
 *
 * The first running process becomes the server and every process started after
 * it is a client: the server holds a local socket with the given name, and a
 * client hands over one message and stops.
 *
 * Messages are sent unilaterally from the client, and the server emits
 * received(bytes) without any reply.
 *
 * The channel is a QLocalServer and QLocalSocket pair, which is a named pipe on
 * Windows and a socket in the temporary directory elsewhere.
 */
class QNamedPipe : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief QNamedPipe
     * @param name: of the channel, which the account name keeps apart from
     * other users' channels
     * @param valid: If valid is false, QNamedPipe does not use a channel and
     * every process is its own server
     * @param parent
     */
    explicit QNamedPipe(QString name, bool valid, QObject *parent = nullptr);
    ~QNamedPipe();
    /**
     * @brief send
     * Sends bytes to the server as a client. Sending from the server, and
     * sending when no server answers, does nothing.
     */
    void send(QByteArray bytes);
    /**
     * @brief isServerMode
     * @return if this process holds the channel and keeps running
     */
    bool isServerMode();

signals:
    /**
     * @brief received
     * Signal which received a byte sequence from a client.
     */
    void received(QByteArray bytes);

private:
    bool startListening();
    void handleNewConnection();

    QLocalServer *m_server;
    QString m_name;
    bool m_valid;
    bool m_serverMode;
};

#endif // QNAMEDPIPE_H
