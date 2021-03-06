#include "networkmanager.h"
#include "networkdevice.h"

NetworkManager *NetworkManager::INSTANCE = nullptr;

NetworkManager *NetworkManager::instance(QObject *parent)
{
    if (!INSTANCE)
        INSTANCE = new NetworkManager(parent);

    return INSTANCE;
}

void NetworkManager::init()
{
    QTimer *dbusCheckTimer = new QTimer;
    dbusCheckTimer->setInterval(100);
    dbusCheckTimer->setSingleShot(false);

    auto checkFunc = [=] {
        if (!m_networkInter->isValid())
            return;

        QTimer::singleShot(100, this, &NetworkManager::reloadDevices);
        QTimer::singleShot(150, this, &NetworkManager::reloadActiveConnections);
        dbusCheckTimer->deleteLater();
    };

    connect(dbusCheckTimer, &QTimer::timeout, checkFunc);
    dbusCheckTimer->start();
}

NetworkManager::GlobalNetworkState NetworkManager::globalNetworkState() const
{
    return GlobalNetworkState(m_networkInter->state());
}

const NetworkDevice::NetworkTypes NetworkManager::states() const
{
    return m_states;
}

const NetworkDevice::NetworkTypes NetworkManager::types() const
{
    return m_types;
}

const QSet<NetworkDevice> NetworkManager::deviceList() const
{
    return m_deviceSet;
}

const QSet<QUuid> NetworkManager::activeConnSet() const
{
    return m_activeConnSet;
}

NetworkDevice::NetworkState NetworkManager::deviceState(const QUuid &uuid) const
{
    const auto item = device(uuid);
    if (item == m_deviceSet.cend())
        return NetworkDevice::Unknow;

    return item->state();
}

bool NetworkManager::deviceEnabled(const QUuid &uuid) const
{
    return m_networkInter->IsDeviceEnabled(QDBusObjectPath(devicePath(uuid)));
}

void NetworkManager::setDeviceEnabled(const QUuid &uuid, const bool enable)
{
    m_networkInter->EnableDevice(QDBusObjectPath(devicePath(uuid)), enable);
}

const QString NetworkManager::deviceHwAddr(const QUuid &uuid) const
{
    const auto item = device(uuid);
    if (item == m_deviceSet.cend())
        return QString();

    return item->hwAddress();
}

const QString NetworkManager::devicePath(const QUuid &uuid) const
{
    const auto item = device(uuid);
    if (item == m_deviceSet.cend())
        return QString();

    return item->path();
}

const QJsonObject NetworkManager::deviceConnInfo(const QUuid &uuid) const
{
    const QString addr = deviceHwAddr(uuid);
    if (addr.isEmpty())
        return QJsonObject();

    const QJsonDocument infos = QJsonDocument::fromJson(m_networkInter->GetActiveConnectionInfo().value().toUtf8());
    Q_ASSERT(infos.isArray());

    for (auto info : infos.array())
    {
        Q_ASSERT(info.isObject());
        const QJsonObject obj = info.toObject();
        if (obj.contains("HwAddress") && obj.value("HwAddress").toString() == addr)
            return obj;
    }

    return QJsonObject();
}

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent),

      m_states(NetworkDevice::None),
      m_types(NetworkDevice::None),

      m_networkInter(new DBusNetwork(this))
{
    connect(m_networkInter, &DBusNetwork::StateChanged, this, &NetworkManager::globalNetworkStateChanged);
    connect(m_networkInter, &DBusNetwork::DevicesChanged, this, &NetworkManager::reloadDevices);
    connect(m_networkInter, &DBusNetwork::ActiveConnectionsChanged, this, &NetworkManager::reloadActiveConnections);
}

const QSet<NetworkDevice>::const_iterator NetworkManager::device(const QUuid &uuid) const
{
    return std::find_if(m_deviceSet.cbegin(), m_deviceSet.cend(),
                        [&] (const NetworkDevice &dev) {return dev == uuid;});
}

void NetworkManager::reloadDevices()
{
    const QJsonDocument doc = QJsonDocument::fromJson(m_networkInter->devices().toUtf8());
    Q_ASSERT(doc.isObject());
    const QJsonObject obj = doc.object();

    NetworkDevice::NetworkTypes types = NetworkDevice::None;
    QSet<NetworkDevice> deviceSet;
    for (auto infoList(obj.constBegin()); infoList != obj.constEnd(); ++infoList)
    {
        Q_ASSERT(infoList.value().isArray());
        const NetworkDevice::NetworkType deviceType = NetworkDevice::deviceType(infoList.key());

        const auto list = infoList.value().toArray();
        if (list.isEmpty())
            continue;

        types |= deviceType;

        for (auto device : list)
            deviceSet.insert(NetworkDevice(deviceType, device.toObject()));
    }

    const QSet<NetworkDevice> removedDeviceList = m_deviceSet - deviceSet;
    for (auto dev : removedDeviceList)
        emit deviceRemoved(dev);
    for (auto dev : deviceSet)
    {
        if (m_deviceSet.contains(dev))
            emit deviceChanged(dev);
        else
            emit deviceAdded(dev);
    }

    m_deviceSet = std::move(deviceSet);
    if (m_types == types)
        return;

    m_types = types;
    emit deviceTypesChanged(m_types);

//    qDebug() << "device type: " << m_types;
}

void NetworkManager::reloadActiveConnections()
{
    const QJsonDocument doc = QJsonDocument::fromJson(m_networkInter->activeConnections().toUtf8());
    Q_ASSERT(doc.isObject());
    const QJsonObject obj = doc.object();

    NetworkDevice::NetworkTypes states = NetworkDevice::None;
    QSet<QUuid> activeConnList;
    for (auto info(obj.constBegin()); info != obj.constEnd(); ++info)
    {
        Q_ASSERT(info.value().isObject());
        const QJsonObject infoObj = info.value().toObject();

        const QUuid uuid = infoObj.value("Uuid").toString();
        // if uuid not in device list, its a wireless connection
        const bool isWireless = std::find(m_deviceSet.cbegin(), m_deviceSet.cend(), uuid) == m_deviceSet.cend();

        if (isWireless)
            states |= NetworkDevice::Wireless;
        else
            states |= NetworkDevice::Wired;

        activeConnList.insert(uuid);
    }

    const QSet<QUuid> removedConnList = m_activeConnSet - activeConnList;
    m_activeConnSet = std::move(activeConnList);

    for (auto uuid : removedConnList)
        emit activeConnectionChanged(uuid);

    for (auto uuid : m_activeConnSet)
        emit activeConnectionChanged(uuid);

    if (m_states == states)
        return;

    m_states = states;
    emit networkStateChanged(m_states);

//    qDebug() << "network states: " << m_states;
}
