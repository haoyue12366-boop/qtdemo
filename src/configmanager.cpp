#include "configmanager.h"

#include "logger.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

ConfigManager::ConfigManager(const QString &applicationDir, QObject *parent)
    : QObject(parent)
    , m_filePath(QDir(applicationDir).filePath(QStringLiteral("config.ini")))
{
}

AppConfig ConfigManager::load()
{
    QSettings settings(m_filePath, QSettings::IniFormat);

    if (!QFileInfo::exists(m_filePath)) {
        AppConfig defaults;
        save(defaults);
        Logger::info(QStringLiteral("首次运行，已生成默认配置: %1").arg(m_filePath));
        return defaults;
    }

    AppConfig config;
    settings.beginGroup(QStringLiteral("SerialPort"));
    config.portName = settings.value(QStringLiteral("PortName"), config.portName).toString();
    config.baudRate = settings.value(QStringLiteral("BaudRate"), config.baudRate).toInt();
    config.dataBits = settings.value(QStringLiteral("DataBits"), config.dataBits).toInt();
    config.stopBits = settings.value(QStringLiteral("StopBits"), config.stopBits).toInt();
    config.parity = settings.value(QStringLiteral("Parity"), config.parity).toString();
    settings.endGroup();

    settings.beginGroup(QStringLiteral("Device"));
    config.slaveId = qBound(1, settings.value(QStringLiteral("SlaveID"), config.slaveId).toInt(), 247);
    config.refreshInterval = qMax(20, settings.value(QStringLiteral("RefreshInterval"), config.refreshInterval).toInt());
    config.simulationMode = settings.value(QStringLiteral("SimulationMode"), config.simulationMode).toBool();
    settings.endGroup();

    Logger::info(QStringLiteral("配置加载完成: %1").arg(m_filePath));
    return config;
}

bool ConfigManager::save(const AppConfig &config)
{
    QSettings settings(m_filePath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("SerialPort"));
    settings.setValue(QStringLiteral("PortName"), config.portName);
    settings.setValue(QStringLiteral("BaudRate"), config.baudRate);
    settings.setValue(QStringLiteral("DataBits"), config.dataBits);
    settings.setValue(QStringLiteral("StopBits"), config.stopBits);
    settings.setValue(QStringLiteral("Parity"), config.parity);
    settings.endGroup();

    settings.beginGroup(QStringLiteral("Device"));
    settings.setValue(QStringLiteral("SlaveID"), qBound(1, config.slaveId, 247));
    settings.setValue(QStringLiteral("RefreshInterval"), qMax(20, config.refreshInterval));
    settings.setValue(QStringLiteral("SimulationMode"), config.simulationMode);
    settings.endGroup();
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        Logger::error(QStringLiteral("配置写入失败: %1").arg(m_filePath));
        return false;
    }
    Logger::info(QStringLiteral("配置保存完成: %1").arg(m_filePath));
    return true;
}

QString ConfigManager::configFilePath() const
{
    return m_filePath;
}
