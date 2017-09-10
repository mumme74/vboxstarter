#include "redirecttcpthread.h"
//#include "sshconnection.h"
#include "settings.h"
#include <QHostAddress>
#include <QFileInfo>
#include <QDir>




RedirectTcpThread::RedirectTcpThread(QObject *parent) :
    QObject(parent),
    m_client(0),
    m_remoteSocket(0),
    m_localSocket(0),
    m_tcpServer(0),
    m_port(0)
{
}


RedirectTcpThread::~RedirectTcpThread()
{
    clear();
}

void RedirectTcpThread::clear()
{
    if (m_localSocket){
        m_localSocket->close();
        m_localSocket->deleteLater();
        m_localSocket = 0;
    }

    if (m_remoteSocket){
        m_remoteSocket->close();
        m_remoteSocket->deleteLater();
        m_remoteSocket = 0;
    }

    if (m_client) {
        m_client->disconnectFromHost();
        m_client->disconnect();
        m_client->deleteLater();
        m_client = 0;
    }

    if (m_tcpServer) {
        m_tcpServer->close();
        m_tcpServer->deleteLater();
        m_tcpServer = 0;
    }
}

void RedirectTcpThread::fromLocalToRemote(QTcpSocket *socket)
{
    QByteArray buf = socket->readAll();
    if (m_remoteSocket && m_remoteSocket->isWritable()){
        m_remoteSocket->write(buf);
    }

   /* QString resp;
    foreach(QChar ch, buf){
        resp += QString::number(ch.toAscii());
    }
    qDebug()<< "tcp sending data from local length:"<<buf.size()<<
               " ' " <<resp <<"'" <<"'"<<QString(buf)<<"'"<<endl;*/

}

void RedirectTcpThread::fromRemoteToLocal()
{
    QByteArray buf = m_remoteSocket->readAll();
    foreach(Lambda *obj, m_localSockets){
        static_cast<QTcpSocket *>(obj->parent())->write(buf);
    }

    /*QString resp;
    foreach(QChar ch, buf){
        resp += QString::number(ch.toAscii());
    }
    qDebug()<< "tcp recived data from remote length:"<<buf.size()<<
               " ' " <<resp <<"'" <<"'"<<QString(buf)<<"'"<<endl;*/
}




void RedirectTcpThread::onClientConnected()
{
    m_remoteSocket = m_client->openTcpSocket("localhost", m_port);
    connect(m_remoteSocket, SIGNAL(readyRead()), this, SLOT(fromRemoteToLocal()));
    connect(m_remoteSocket, SIGNAL(connected()), this, SLOT(onRemoteConnected()));
}


void RedirectTcpThread::onLocalConnected()
{
    qDebug()<<"connected a new connection " <<endl;
}

void RedirectTcpThread::onLocalDisconnected(QObject *socket)
{
    foreach(Lambda *obj, m_localSockets){
        if (obj->parent() == socket) {
            m_localSockets.removeOne(obj);
            obj->deleteLater();
        }
    }
}


void RedirectTcpThread::onRemoteConnected()
{
    emit redirectComplete(m_port);
}

void RedirectTcpThread::onDisconnected()
{

    emit redirectDisconnected(m_port);

    clear();
}

void RedirectTcpThread::onSSHError(QxtSshClient::Error err)
{
    qWarning() << "Error in redirection of port " << m_port << " errCode:" << err <<endl;
}

void RedirectTcpThread::onTcpError(QAbstractSocket::SocketError err)
{
    qDebug()<<"Error tcpsocket error on port "<< m_port<<" errCode:"<<err<<endl;
}

void RedirectTcpThread::onNewConnection()
{
    qDebug()<<"new connection" << endl;
    QTcpSocket *newConn = m_tcpServer->nextPendingConnection();
    Lambda *container = new Lambda(newConn);

    connect(container, SIGNAL(readyRead(QTcpSocket*)), this, SLOT(fromLocalToRemote(QTcpSocket*)));
    connect(newConn, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onTcpError(QAbstractSocket::SocketError)));

    connect(newConn, SIGNAL(connected()), this, SLOT(onLocalConnected()));
    connect(newConn, SIGNAL(destroyed(QObject*)), this, SLOT(onLocalDisconnected(QObject*)));

    m_localSockets.append(container);

    // may need a buffer to send client until we have established a connection?

}


void RedirectTcpThread::redirect(quint16 port, const QString passPhrase)
{
    clear();

    m_port = port;
    m_passPhrase = passPhrase;

    Settings settings;
    m_client = new QxtSshClient;
    m_client->setKeyFiles(settings.myValue("keyfilepublic").toString(),
                          settings.myValue("keyfileprivate").toString());

    QFileInfo info(settings.fileName());
    m_client->loadKnownHosts(info.absoluteDir().absoluteFilePath("known_hosts"));

    if (!m_passPhrase.isEmpty()){
        m_client->setPassphrase(m_passPhrase);
    }


    connect(m_client, SIGNAL(connected()), this, SLOT(onClientConnected()));
    connect(m_client, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    connect(m_client, SIGNAL(error(QxtSshClient::Error)), this, SLOT(onSSHError(QxtSshClient::Error)));

    m_client->connectToHost(settings.value("username").toString(),
                            settings.value("server").toString(),
                            settings.value("port").toInt());

    m_tcpServer = new QTcpServer(m_client);
    connect(m_tcpServer, SIGNAL(newConnection()), this, SLOT(onNewConnection()));


    m_tcpServer->listen(QHostAddress(QHostAddress::LocalHost), m_port);

    qDebug()<<"server listens to " << m_tcpServer->serverAddress() <<":"<< m_tcpServer->serverPort()<<endl;

}

void RedirectTcpThread::tearDown()
{
    emit onDisconnected();
    clear();
}


