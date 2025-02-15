/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp ./dde-session-shell/xml/com.huawei.switchos.xml -a ./dde-session-shell/toolGenerate/qdbusxml2cpp/com.huawei.switchosAdaptor -i ./dde-session-shell/toolGenerate/qdbusxml2cpp/com.huawei.switchos.h
 *
 * qdbusxml2cpp is Copyright (C) 2017 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#include "./dde-session-shell/toolGenerate/qdbusxml2cpp/com.huawei.switchosAdaptor.h"
#include <QtCore/QMetaObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

/*
 * Implementation of adaptor class SwitchosAdaptor
 */

SwitchosAdaptor::SwitchosAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    // constructor
    setAutoRelaySignals(true);
}

SwitchosAdaptor::~SwitchosAdaptor()
{
    // destructor
}

uchar SwitchosAdaptor::getOsFlag()
{
    // handle method call com.huawei.switchos.getOsFlag
    uchar flag;
    QMetaObject::invokeMethod(parent(), "getOsFlag", Q_RETURN_ARG(uchar, flag));
    return flag;
}

uchar SwitchosAdaptor::isDualOsSwitchAvail()
{
    // handle method call com.huawei.switchos.isDualOsSwitchAvail
    uchar isAvail;
    QMetaObject::invokeMethod(parent(), "isDualOsSwitchAvail", Q_RETURN_ARG(uchar, isAvail));
    return isAvail;
}

uint SwitchosAdaptor::setOsFlag(uchar flag)
{
    // handle method call com.huawei.switchos.setOsFlag
    uint result;
    QMetaObject::invokeMethod(parent(), "setOsFlag", Q_RETURN_ARG(uint, result), Q_ARG(uchar, flag));
    return result;
}

