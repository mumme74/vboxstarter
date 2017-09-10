#include "sshredirectport.h"


SshRedirectPort::SshRedirectPort(SharedInfoHolder * info, QString port_identifier, QObject * parent)
{
    is_reverse = false;

    must_reconnect_local_socket = true;

    tcpServer   = NULL;
    sshSocket   = NULL;
    sshListener = NULL;

    sharedInfo = info;

    m_port_id = port_identifier;

    qDebug() << "INFO : SshRedirectPort : initialize a redirection manager for protocole=" << m_port_id;
}

/* PUBLIC METHODS */

void SshRedirectPort::redirectPort(int port, int bind, bool reverse)
{
    int AUTO_PORT = 0;
    is_reverse = reverse;
    distantPort = port; // keep for later tunneling

    if (!is_reverse)
    {
        tcpServer = new QTcpServer(this);

        connect(tcpServer, SIGNAL(newConnection()),
                this     , SLOT  (onNewConnection()));

        tcpServer->listen(QHostAddress(sharedInfo->LOCALHOST_IP), qMax(bind, AUTO_PORT));

        localTcpPort = tcpServer->serverPort();

        qDebug() << "DEBUG : SshRedirectPort (for" << m_port_id << ") : listen at " << localTcpPort;

        qDebug() << "DEBUG : SshRedirectPort (for" << m_port_id << ") : try forwarding server_port=" << distantPort;
        sshSocket = sharedInfo->SSH_CLIENT->openTcpSocket(sharedInfo->LOCALHOST_IP, m_port_id, distantPort);
        if (sshSocket != NULL)
        {
            connect(sshSocket, SIGNAL(readyRead()),
                    this     , SLOT  (onSshDataReceived()));
            emit sshSocketRedirected();
        }
        else {
            qDebug() << "ERROR : SshRedirectPort : error forwarding" << m_port_id;
            emit sshRedirectionError(m_port_id);
        }

    }
    else
    {
        /* REVERSE */

        localTcpPort = bind;

        qDebug() << "DEBUG : SshRedirectPort (for" << m_port_id << ") : try reverse forwarding port " << distantPort;
        sshListener = sharedInfo->SSH_CLIENT->listenTcpServer("localhost", m_port_id, distantPort, bind);

        connect(sshListener, SIGNAL(channelAccepted()), this, SLOT(onReverseChannelAccepted()));

        if (sshListener != NULL && localTcpPort > 0) {
            connect(sshListener, SIGNAL(readyRead()),
                    this       , SLOT  (onSshDataReceived()));
            emit sshSocketRedirected();
        }
        else {
            qDebug() << "ERROR : SshRedirectPort : error reverse forwarding" << m_port_id;
            emit sshRedirectionError(m_port_id);
        }
    }
}

void SshRedirectPort::onReverseChannelAccepted()
{
    if (is_reverse)
    {
        if (tcpSocket != NULL) {
            qDebug() << "WARNING : SshRedirectPort : tcpSocket was not null, deleting";
            disconnect(tcpSocket, 0, 0, 0);
            delete tcpSocket;
        }

        tcpSocket = new QTcpSocket(this);

        connect(tcpSocket, SIGNAL(connected()),
                this     , SLOT  (onReverseSocketConnected()));
        connect(tcpSocket, SIGNAL(disconnected()),
                this     , SLOT  (onReverseSocketDisconnected()));
        connect(tcpSocket, SIGNAL(readyRead()),
                this     , SLOT  (onTcpDataReceived()) );
        connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)),
                this     , SLOT  (onReverseSocketError(QAbstractSocket::SocketError)));

        //tcpSocket->connectToHost(QHostAddress(sharedInfo->LOCALHOST_IP), localTcpPort);
    }
}

void SshRedirectPort::onReverseSocketConnected()
{
    //QTcpSocket * tcpSocket = qobject_cast<QTcpSocket *>(sender());

    if (tcpSocket != NULL)
    {

        qDebug() << "DEBUG : SshRedirectPort (for" << m_port_id << ") : created a new connexion on tcp reverse socket"
                 << "local_port=" << tcpSocket->localPort()
                 << "server_port=" << tcpSocket->peerPort();

        qDebug() << "DEBUG : SshRedirectPort (for" << m_port_id << ") : now have one reverse socket to manage";


        must_reconnect_local_socket = false;

        tcpSocket->write(bufferSsh);
        bufferSsh.clear();
    }
}

void SshRedirectPort::onReverseSocketDisconnected()
{
    //QTcpSocket * tcpSocket = qobject_cast<QTcpSocket *>(sender());

    if (tcpSocket != NULL)
    {
        must_reconnect_local_socket = true;

        qDebug() << "WARNING : SshRedirectPort (for" << m_port_id << ") : tcp reverse socket disconnected !";
    }
}

void SshRedirectPort::stopRedirection()
{
    if (sshSocket != NULL)
    {
        qDebug() << "INFO : SshRedirectPort (for" << m_port_id << ") : stopping local ssh socket";
        sshSocket->close();
    }
    if (sshListener != NULL)
    {
        qDebug() << "INFO : SshRedirectPort (for" << m_port_id << ") : stopping local ssh listener";
        sshListener->close();
    }
    if (tcpServer != NULL)
    {
        qDebug() << "INFO : SshRedirectPort (for" << m_port_id << ") : stopping local tcp server";
        tcpServer->close();
    }
    if (tcpSocket != NULL)
    {
        qDebug() << "INFO : SshRedirectPort (for" << m_port_id << ") : stopping local tcp sockets";
        tcpSocket->close();
    }
}

int SshRedirectPort::localPort()
{
    return localTcpPort;
}


/* PRIVATE SLOTS */

void SshRedirectPort::onNewConnection()
{
    qDebug() << "DEBUG : SshRedirectPort (for" << m_port_id << ") : grabbing tcp socket from local tcp server next pending connection";

    if (tcpSocket != NULL) {
        qDebug() << "WARNING : SshRedirectPort : tcpSocket was not null, deleting";
        disconnect(tcpSocket, 0, 0, 0);
        delete tcpSocket;
    }

    tcpSocket = tcpServer->nextPendingConnection();

    connect(tcpSocket, SIGNAL(readyRead()),
            this     , SLOT  (onTcpDataReceived()));
    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this     , SLOT  (displayError(QAbstractSocket::SocketError)));


    qDebug() << "DEBUG : SshRedirectPort (for" << m_port_id << ") : created a new connexion on tcp socket"
             << "local_port=" << tcpSocket->localPort()
             << "server_port=" << tcpSocket->peerPort();

    qDebug() << "DEBUG : SshRedirectPort (for" << m_port_id << ") : now have one socket to manage";


    tcpSocket->write(bufferSsh);
    bufferSsh.clear();
}

void SshRedirectPort::onSshDataReceived()
{
    QByteArray data;
    if (!is_reverse)
    {
        data = sshSocket->readAll();
    }
    else
    {
        data = sshListener->readAll();
    }

    if (is_reverse)
    {
        qDebug() << "DUMP : SshRedirectPort (for" << m_port_id << ") : received SSH data=\n" << data.toBase64();
    }

    if (tcpSocket != NULL)
    {
        qDebug() << "DEBUG : SshRedirectPort : tcpSocket is Not Null";

        if (tcpSocket->isValid())
        {
            qDebug() << "DEBUG : SshRedirectPort : tcpSocket is Valid";

            if (tcpSocket->state() == QAbstractSocket::ConnectedState)
            {
                qDebug() << "DEBUG : SshRedirectPort : tcpSocket is in State Connected";

                qDebug() << "DEBUG : SshRedirectPort (for" << m_port_id << ") : writing SSH data to local TCP socket";
                tcpSocket->write(data);
            }
            else
            {
                qDebug() << "DEBUG : SshRedirectPort (for" << m_port_id << ") : waiting socket to reconnect, writing SSH data to buffer";
                bufferSsh.append(data);

                if (is_reverse)
                {
                    tcpSocket->connectToHost(QHostAddress(sharedInfo->LOCALHOST_IP), localTcpPort, QIODevice::ReadWrite);
                }
            }

        }
        else
        {
            qDebug() << "DEBUG : SshRedirectPort (for" << m_port_id << ") : local tcp socket is NULL, writing SSH data to buffer";
            bufferSsh.append(data);
        }
    }
    else
    {
        qDebug() << "DEBUG : SshRedirectPort (for" << m_port_id << ") : local tcp socket is NULL, writing SSH data to buffer";
        bufferSsh.append(data);
    }
}

void SshRedirectPort::onTcpDataReceived()
{
    //QTcpSocket * tcpSocket = qobject_cast<QTcpSocket *>(sender());
    if (tcpSocket != NULL)
    {
        QByteArray data;
        data = tcpSocket->readAll();
        qDebug() << "DUMP : SshRedirectPort (for" << m_port_id << ") : received TCP data=\n" << data.toBase64();

        if (!is_reverse)
        {
            sshSocket->write(data);
        }
        else {
            sshListener->write(data);
        }
    }
    else {
        qDebug() << "WARNING : SshRedirectPort (for" << m_port_id << ") : received TCP data but not seems to be a valid Tcp socket";
    }
}

void SshRedirectPort::onReverseSocketError(QAbstractSocket::SocketError error)
{
    qDebug() << "ERROR : SshRedirectPort (for" << m_port_id << ") : redirection reverse socket error=" << error;
    must_reconnect_local_socket = true;

}

void SshRedirectPort::displayError(QAbstractSocket::SocketError error)
{
    qDebug() << "ERROR : SshRedirectPort (for" << m_port_id << ") : redirection socket error=" << error;
}