#include "authinterface.h"
#include "sessionbasemodel.h"
#include "userinfo.h"

#include <grp.h>
#include <libintl.h>
#include <pwd.h>
#include <unistd.h>
#include <QProcessEnvironment>
#include <stdio.h>

#define POWER_CAN_SLEEP "POWER_CAN_SLEEP"
#define POWER_CAN_HIBERNATE "POWER_CAN_HIBERNATE"

const QString force_hiberbate_1 = "systemd.force-hibernate=1";
const QString force_hiberbate_true = "systemd.force-hibernate=true";
const QString force_hiberbate_yes = "systemd.force-hibernate=yes";
const QString cmd_path = "/proc/cmdline";

using namespace Auth;

static std::pair<bool, qint64> checkIsPartitionType(const QStringList &list)
{
    std::pair<bool, qint64> result{ false, -1 };

    if (list.length() != 5) {
        return result;
    }

    const QString type{ list[1] };
    const QString size{ list[2] };

    result.first  = type == "partition";
    result.second = size.toLongLong() * 1024.0f;

    return result;
}

static qint64 get_power_image_size()
{
    qint64 size{ 0 };
    QFile  file("/sys/power/image_size");

    if (file.open(QIODevice::Text | QIODevice::ReadOnly)) {
        size = file.readAll().trimmed().toLongLong();
        file.close();
    }

    return size;
}

static std::shared_ptr<User> createUser(const QString &path)
{
    uint uid = 0;
    sscanf(path.toLatin1().data(), "/com/deepin/daemon/Accounts/User%u", &uid);
    std::shared_ptr<User> usr_ptr;
    if (uid > 10000) {
        struct passwd *pws;
        pws = getpwuid(uid);

        usr_ptr = std::shared_ptr<User>(new ADDomainUser(path, uid));
        static_cast<ADDomainUser *>(usr_ptr.get())->setUserDisplayName(pws->pw_name);
        static_cast<ADDomainUser *>(usr_ptr.get())->setUserName(pws->pw_name);
        qDebug() << "AuthInterface add domain user:" << uid;
    } else {
        usr_ptr = std::shared_ptr<User>(new NativeUser(path));
    }

    return usr_ptr;
}

AuthInterface::AuthInterface(SessionBaseModel *const model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_accountsInter(new AccountsInter(ACCOUNT_DBUS_SERVICE, ACCOUNT_DBUS_PATH, QDBusConnection::systemBus(), this))
    , m_loginedInter(new LoginedInter(ACCOUNT_DBUS_SERVICE, "/com/deepin/daemon/Logined", QDBusConnection::systemBus(), this))
    , m_login1Inter(new DBusLogin1Manager("org.freedesktop.login1", "/org/freedesktop/login1", QDBusConnection::systemBus(), this))
    , m_lastLogoutUid(0)
    , m_loginUserList(0)
{
    if (!m_login1Inter->isValid()) {
        qWarning() << "m_login1Inter:" << m_login1Inter->lastError().type();
    }
    if (QGSettings::isSchemaInstalled("com.deepin.dde.sessionshell.control")) {
        m_gsettings = new QGSettings("com.deepin.dde.sessionshell.control", "/com/deepin/dde/sessionshell/control/", this);
    }
}

void AuthInterface::switchToUser(std::shared_ptr<User> user)
{
    Q_UNUSED(user)
    onLoginUserListChanged(m_loginedInter->GetLoginedUsers());
}

void AuthInterface::setLayout(std::shared_ptr<User> user, const QString &layout) {
    user->setCurrentLayout(layout);
}

void AuthInterface::onUserListChanged(const QStringList &list)
{
    qDebug() << Q_FUNC_INFO << list;
    QStringList tmpList;
    for (std::shared_ptr<User> u : m_model->userList()) {
        tmpList << QString("/com/deepin/daemon/Accounts/User%1").arg(u->uid());
    }

    for (const QString &u : list) {
        std::shared_ptr<User> usr_ptr = createUser(u);
        usr_ptr->setisLogind(this->isLogined(usr_ptr->uid()));

        if (!tmpList.contains(u)) {
            tmpList << u;
            onUserAdded(u, usr_ptr);
        }
    }

    for (const QString &u : tmpList) {
        if (!list.contains(u)) {
            onUserRemove(u);
        }
    }
}

void AuthInterface::onUserAdded(const QString &user, std::shared_ptr<User> user_ptr)
{
    user_ptr->setisLogind(isLogined(user_ptr->uid()));
    m_model->userAdd(user_ptr);
}

void AuthInterface::onUserRemove(const QString &user)
{
    QList<std::shared_ptr<User>> list = m_model->userList();

    for (auto u : list) {
        if (u->path() == user && u->type() == User::Native) {
            m_model->userRemoved(u);
            break;
        }
    }
}

void AuthInterface::initData()
{
    onUserListChanged(m_accountsInter->userList());
    onLoginUserListChanged(m_loginedInter->userList());
    onLastLogoutUserChanged(m_loginedInter->lastLogoutUser());
    checkConfig();
    checkPowerInfo();
    checkVirtualKB();
}

void AuthInterface::initDBus()
{
    m_accountsInter->setSync(true);
    m_loginedInter->setSync(true);

    connect(m_accountsInter, &AccountsInter::UserListChanged, this, &AuthInterface::onUserListChanged, Qt::QueuedConnection);
    connect(m_accountsInter, &AccountsInter::UserAdded, this, [this](QString path){
        std::shared_ptr<User> usr_ptr = createUser(path);
        usr_ptr->setisLogind(this->isLogined(usr_ptr->uid()));
        this->onUserAdded(path, usr_ptr);
    }, Qt::QueuedConnection);
    connect(m_accountsInter, &AccountsInter::UserDeleted, this, &AuthInterface::onUserRemove, Qt::QueuedConnection);

    connect(m_loginedInter, &LoginedInter::LastLogoutUserChanged, this, &AuthInterface::onLastLogoutUserChanged);
    connect(m_loginedInter, &LoginedInter::UserListChanged, this, &AuthInterface::onLoginUserListChanged);
}

void AuthInterface::onLastLogoutUserChanged(uint uid)
{
    m_lastLogoutUid = uid;

    QList<std::shared_ptr<User>> userList = m_model->userList();
    for (auto it = userList.constBegin(); it != userList.constEnd(); ++it) {
        if ((*it)->uid() == uid) {
            m_model->setLastLogoutUser((*it));
            return;
        }
    }

    m_model->setLastLogoutUser(std::shared_ptr<User>(nullptr));
}

void AuthInterface::onLoginUserListChanged(const QString &list)
{
    qDebug() << Q_FUNC_INFO << list ;
    m_loginUserList.clear();

    std::list<uint> availableUidList;
    for (std::shared_ptr<User> user : m_model->userList()) {
        availableUidList.push_back(user->uid());
    }

    QJsonObject userList = QJsonDocument::fromJson(list.toUtf8()).object();
    for (auto it = userList.constBegin(); it != userList.constEnd(); ++it) {
        const bool haveDisplay = checkHaveDisplay(it.value().toArray());
        const uint uid         = it.key().toUInt();

        // skip not have display users
        if (haveDisplay) {
            m_loginUserList.push_back(uid);
        }
    }

    QList<std::shared_ptr<User>> uList = m_model->userList();
    for (auto it = uList.begin(); it != uList.end();) {
        std::shared_ptr<User> user = *it;
        user->setisLogind(isLogined(user->uid()));
        ++it;
    }

    if(m_model->isServerModel())
        emit m_model->userListLoginedChanged(m_model->logindUser());
}

bool AuthInterface::checkHaveDisplay(const QJsonArray &array)
{
    for (auto it = array.constBegin(); it != array.constEnd(); ++it) {
        const QJsonObject &obj = (*it).toObject();

        // In X11,if user without desktop , this is system service, need skip.
        // In wayland, it is the same.
        if (!obj["Desktop"].toString().isEmpty()){
            return true;
        }
    }

    return false;
}

bool AuthInterface::gsettingsExist(const QString& key)
{
    return m_gsettings != nullptr && m_gsettings->keys().contains(key);
}

QVariant AuthInterface::getGSettings(const QString& key)
{
    QVariant value = true;
    if (m_gsettings != nullptr && m_gsettings->keys().contains(key)) {
        value = m_gsettings->get(key);
    }
    return value;
}

bool AuthInterface::checkIsADDomain(const QString &key)
{
    //当没有安装realmd或没有加入AD域时，返回为空
    QProcess process;
    QString command = "";
    if(key == "lock") {
        command = "realm list";
    } else if(key == "greeter"){
        //因为登录界面时，用户是lightdm，直接realm list，会返回为空，所以需要sudo
        command = "sudo realm list";
    } else {
        qDebug() << "is not lock/greeter";
        return false;
    }

    process.start(command);
    process.waitForFinished();
    QString cmdinfo = QString(process.readAllStandardOutput());
    qDebug() << "cmd-result :" << cmdinfo;
    if (!cmdinfo.isEmpty() && cmdinfo.contains("realm-name:"))
        return true;

    return false;
}

bool AuthInterface::isLogined(uint uid)
{
    auto isLogind = std::find_if(m_loginUserList.begin(), m_loginUserList.end(),
                                 [=](uint UID) { return uid == UID; });

    return isLogind != m_loginUserList.end();
}

bool AuthInterface::isDeepin()
{
    // 这是临时的选项，只在Deepin下启用同步认证功能，其他发行版下禁用。
#ifdef QT_DEBUG
    return true;
#else
    return getGSettings("useDeepinAuth").toBool();
#endif
}


QString AuthInterface::getFileContent(QString path)
{
    QFile file(path);
    QString retContent = "";

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray content = file.readAll();
        retContent = content;
        file.close();
    }

    qDebug() << " path : " << path << " , content : " << retContent;
    return retContent;
}

void AuthInterface::checkConfig()
{
    m_model->setAlwaysShowUserSwitchButton(getGSettings("switchuser").toInt() == AuthInterface::Always);
    m_model->setAllowShowUserSwitchButton(getGSettings("switchuser").toInt() == AuthInterface::Ondemand);
}

void AuthInterface::checkPowerInfo()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    //bool config_sleep = gsettingsExist("sleep") ? getGSettings("sleep").toBool() : valueByQSettings<bool>("Power", "sleep", true);
    //bool can_sleep = env.contains(POWER_CAN_SLEEP) ? QVariant(env.value(POWER_CAN_SLEEP)).toBool()
    //                                               : config_sleep && m_login1Inter->CanSuspend().value().contains("yes");
        bool can_sleep = env.contains(POWER_CAN_SLEEP) ? QVariant(env.value(POWER_CAN_SLEEP)).toBool()
                                                   : valueByQSettings<bool>("Power", "sleep", true) &&
                                                     m_login1Inter->CanSuspend().value().contains("yes");
    m_model->setCanSleep(can_sleep);

    QString force_hibernate = getFileContent(cmd_path);
    //judge systemd.force-hibernate=1 , true, yes
    if (force_hibernate.contains(force_hiberbate_1)
            || force_hibernate.contains(force_hiberbate_true)
            || force_hibernate.contains(force_hiberbate_yes)) {
        qDebug() << " ignore check hibernate , direct checkSwap .";
        //kernel cmdline中开启了强制休眠
        m_model->setForceHibernate(true);
        return;
    }

bool can_hibernate = env.contains(POWER_CAN_HIBERNATE) ? QVariant(env.value(POWER_CAN_HIBERNATE)).toBool()
                                                           : valueByQSettings<bool>("Power", "hibernate", true) &&
                                                             m_login1Inter->CanHibernate().value().contains("yes");

    //bool config_hibernate = gsettingsExist("hibernate") ? getGSettings("hibernate").toBool() : valueByQSettings<bool>("Power", "hibernate", true);
    //bool can_hibernate = env.contains(POWER_CAN_HIBERNATE) ? QVariant(env.value(POWER_CAN_HIBERNATE)).toBool()
    //                                                       : config_hibernate && m_login1Inter->CanHibernate().value().contains("yes");
    if (can_hibernate) {
        checkSwap();
    } else {
        m_model->setHasSwap(false);
    }
}

void AuthInterface::checkVirtualKB()
{
    m_model->setHasVirtualKB(QProcess::execute("which", QStringList() << "onboard") == 0);
}

void AuthInterface::checkSwap()
{
    //临时方案，判断swap_file与image_size大小
    //https://pms.uniontech.com/zentao/task-view-43465.html
    //-rw------- 1 root root 8589934592 11\xE6\x9C\x88  2 03:49 /swap_file\n
    bool hasSwap = false;
    QProcess process;
    process.start("ls -l /swap_file");
    process.waitForFinished();
    QString cmdinfo = QString(process.readAllStandardOutput());
    qDebug() << "cmd-result :" << cmdinfo;
    QStringList strList = cmdinfo.split(" ");
    if (strList.size() < 5) {
        m_model->setHasSwap(false);
    }
    qint64 swap_size = strList.at(4).toLongLong();
    qint64 image_size{ get_power_image_size() };
    hasSwap = image_size < swap_size;
    qDebug() << "image_size=" << image_size << ":swap_size=" << swap_size;

    m_model->setHasSwap(hasSwap);
//    QFile file("/proc/swaps");
//    if (file.open(QIODevice::Text | QIODevice::ReadOnly)) {
//        bool           hasSwap{ false };
//        const QString &body = file.readAll();
//        QTextStream    stream(body.toUtf8());
//        while (!stream.atEnd()) {
//            const std::pair<bool, qint64> result =
//                checkIsPartitionType(stream.readLine().simplified().split(
//                    " ", QString::SplitBehavior::SkipEmptyParts));
//            qint64 image_size{ get_power_image_size() };

//            if (result.first) {
//                hasSwap = image_size < result.second;
//            }

//            qDebug() << "check using swap partition!";
//            qDebug() << QString("image_size: %1, swap partition size: %2")
//                            .arg(QString::number(image_size))
//                            .arg(QString::number(result.second));

//            if (hasSwap) {
//                break;
//            }
//        }

//        m_model->setHasSwap(hasSwap);
//        file.close();
//    }
//    else {
//        qWarning() << "open /proc/swaps failed! please check permission!!!";
//    }
}
