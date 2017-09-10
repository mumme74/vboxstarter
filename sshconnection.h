#ifndef SSHCONNECTION_H
#define SSHCONNECTION_H

#include <QObject>
#include <QList>
#include <QxtSshChannel>
#include <QxtSshClient>
#include <QxtSshProcess>
#include "redirecttcpthread.h"

class SSHJob : public QObject
{
    Q_OBJECT

public:
    SSHJob(QObject *parent = 0);
    QString cmd() const { return m_cmd; }
    void setCmd(QString cmd) { m_cmd = cmd; }
    QString response(bool deleteLater = true);
    void setResponse(QString response);

signals:
    void finished(SSHJob *job);

private:
    QString m_response;
    QString m_cmd;
};

class SSHConnection : public QObject
{
    Q_OBJECT
public:
    explicit SSHConnection(QObject *parent = 0);
    ~SSHConnection();

    bool isConnected();
    void setRestartCount(quint8 maxtimes);

    SSHJob *sendCmd(QString cmd);


public slots:
    void connectSSH();
    void disconnectSSH();
    void redirectPort(bool connect, quint16 port);

private:
    QString m_passPhrase;
    QHash<quint16, RedirectTcpThread*> m_tcpSockets;
    QxtSshClient *m_client;
    QxtSshProcess *m_process;
    bool m_connected;
    bool m_remoteRunning;
    quint8 m_restartCount;
    SSHJob *m_activeJob;

    QList<SSHJob*> m_jobs;

    void saveHostKey();
    void sendNext();

signals:
    void connectedToServer();
    void disconnectedFromServer();
    void remoteProcessStarted();
    void remoteProcessStopped(int code);
    void redirectComplete(quint16 port);
    void redirectDisconnected(quint16 port);

private slots:
    void wrongPassphrase();
    void onConnected();
    void onDisconnected();
    void onProcessStarted();
    void onProcessFinished(int code);
    void onProcessAboutToClose();
    void onReadyRead();
    void onSSHError(QxtSshClient::Error err);
    void onRedirectComplete(quint16 port);
    void onRedirectDisconnected(quint16 port);

};



#endif // SSHCONNECTION_H
