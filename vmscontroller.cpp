#include "vmscontroller.h"
#include <QxtJSON>
#include <QMessageBox>
#include "settings.h"
#include "sshconnection.h"
#include "appcontroller.h"
#include "mainwindow.h"





struct Server {
public:
    enum cmds{
        ListAll, //"{\"command\":\"listall\"}\n"
        ListRunning, //"{\"command\":\"listrunning\"}\n"
        VrdeConnected, //"{\"command\":\"getvrde\",\"arg\":"vm""}"
        Start, //"{\"command\":\"start\",\"arg\":\""vm"\"}"
        Stop, //"{\"command\":\"stop\",\"arg\":\""vm"\"}"
        Users
    };

    static QString const cmd(Server::cmds sendCmd, const QString arg = "")
    {
        switch (sendCmd){
            case Server::ListAll:
                return Server::cmd("listall");
            case Server::ListRunning:
                return Server::cmd("listrunning");
            case Server::VrdeConnected:
                return Server::cmd("getvrde", arg);
            case Server::Start:
                return Server::cmd("start", arg);
            case Server::Stop:
                return Server::cmd("stop", arg);
            case Server::Users:
                return Server::cmd("users");
            default:
            return "";
        }
    }

    static QString const cmd(const QString sendCmd, const QString arg = "")
    {
        QMap<QString, QVariant> root;
        root["command"] = sendCmd;
        if (arg != "") {
            root["arg"] = arg;
        }

        QString res = QxtJSON::stringify(QVariant(root));
        return res;
    }

    static QVariant result(const QString json)
    {
        if (json.isEmpty()){
            return QVariant();
        }

        QVariant resp = QxtJSON::parse(json);
        if (!resp.isNull() &&
            resp.canConvert(QVariant::Map)
        ) {

            QVariantMap res = resp.toMap();
            resp = res["result"];
            return resp;
        }
        return QVariant();
    }
};

VmsController::VmsController(QObject *parent) :
    QObject(parent)
{

}

void VmsController::init()
{
    AppController *controller = qobject_cast<AppController*>(parent());
    m_ssh = controller->getSSHConnection();
    m_mainWindow = controller->getMainWindow();

    connect(m_ssh, SIGNAL(redirectComplete(quint16)), this, SLOT(onTunnelComplete(quint16)));
    connect(m_ssh, SIGNAL(redirectDisconnected(quint16)), this, SLOT(onTunnelDestroyed(quint16)));
}

void VmsController::addVmToMap(QVariantMap jsonVm)
{
    VmInfo *vm = new VmInfo(this);
    vm->setName(jsonVm["name"].toString());
    vm->setDescription(jsonVm["description"].toString());
    vm->setPort(jsonVm["port"].toInt());
    QStringList programs = jsonVm["program"].toStringList();
    vm->setPrograms(programs);
    m_vms[jsonVm["name"].toString()] = vm;
}

void VmsController::allVmsResponse(SSHJob *job)
{
    QString response = job->response();
    QVariant resp = Server::result(response);
    if (!resp.isNull()){
        foreach(QVariant jsonVm, resp.toList()) {
            addVmToMap(jsonVm.toMap());
        }
    }
}

const QList<QString> VmsController::availableVms() const
{
    QList<QString> ret;
    if (m_vms.empty()){
        return ret;
    }

    foreach(VmInfo *vm, m_vms){
        ret.append(vm->name());
    }

    return ret;
}

void VmsController::clearVms()
{
    foreach(VmInfo *vm, m_vms){
        vm->deleteLater();
    }
    m_vms.clear();
}


const VmInfo *VmsController::getVm(const QString VmID) const
{
    if (m_vms.contains(VmID)){
        return m_vms[VmID];
    }
    return new VmInfo;
}

void VmsController::onClientConnected()
{
    refresh();
}

void VmsController::onClientDisconnected()
{
    clearVms();
    emit recreateVmList();
}


void VmsController::onTunnelComplete(quint16 port)
{
    setTunnelState(true, port);
}

void VmsController::onTunnelDestroyed(quint16 port)
{
    setTunnelState(false, port);
}



void VmsController::refresh()
{
    clearVms();

    SSHJob *job = m_ssh->sendCmd(Server::cmd(Server::ListAll));
    connect(job, SIGNAL(finished(SSHJob*)), this, SLOT(allVmsResponse(SSHJob*)));

    job = m_ssh->sendCmd(Server::cmd(Server::ListRunning));
    connect(job, SIGNAL(finished(SSHJob*)), this, SLOT(runningVmsResponse(SSHJob*)));
}



void VmsController::runningVmsResponse(SSHJob *job)
{
    QString response = job->response();
    QVariant resp = Server::result(response);
    if (!resp.isNull()){
        foreach(QVariant tmp, resp.toList()) {
            QVariantMap jsonVm = tmp.toMap();
            if (!m_vms.contains(jsonVm["name"].toString())){
                addVmToMap(jsonVm);
            }
            m_vms[jsonVm["name"].toString()]->setRunning(true);
        }
    }

    if (!m_vms.empty()) {
        emit recreateVmList();
    }
}

void VmsController::setTunnelState(bool state, quint16 port)
{
    QString VmID;
    foreach(VmInfo *vm, m_vms){
        if (vm->port() == port) {
            vm->setTunneled(state);
            VmID = vm->name();
            break;
        }
    }
    if (!VmID.isEmpty()) {
        emit tunnelStateChanged(state, VmID);
    }
}

void VmsController::setTunnling(bool tunnel, const QString VmID)
{
    if (!m_vms.contains(VmID)){
        qWarning()<<"hittar inte vm med namnet "<<VmID<<endl;
        return;
    }

    VmInfo *vm = m_vms[VmID];
    if (vm->port() > 0) {
        if (vm->tunneled() && !tunnel){
            vm->setTunneled(false);
            m_ssh->redirectPort(false,vm->port());
        } else if (!vm->tunneled() && tunnel){
            vm->setTunneled(true);
            m_ssh->redirectPort(true, vm->port());
        }
    }
}

void VmsController::start(const QString VmID)
{
    if (!m_vms.contains(VmID)){
        qWarning()<<"VM med id: "<<VmID << "finns inte"<<endl;
        return;
    }
    m_vms[VmID]->setPending(true);
    SSHJob *job = m_ssh->sendCmd(Server::cmd(Server::Start, VmID));
    connect(job, SIGNAL(finished(SSHJob*)), this, SLOT(startResponse(SSHJob*)));
}

void VmsController::startResponse(SSHJob *job)
{
    QString response = job->response();
    QVariant resp = Server::result(response);
    if (!resp.isNull()){
        response = resp.toString();
        if (response.startsWith("-**- VMnamnet")) {
            QMessageBox::information(m_mainWindow, trUtf8("Fel vid start"), trUtf8(response.toAscii()));
            return;
        }

        QVariantMap cmd = QxtJSON::parse(job->cmd()).toMap();
        if(m_vms.contains(cmd["arg"].toString())){
            VmInfo *vm = m_vms[cmd["arg"].toString()];
            vm->setRunning(true);
            vm->setPending(false);
            emit vmStarted(cmd["arg"].toString());
        }
    }
}

void VmsController::stop(const QString VmID)
{
    if (!m_vms.contains(VmID)){
        qWarning()<<"VM med id: "<<VmID << "finns inte"<<endl;
        return;
    }
    m_vms[VmID]->setPending(true);

    vrdeCheckConnections(VmID);
}

void VmsController::stopResponse(SSHJob *job)
{
    QString response = job->response();
    QVariant resp = Server::result(response);
    if (!resp.isNull()){
        response = resp.toString();
        if (response.startsWith("** VMnamnet")) {
            QMessageBox::information(m_mainWindow, trUtf8("Fel vid stop"), trUtf8(response.toAscii()), QMessageBox::Ok);
            return;
        }

        QVariantMap cmd = QxtJSON::parse(job->cmd()).toMap();
        if(m_vms.contains(cmd["arg"].toString())){
            VmInfo *vm = m_vms[cmd["arg"].toString()];
            vm->setRunning(false);
            vm->setPending(false);
            emit vmStopped(cmd["arg"].toString());
        }
    }
}

void VmsController::updateUsers()
{
    SSHJob *job = m_ssh->sendCmd(Server::cmd(Server::Users));
    connect(job, SIGNAL(finished(SSHJob*)), this, SLOT(usersResponse(SSHJob*)));
}

void VmsController::usersResponse(SSHJob *job)
{
    QString response = job->response();
    QVariant resp = Server::result(response);
    if (!resp.isNull()){
        QStringList users = resp.toStringList();
        emit usersList(users);
    }
}


void VmsController::vrdeCheckConnections(const QString VmID)
{
    if (!m_vms.contains(VmID)) {
        return;
    }

    SSHJob *job = m_ssh->sendCmd(Server::cmd(Server::VrdeConnected, VmID));
    connect(job, SIGNAL(finished(SSHJob*)), this, SLOT(vrdeResponse(SSHJob*)));

}

void VmsController::vrdeResponse(SSHJob *job)
{
    QVariantMap response = QxtJSON::parse(job->response()).toMap();
    QVariantMap cmd = QxtJSON::parse(job->cmd()).toMap();
    QString VmID;
    if (!cmd.isEmpty() && cmd.contains("arg")){
        VmID = cmd["arg"].toString();
    }


    if (!response.isEmpty() && response.contains("result")){
        QVariantMap result = response["result"].toMap();
        if (result["have_connections"].toBool()) {
            QMessageBox::warning(m_mainWindow, trUtf8("Kan ej stoppa VM"),
                                 trUtf8("VM %0 har anslutningar, kan ej stoppa den.\n"
                                        "det vore extremt oartigt mot övriga anslutna!")
                                 .arg(cmd["arg"].toString()));
            m_vms[VmID]->setPending(false);
            emit cantStopVm(VmID);
            return;

        } else {
            SSHJob *job = m_ssh->sendCmd(Server::cmd(Server::Stop, VmID));
            connect(job, SIGNAL(finished(SSHJob*)), this, SLOT(stopResponse(SSHJob*)));
            return;
        }

    }

    qWarning()<< "error returned "<< response["error"].toString() << endl;

    m_vms[VmID]->setPending(false);
    emit cantStopVm(VmID);
}
