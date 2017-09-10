#include "settings.h"
#include <cstdlib>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>

#ifdef Q_WS_WIN
// windows
#define USERNAME "USERNAME"
#define SSH_PRIVATE_KEY QDir::homePath() + "/Application Data/ssh/id_rsa"
#define SSH_PUBLIC_KEY QDir::homePath() + "/Application Data/ssh/id_rsa.pub"
#else
// os x and linux
#define USERNAME "USER"
#define SSH_PRIVATE_KEY QDir::homePath() + "/.ssh/id_rsa"
#define SSH_PUBLIC_KEY QDir::homePath() + "/.ssh/id_rsa.pub"
#endif

#ifdef Q_WS_WIN
#define RDP_PROGRAM "mstsc.exe /v:localhost:[port] /f"
#elif defined Q_WS_MAC
#define RDP_PROGRAM "open rdp://localhost:[port]?fullscreen=true\\&screenDepth=16"
#else
#define RDP_PROGRAM "rdesktop -a 16 -N localhost:[port]"
#endif

Settings::Settings(QObject *parent) :
    QSettings(parent)
{
}

// only to implement a application specific default for the settings
QVariant Settings::myValue(const QString &key) const
{
    QVariant res = QSettings::value(key);

    if (res.isNull() || res.toString().isEmpty()){
        if (key == "username") {
            res = QString(getenv(USERNAME));
        } else if (key == "port") {
            res = 22;
        } else if (key == "server") {
            res = QString("kufordonserver.local");
        } else if (key == "keyfileprivate") {
            res = QString(SSH_PRIVATE_KEY);
        } else if (key == "keyfilepublic") {
            res = QString(SSH_PUBLIC_KEY);
        } else if(key == "serverClient"){
            res = QString("node ~/Resurs/vbox_startstop/client.js json");
        }else if (key == "rdpprogram"){
            res = QString(RDP_PROGRAM);
        } else {
            qDebug()<<"Unknown key supplied to Settings:"<<key<<endl;
        }
    }

    return res;
}

