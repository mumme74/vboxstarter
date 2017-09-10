#include "sshconnection.h"

#include <QxtNetwork>
#include <QThread>
#include <QMessageBox>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QTextCodec>
#include "settings.h"
#include "passphrase.h"
#include "mainwindow.h"
#include "appcontroller.h"
#include "redirecttcpthread.h"


#define debug

SSHJob::SSHJob(QObject *parent) :
    QObject(parent)
{
}

void SSHJob::setResponse(QString response)
{
    m_response = response;
    emit finished(this);
}

QString SSHJob::response(bool deleteLater)
{
    if (deleteLater) {
        this->deleteLater();
    }
    return m_response;
}

// defines how many times remote clinet could be restarted
#define MAX_RESTART_COUNT 10

SSHConnection::SSHConnection(QObject *parent) :
    QObject(parent),
    m_client(0),
    m_process(0),
    m_connected(false),
    m_remoteRunning(false),
    m_restartCount(MAX_RESTART_COUNT),
    m_activeJob(0)
{

}

SSHConnection::~SSHConnection()
{
    disconnectSSH();
}


void SSHConnection::connectSSH()
{
    disconnectSSH();;

    Settings settings;

    QFileInfo publicFile = settings.myValue("keyfilepublic").toString();
    QFileInfo privateFile = settings.myValue("keyfileprivate").toString();
    if (!publicFile.exists()) {
        QMessageBox::information(static_cast<AppController*>(parent())->getMainWindow(),
                                 trUtf8("Privat ssh nyckel"),
                                 trUtf8("Den angivna privata SSH nycklen %0 existerar inte!")
                                    .arg(publicFile.filePath()));
        return;
    }
    if (!privateFile.exists()) {
        QMessageBox::information(static_cast<AppController*>(parent())->getMainWindow(),
                                 trUtf8("Publik ssh nyckel"),
                                 trUtf8("Den angivna publika SSH nycklen %0 existerar inte!")
                                    .arg(privateFile.filePath()));
        return;
    }

    m_client = new QxtSshClient;
    m_client->setKeyFiles(publicFile.filePath(), privateFile.filePath());

    QFileInfo info(settings.fileName());
    m_client->loadKnownHosts(info.absoluteDir().absoluteFilePath("known_hosts"));

    if (!m_passPhrase.isEmpty()){
        m_client->setPassphrase(m_passPhrase);
    } else if(!settings.myValue("passphrase").isNull()) {
        m_passPhrase = settings.myValue("passphrase").toString();
        m_client->setPassphrase(m_passPhrase);
    }

    connect(m_client, SIGNAL(authenticationRequired(QList<QxtSshClient::AuthenticationMethod>)),
            this, SLOT(wrongPassphrase()));
    connect(m_client, SIGNAL(connected()), this, SLOT(onConnected()));
    connect(m_client, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    connect(m_client, SIGNAL(error(QxtSshClient::Error)), this, SLOT(onSSHError(QxtSshClient::Error)));

    m_client->connectToHost(settings.myValue("username").toString(),
                            settings.myValue("server").toString(),
                            settings.myValue("port").toInt());

}

void SSHConnection::disconnectSSH()
{
    foreach(RedirectTcpThread *tcpObj, m_tcpSockets){
        tcpObj->tearDown();
        tcpObj->deleteLater();
    }
    m_tcpSockets.clear();

    if (m_process){
        m_process->write("exit\n");
        m_process->close();
        m_process->deleteLater();
        m_process = 0;
        emit remoteProcessStopped(0);
    }

    if (m_client) {
        m_client->disconnectFromHost();
        m_client->disconnect();
        m_client->deleteLater();
        m_client = 0;
        emit disconnectedFromServer();
    }

    m_connected = false;
    m_remoteRunning = false;
    m_restartCount = MAX_RESTART_COUNT;
    m_activeJob = 0;
}

bool SSHConnection::isConnected()
{
    if (!m_client){
        return false;
    } else if (!m_process){
        return false;
    } else if (!m_connected){
        return false;
    } else if (!m_remoteRunning) {
        return m_remoteRunning;
    }

    return m_process->isOpen();
}

void SSHConnection::onConnected()
{
    m_connected = true;

    // start process
    m_process = m_client->openProcessChannel();

    connect(m_process, SIGNAL(started()), this, SLOT(onProcessStarted()));
    connect(m_process, SIGNAL(finished(int)), this, SLOT(onProcessFinished(int)));
    connect(m_process, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(m_process, SIGNAL(aboutToClose()), this, SLOT(onProcessAboutToClose()));
    connect(m_process, SIGNAL(disconnected()), this, SLOT(onDisconnected()));

    Settings settings;
    m_process->requestPty(QxtSshProcess::VanillaTerminal); //settings.value("serverClient").toByteArray()
    m_process->start(settings.value("serverClient").toString());

    emit connectedToServer();
}

void SSHConnection::onDisconnected()
{
    m_connected = false;
    emit disconnectedFromServer();
}

void SSHConnection::onProcessFinished(int code)
{
    MainWindow *motherWidget = qobject_cast<AppController*>(parent())->getMainWindow();

    m_remoteRunning = false;
    emit remoteProcessStopped(code);

    if (m_connected && m_restartCount > 0) {
        m_restartCount--;
        onConnected();
    } else {
        QMessageBox::warning(motherWidget, trUtf8("Fel"),
                             trUtf8("Serverklienten har stannat!\nDet kan vara problem med servern"));
    }
}

void SSHConnection::onProcessAboutToClose()
{
#ifdef debug
    qDebug()<< "onProcessAboutToClose()" <<endl;
#endif

    if (m_process->isWritable()){
        m_process->write("exit\n");
        m_process->waitForReadyRead(100);
        qDebug() << "onexit:"<<m_process->readAll()<<endl;
        m_process->write("exit\n");

    }
}

void SSHConnection::onProcessStarted()
{
    emit remoteProcessStarted();
    qDebug()<< "process started" <<endl;

}

void SSHConnection::onReadyRead()
{
    QByteArray data = m_process->readAll();
    QString response = QString::fromUtf8(data.data(), data.length());

#ifdef debug
    qDebug() << "recieved:" << qPrintable(response)<<endl;
#endif

    if (m_activeJob != 0) {
        response = response.replace(QRegExp("\\s*$"), "");
        if (m_activeJob->cmd() != response){
            m_activeJob->setResponse(response);
            m_activeJob = 0;
        }
    } else {
        if (response.length() == 0) {
            // hack because this signal doesnt get emitted from QxtSsh
            // a zero length response is probably that the process died
            onProcessFinished(0);
            return;
        }

        if (response.indexOf("\"connected\"") > -1) {
            m_remoteRunning = true;
        }
    }

    sendNext();
}

void SSHConnection::onRedirectComplete(quint16 port)
{
    emit redirectComplete(port);
}

void SSHConnection::onRedirectDisconnected(quint16 port)
{
    emit redirectDisconnected(port);
}

void SSHConnection::onSSHError(QxtSshClient::Error err)
{
    MainWindow *main = qobject_cast<AppController *>(parent())->getMainWindow();

    QString msg;
    switch (err) {
    case QxtSshClient::AuthenticationError:
        if (m_passPhrase == "") {
            return;
        }
        return; //msg = trUtf8("Autensieringsfel, har du skrivit rätt nyckelfras?");
        // handle in wronpassPhrase slot instead
        break;
    case QxtSshClient::HostKeyUnknownError:
        if (QMessageBox::question(main, trUtf8("Okänd host %1").arg(m_client->hostName()),
                                  trUtf8("Denna server med namnet \"%1\" och med ssh nyckel %2"
                                         " är inte känd sedan tidigare.\n"
                                         "Vill du lägga till denna server i listan med betrodda servrar?")
                                        .arg(QString(m_client->hostName().toAscii()))
                                        .arg(QString(m_client->hostKey().key)))
                           == QMessageBox::Ok
        ){
            saveHostKey();
        }

        return;
    case QxtSshClient::HostKeyInvalidError:
        msg = "Denna servers hostkey är ogilltig";
        break;
    case QxtSshClient::HostKeyMismatchError:
        msg = trUtf8("Denna servers hostkey stämmer inte mot sparad nyckel");
        if (QMessageBox::question(main, trUtf8("Uppdatera lista med kända nycklar"),msg)
                == QMessageBox::Ok)
        {
            saveHostKey();
        }
        break;
    case QxtSshClient::ConnectionRefusedError:
        msg = trUtf8("Anslutning nekades");
        break;
    case QxtSshClient::UnexpectedShutdownError:
        msg = trUtf8("Oväntat anslutningsfel");
        break;
    case QxtSshClient::HostNotFoundError:
        msg = trUtf8("Servern hittades inte");
        break;
    case QxtSshClient::SocketError:
        msg = trUtf8("Fel på socket ssh");
        break;
    case QxtSshClient::UnknownError:
        // falltrough
    default:
        msg = trUtf8("Okänt fel inträffade i ssh");
        break;
    }

    QMessageBox::critical(main, trUtf8("Anslutningsfel"), msg);

    qWarning() <<"fel i anslutning:"<<err << endl;
}

void SSHConnection::redirectPort(bool connect, quint16 port)
{
    if (m_connected) {
        if (connect) {
            RedirectTcpThread *redir = new RedirectTcpThread;
            redir->redirect(port, m_passPhrase);
            m_tcpSockets[port] = redir;
            this->connect(redir, SIGNAL(redirectComplete(quint16)),
                    this, SIGNAL(redirectComplete(quint16)));
            this->connect(redir, SIGNAL(redirectDisconnected(quint16)),
                    this, SLOT(onRedirectDisconnected(quint16)));
        } else {
            RedirectTcpThread *sock = m_tcpSockets.take(port);
            sock->tearDown();
            sock->deleteLater();
        }
    }
}

void SSHConnection::saveHostKey()
{
    Settings settings;

    m_client->addKnownHost(settings.value("server").toString(), m_client->hostKey());

    QFileInfo info(settings.fileName());
    m_client->saveKnownHosts(info.absoluteDir().absoluteFilePath("known_hosts"));
}

SSHJob *SSHConnection::sendCmd(QString cmd)
{
    SSHJob *job = new SSHJob;
    job->setCmd(cmd);

    m_jobs.append(job);
    sendNext();
    return job;
}

void SSHConnection::sendNext()
{
    if (m_activeJob || !isConnected()){
        return;
    }

    if (!m_jobs.empty()) {
        SSHJob *job = m_jobs.takeFirst();
        QByteArray cmd(job->cmd().toLocal8Bit());
        cmd.append('\n');
        m_process->write(cmd);
        m_activeJob = job;
#ifdef debug
        qDebug()<< "sending:"<<cmd<<endl;
#endif
    }
}

void SSHConnection::wrongPassphrase()
{
    MainWindow *motherWidget = qobject_cast<AppController*>(parent())->getMainWindow();
    if (m_passPhrase != "") {
        QMessageBox::information(motherWidget, trUtf8("Fel nyckelfras"),
                                 trUtf8("Du har angett fel nyckelfras"));
    }

    PassPhrase dlg;
    if (dlg.exec() == QDialog::Accepted){
        m_passPhrase = dlg.value();
        m_client->setPassphrase(m_passPhrase); // it tries to reconnect implicitly after this event loop
    } else {
        QMessageBox::information(motherWidget, trUtf8("Ingen anslutning"),
                                 trUtf8("Kan ej ansluta till servern"));
    }
}
