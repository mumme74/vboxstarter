#ifndef VMSCONTROLLER_H
#define VMSCONTROLLER_H

#include <QObject>
#include <QtCore>
#include <QStringList>

class SSHJob;
class SSHConnection;
class MainWindow;
class AppController;
class VmInfo;

class VmInfo : public QObject
{
    Q_OBJECT
public:
    VmInfo(QObject *parent = 0) :
        QObject(parent),
        m_running(false),
        m_pending(false),
        m_tunneled(false),
        m_port(0){ }

    void setName(const QString name) { m_name = name; }
    const QString name() const { return m_name; }

    void setPort(const quint16 port) { m_port = port; }
    quint16 port() const { return m_port; }

    void setDescription(const QString desc) { m_description = desc; }
    const QString description() const { return m_description; }

    void setPrograms(const QStringList programs) { m_programs = programs; }
    const QStringList programs() const { return m_programs; }

    void setPending(const bool pending) { m_pending = pending; }
    bool pending() const { return m_pending; }

    void setRunning(const bool running) { m_running = running; }
    bool running() const { return m_running; }

    void setTunneled(bool tunneled) { m_tunneled = tunneled; }
    bool tunneled() const { return m_tunneled; }



private:
    bool m_running;
    bool m_pending;
    bool m_tunneled;
    QString m_name;
    QString m_description;
    QStringList m_programs;
    quint16 m_port;
};

// this class controlls all the vm objects

class VmsController : public QObject
{
    Q_OBJECT
public:
    explicit VmsController(QObject *parent = 0);

    const QList<QString>  availableVms() const; // list of all the vmnames (VmID)

    void            init();

    void            refresh();          // reload the internal info



    void            updateUsers();

    void            start(const QString VmID); // start a vm
    void            stop(const QString VmID);  // stop a vm



    const VmInfo * getVm(const QString VmID) const;


private:
    QMap<QString, VmInfo*> m_vms;
    QList<QVariant> m_runningVms;
    SSHConnection   *m_ssh;
    MainWindow      *m_mainWindow;

    void addVmToMap(QVariantMap jsonVm);
    void clearVms();
    void setTunnelState(bool state, quint16 port);
    void vrdeCheckConnections(const QString VmID); // true if VM has a remote session



signals:
    void recreateVmList();
    void vmStarted(const QString VmID);
    void vmStopped(const QString VmID);
    void usersList(const QStringList users);
    void tunnelStateChanged(bool connected, const QString VmID);
    void cantStopVm(const QString VmID);

public slots:
    void onClientConnected();
    void onClientDisconnected();
    void setTunnling(bool tunnel, const QString VmID);

private slots:
    void allVmsResponse(SSHJob *job);
    void runningVmsResponse(SSHJob *job);
    void vrdeResponse(SSHJob *job);
    void startResponse(SSHJob *job);
    void stopResponse(SSHJob *job);
    void usersResponse(SSHJob *job);
    void onTunnelComplete(quint16 port);
    void onTunnelDestroyed(quint16 port);
};

#endif // VMSCONTROLLER_H
