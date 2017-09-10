#include "appcontroller.h"
#include "mainwindow.h"
#include "sshconnection.h"
#include "vmscontroller.h"
#include "settings.h"
#include <QProcess>
#include <QTimer>

AppController::AppController(QObject *parent) :
    QObject(parent)
{
    m_sshConnection = new SSHConnection(this);
    m_vmsController = new VmsController(this);
    m_mainWindow = new MainWindow;

    m_vmsController->init();
    m_mainWindow->init(this);

    // ssh main connection
    connect(m_sshConnection, SIGNAL(connectedToServer()), m_mainWindow, SLOT(onMainConnected()));
    connect(m_sshConnection, SIGNAL(disconnectedFromServer()), m_mainWindow, SLOT(onMainDisconnected()));
    connect(m_sshConnection, SIGNAL(remoteProcessStarted()), m_mainWindow, SLOT(onRemoteProgramStarted()));
    connect(m_sshConnection, SIGNAL(remoteProcessStopped(int)), m_mainWindow, SLOT(onRemoteProgramStopped(int)));
    connect(m_mainWindow, SIGNAL(connectMainSSH()), m_sshConnection, SLOT(connectSSH()));
    connect(m_mainWindow, SIGNAL(disconnectMainSSH()), m_sshConnection, SLOT(disconnectSSH()));

    // vms controller
    connect(m_sshConnection, SIGNAL(remoteProcessStarted()), m_vmsController, SLOT(onClientConnected()));
    connect(m_sshConnection, SIGNAL(disconnectedFromServer()), m_vmsController, SLOT(onClientDisconnected()));

    // redirect port
    connect(m_mainWindow, SIGNAL(connectToVm(bool,QString)),
            m_vmsController, SLOT(setTunnling(bool,QString)));
    connect(m_vmsController, SIGNAL(tunnelStateChanged(bool,QString)),
            this, SLOT(controlRdp(bool,QString)));


    // ui connctions
    connect(m_vmsController, SIGNAL(recreateVmList()),
            m_mainWindow, SLOT(onRecreateVmList()));
    connect(m_vmsController, SIGNAL(vmStarted(QString)),
            m_mainWindow, SLOT(onVmStarted(QString)));
    connect(m_vmsController, SIGNAL(cantStopVm(QString)),
            m_mainWindow, SLOT(onVmStarted(QString)));
    connect(m_vmsController, SIGNAL(vmStopped(QString)),
            m_mainWindow, SLOT(onVmStopped(QString)));
    connect(m_vmsController, SIGNAL(usersList(QStringList)),
            m_mainWindow,SLOT(updateUserList(QStringList)));
}

AppController::~AppController()
{
    m_mainWindow->deleteLater();
}



MainWindow *AppController::getMainWindow() const
{
    return m_mainWindow;
}

SSHConnection *AppController::getSSHConnection() const
{
    return m_sshConnection;
}

VmsController *AppController::getVmsController() const
{
    return m_vmsController;
}

void AppController::controlRdp(bool connect, const QString VmID)
{
    const VmInfo *vm = m_vmsController->getVm(VmID);
    if (connect){
        if (vm->port() > 0) {
            // delay the start a bit helps cord on mac
            m_lastVm = VmID;
            QTimer *tmr = new QTimer(this);
            tmr->setSingleShot(true);
            this->connect(tmr, SIGNAL(timeout()), this, SLOT(startRdpProgram()));
            tmr->start(300);
        }
    } else {
        if (m_rdpProcesses.contains(vm->name())) {
            QProcess *process = m_rdpProcesses.take(vm->name());
            process->close();
            process->deleteLater();
        }
    }
}

void AppController::onRdpProgramError(QProcess::ProcessError error)
{
    qDebug()<<"Process error from rdpProgram:" << error << endl;
}

void AppController::onRdpProgramStateChange(QProcess::ProcessState newState)
{
    qDebug()<<"Rdpprogram state changed: "<<newState <<endl;
}

void AppController::show()
{
    m_mainWindow->show();
}

void AppController::startRdpProgram()
{
    if (m_lastVm.isEmpty()) {
        return;
    }
     const VmInfo *vm = m_vmsController->getVm(m_lastVm);

    Settings settings;
    QString prog = settings.myValue("rdpprogram").toString();
    prog = prog.replace("[port]", QString::number(vm->port()));

    QProcess *rdp = new QProcess(this);
    //rdp->setStandardOutputFile("/tmp/rdesktop-stdout");
    //rdp->setStandardErrorFile("/tmp/rdesktop-error");
    this->connect(rdp, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(onRdpProgramError(QProcess::ProcessError)));
    this->connect(rdp, SIGNAL(stateChanged(QProcess::ProcessState)),
            this, SLOT(onRdpProgramStateChange(QProcess::ProcessState)));

    rdp->start(prog);

    m_rdpProcesses[vm->name()] = rdp;
    rdp->waitForStarted();
    qDebug()<<"rdp status:"<<rdp->state()<< " error:" << rdp->errorString()<< endl;

    m_lastVm = "";

}
