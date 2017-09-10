#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settings.h"
#include "vmscontroller.h"
#include "appcontroller.h"
#include <QtGui>

#define VMSTOPPED_CAPTION "Starta"
#define VMSTARTED_CAPTION "Stoppa"
#define VMSTOPPING_CAPTION "Stoppar..."
#define VMSTARTING_CAPTION "Startar..."

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_vmsController(0)
{

    ui->setupUi(this);

    Settings settings;
    move(settings.value("pos", QPoint(200,200)).toPoint());
    ui->txtKeyFilePrivate->setText(settings.myValue("keyfileprivate").toString());
    ui->txtKeyFilePublic->setText(settings.myValue("keyfilepublic").toString());
    ui->txtServerURL->setText(settings.myValue("server").toString());
    ui->txtUserName->setText(settings.myValue("username").toString());
    ui->txtServerCmd->setPlainText(settings.myValue("serverClient").toString());
    ui->spinPort->setValue(settings.myValue("port").toInt());
    ui->txtRdpProgram->setText(settings.myValue("rdpprogram").toString());

    ui->tabWidget->setCurrentIndex(0); // default to vms tab

    connect(ui->btnConnect,SIGNAL(toggled(bool)), this, SLOT(connectMainPressed(bool)));
    connect(ui->btnStartStop, SIGNAL(toggled(bool)), this, SLOT(startVmPressed(bool)));
    connect(ui->btnSaveSettings, SIGNAL(clicked()), this, SLOT(saveSettings()));
    connect(ui->btnFileBrowsePrivate, SIGNAL(clicked()), this, SLOT(browseForKeyfilePrivate()));
    connect(ui->btnFileBrowsePublic, SIGNAL(clicked()), this, SLOT(browseForKeyfilePublic()));
    connect(ui->btnConnectRDP, SIGNAL(toggled(bool)), this, SLOT(connectToVmPressed(bool)));

    connect(ui->listVms, SIGNAL(currentRowChanged(int)), this, SLOT(selectedVmChanged()));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(onTabSwitch(int)));
}

void MainWindow::init(AppController *parent)
{
    m_vmsController = parent->getVmsController();
}

MainWindow::~MainWindow()
{
    Settings settings;
    settings.setValue("pos", pos());

    delete ui;
}


void MainWindow::browseForKeyfilePrivate()
{
    QString fileName = ui->txtKeyFilePrivate->text();
    fileName = QFileDialog::getOpenFileName(this, trUtf8("Bläddra fram din privata SSH nyckel"), fileName);

    if (!fileName.isEmpty()){
        ui->txtKeyFilePrivate->setText(fileName);
    }

}

void MainWindow::browseForKeyfilePublic()
{
    QString fileName = ui->txtKeyFilePublic->text();
    fileName = QFileDialog::getOpenFileName(this, trUtf8("Bläddra fram din publika SSH nyckel"), fileName, trUtf8("Alla filer (*.*)"));

    if (!fileName.isEmpty()){
        ui->txtKeyFilePublic->setText(fileName);
    }

}

void MainWindow::connectMainPressed(bool connect)
{
    // user toggled mainconnect button
    if (connect) {
        emit connectMainSSH();
    } else {
        emit disconnectMainSSH();
    }
}


void MainWindow::connectToVmPressed(bool connect)
{
    if (!ui->listVms->selectedItems().empty()){
        emit connectToVm(connect, ui->listVms->selectedItems().first()->text());
    }
}

void MainWindow::onMainConnected()
{
    ui->lblMainStatus->setText(trUtf8("Ansluten till server, startar klient"));
}

void MainWindow::onMainDisconnected()
{
    ui->lblMainStatus->setText(trUtf8("Nedkopplad"));
    ui->btnConnect->setChecked(false);
}

void MainWindow::onRecreateVmList()
{
    ui->listVms->clear();
    ui->lblVMDescription->setText("");
    foreach(QString VmID, m_vmsController->availableVms()){
        QListWidgetItem *item = new QListWidgetItem(VmID, ui->listVms);
        const VmInfo *vm = m_vmsController->getVm(VmID);
        if (vm && vm->running()) {
            item->setIcon(QIcon(":/play.png"));
        } else {
            item->setIcon(QIcon(":/pause.png"));
        }
    }
}

void MainWindow::onRemoteProgramStarted()
{
    ui->lblMainStatus->setText(trUtf8("Serverklient startad!"));
}

void MainWindow::onRemoteProgramStopped(int /*code*/)
{
    ui->lblMainStatus->setText(trUtf8("Serverklient stoppad, dvs. problem..."));
}

void MainWindow::onTabSwitch(int tabIndex)
{
    if (tabIndex == 1) {
        // users tab
        if (m_vmsController){
            ui->listUsers->clear();
            m_vmsController->updateUsers();
        }
    }
}

void MainWindow::onVmStarted(const QString VmID)
{
    setVmState(VmID, true);
}

void MainWindow::onVmStopped(const QString VmID)
{
   setVmState(VmID, true);
}

void MainWindow::setVmState(QString VmID, bool changeListItem)
{
    QString img;
    QString caption;
    bool checked = false;
    bool enabled = true;

    const VmInfo *vm = m_vmsController->getVm(VmID);
    if (vm->running()) {
        if (vm->pending()) {
            img = ":/pending.png";
            caption = VMSTOPPING_CAPTION;
            enabled = false;
        } else {
            img = ":/play.png";
            caption = VMSTARTED_CAPTION;
            checked = true;
        }
    } else {
        if (vm->pending()) {
            img = ":/pending.png";
            caption = VMSTARTING_CAPTION;
            enabled = false;
        } else {
            img = ":/pause.png";
            caption = VMSTOPPED_CAPTION;
        }
    }


    if (ui->listVms->currentItem() &&
         ui->listVms->currentItem()->text() == VmID
    ){
        ui->btnStartStop->blockSignals(true);
        ui->btnStartStop->setChecked(checked);
        ui->btnStartStop->setEnabled(enabled);
        ui->btnStartStop->blockSignals(false);
        ui->btnStartStop->setText(caption);


        ui->btnConnectRDP->blockSignals(true);
        ui->btnConnectRDP->setChecked(vm->tunneled());
        ui->btnConnectRDP->setEnabled(vm->running());
        ui->btnConnectRDP->blockSignals(false);
    }

    if (changeListItem) {
        QList<QListWidgetItem *> items = ui->listVms->findItems(VmID, Qt::MatchExactly);
        if (!items.empty()){
            items.first()->setIcon(QIcon(img));
        }
    }
}

void MainWindow::saveSettings()
{
    Settings settings;
    settings.setValue("keyfileprivate", ui->txtKeyFilePrivate->text());
    settings.setValue("keyfilepublic", ui->txtKeyFilePublic->text());
    settings.setValue("server", ui->txtServerURL->text());
    settings.setValue("username", ui->txtUserName->text());
    settings.setValue("serverClient", ui->txtServerCmd->toPlainText());
    settings.setValue("port", ui->spinPort->value());
    settings.setValue("rdpprogram", ui->txtRdpProgram->text());
}

void MainWindow::selectedVmChanged()
{
    QListWidgetItem *item = ui->listVms->currentItem();
    if (!item) {
        return;
    }

    QString VmID = item->text();
    if (m_vmsController){
        // start stop button
        setVmState(VmID, false);

        // Description
        const VmInfo *vm = m_vmsController->getVm(VmID);
        QString desc = "<p><b>" + vm->description() + "</b></p>";
        desc += "<ul type='circle' style='margin:0;padding:0;'>";
        foreach(QString prog,  vm->programs()){
            desc += "<li>" + prog + "</li>";
        }
        desc += "</ul>";


        ui->lblVMDescription->setHtml(desc);
    }
}

void MainWindow::startVmPressed(bool start)
{
    QList<QListWidgetItem *> selected = ui->listVms->selectedItems();
    if (!selected.empty()){
        if (m_vmsController) {
            QString vm = selected.first()->text();
            if (start) {
                m_vmsController->start(vm);
                setVmState(vm, true);
            } else {
                m_vmsController->stop(vm);
                setVmState(vm, true);
            }
        }
        emit startVm(start, ui->listVms->selectedItems().first()->text());
    }
}

void MainWindow::updateUserList(const QStringList users)
{
    foreach(const QString user, users){
        new QListWidgetItem(user, ui->listUsers);
    }
}
