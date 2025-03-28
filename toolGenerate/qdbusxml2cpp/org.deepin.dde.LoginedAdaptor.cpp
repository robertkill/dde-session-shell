/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp ./dde-session-shell/xml/org.deepin.dde.Logined.xml -a ./dde-session-shell/toolGenerate/qdbusxml2cpp/org.deepin.dde.LoginedAdaptor -i ./dde-session-shell/toolGenerate/qdbusxml2cpp/org.deepin.dde.Logined.h
 *
 * qdbusxml2cpp is Copyright (C) 2017 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#include "./dde-session-shell/toolGenerate/qdbusxml2cpp/org.deepin.dde.LoginedAdaptor.h"
#include <QtCore/QMetaObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

/*
 * Implementation of adaptor class LoginedAdaptor
 */

LoginedAdaptor::LoginedAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    // constructor
    setAutoRelaySignals(true);
}

LoginedAdaptor::~LoginedAdaptor()
{
    // destructor
}

uint LoginedAdaptor::lastLogoutUser() const
{
    // get the value of property LastLogoutUser
    return qvariant_cast< uint >(parent()->property("LastLogoutUser"));
}

QString LoginedAdaptor::userList() const
{
    // get the value of property UserList
    return qvariant_cast< QString >(parent()->property("UserList"));
}

