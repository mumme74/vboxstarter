#ifndef PASSPHRASE_H
#define PASSPHRASE_H

#include <QDialog>

namespace Ui {
    class PassPhrase;
}

class PassPhrase : public QDialog
{
    Q_OBJECT


public:
    explicit PassPhrase(QWidget *parent = 0);
    ~PassPhrase();

    QString value();

public slots:

private:
    Ui::PassPhrase *ui;
};

#endif // PASSPHRASE_H
