#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSettings>
#include <QCoreApplication>

class Settings : public QSettings
{
    Q_OBJECT
public:
    explicit Settings(QObject *parent = 0);

    QVariant myValue(const QString &key) const;

    static void setDefaults()
    {
        // set some defaults and store it in Application Data instead ocf within registry
        QCoreApplication::setOrganizationName("mummesoft");
        QCoreApplication::setOrganizationDomain("mumme.se");
        QCoreApplication::setApplicationName("vboxstarter");
        QSettings::setDefaultFormat(QSettings::IniFormat);
    }

signals:

public slots:

};

#endif // SETTINGS_H
