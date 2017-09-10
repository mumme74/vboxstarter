#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
    class MainWindow;
}


class VmsController;
class AppController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    void init(AppController *parent);
    ~MainWindow();



private:

    Ui::MainWindow *ui;
    VmsController *m_vmsController;

    void setVmState(QString VmID, bool changeListItem);

public slots:
    void onMainConnected();
    void onMainDisconnected();
    void onRemoteProgramStarted();
    void onRemoteProgramStopped(int code);
    void onRecreateVmList();
    void onVmStarted(const QString VmID);
    void onVmStopped(const QString VmID);
    void updateUserList(const QStringList users);

protected slots:
    void connectMainPressed(bool connect);
    void startVmPressed(bool start);
    void connectToVmPressed(bool connect);
    void saveSettings();
    void browseForKeyfilePrivate();
    void browseForKeyfilePublic();
    void selectedVmChanged();
    void onTabSwitch(int tabIndex);

signals:
    void startVm(bool start, QString vm);
    void connectToVm(bool connect, const QString VmID);
    void connectMainSSH();
    void disconnectMainSSH();
};

#endif // MAINWINDOW_H
