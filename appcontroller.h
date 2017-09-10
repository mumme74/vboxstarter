#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QMap>
#include <QProcess>
class MainWindow;
class SSHConnection;
class VmsController;
class VmInfo;



// the allmighty AppController wich biand together all the pieces
class AppController : public QObject
{
    Q_OBJECT
public:
    explicit AppController(QObject *parent = 0);
    ~AppController();

    MainWindow *getMainWindow() const;
    SSHConnection *getSSHConnection() const;
    VmsController *getVmsController() const;
    void show();

private:
    MainWindow *m_mainWindow;
    SSHConnection *m_sshConnection;
    VmsController *m_vmsController;
    QString       m_lastVm;
    QMap<QString, QProcess*> m_rdpProcesses;

signals:

public slots:

private slots:
    void controlRdp(bool connect, const QString VmID);
    void onRdpProgramError(QProcess::ProcessError error);
    void onRdpProgramStateChange(QProcess::ProcessState newState);
    void startRdpProgram();

};

#endif // APPCONTROLLER_H
