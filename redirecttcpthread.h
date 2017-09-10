#ifndef REDIRECTTCPTHREAD_H
#define REDIRECTTCPTHREAD_H

#include <QObject>
#include <QList>
#include <QxtSshTcpSocket>
#include <QxtSshClient>
#include <QTcpSocket>
#include <QTcpServer>

class Lambda;


// this class is meant to handle the redirection of port msg
// it should be running in its own thread
class RedirectTcpThread : public QObject
{
    Q_OBJECT
public:
    explicit RedirectTcpThread(QObject *parent = 0);
    ~RedirectTcpThread();

    void redirect(quint16 port, const QString passPhrase);
    void tearDown();
    void setPassPhrase(const QString passPhrase) { m_passPhrase = passPhrase; }

protected:


private:
    void clear();

    QxtSshClient    *m_client;
    QxtSshTcpSocket *m_remoteSocket;
    QTcpSocket      *m_localSocket;
    QTcpServer      *m_tcpServer;
    QString         m_passPhrase;
    quint16         m_port;
    QList<Lambda *> m_localSockets;

signals:
    void redirectComplete(quint16 port);
    void redirectDisconnected(quint16 port);

public slots:

private slots:
    void fromLocalToRemote(QTcpSocket *socket);
    void fromRemoteToLocal();
    void onClientConnected();
    void onRemoteConnected();
    void onLocalConnected();
    void onDisconnected();
    void onLocalDisconnected(QObject*);
    void onSSHError(QxtSshClient::Error err);
    void onTcpError(QAbstractSocket::SocketError err);

    void onNewConnection();
};

class Lambda :public QObject {
    Q_OBJECT
public:
    Lambda(QTcpSocket* socket) :QObject(socket)
    {
        connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    }

signals:
    void readyRead(QTcpSocket *socket);

private slots:
    void onReadyRead() {
        emit readyRead(static_cast<QTcpSocket*>(parent()));
    }

};

#endif // REDIRECTTCPTHREAD_H
