#-------------------------------------------------
#
# Project created by QtCreator 2012-04-01T22:35:39
#
#-------------------------------------------------

QT       += core gui
CONFIG   += qxt
QXT      += core network

TARGET = vboxstarter
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    settings.cpp \
    vmscontroller.cpp \
    sshconnection.cpp \
    redirecttcpthread.cpp \
    passphrase.cpp \
    appcontroller.cpp

HEADERS  += mainwindow.h \
    settings.h \
    vmscontroller.h \
    sshconnection.h \
    redirecttcpthread.h \
    passphrase.h \
    appcontroller.h

FORMS    += mainwindow.ui \
    passphrase.ui

RESOURCES += \
    resources.qrc
