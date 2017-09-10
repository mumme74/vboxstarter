#include "passphrase.h"
#include "ui_passphrase.h"

PassPhrase::PassPhrase(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PassPhrase)
{
    ui->setupUi(this);
    ui->txtPassPhrase->setFocus();
}

PassPhrase::~ PassPhrase()
{
    delete ui;
}

QString PassPhrase::value()
{
    return ui->txtPassPhrase->text();
}
