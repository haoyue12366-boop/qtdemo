#pragma once

#include <QObject>
#include <QMetaType>
#include <QString>

/**
 * @brief 串口与设备配置结构体。
 *
 * 数据来源为 config.ini。首次运行时会自动生成文档要求的默认配置。
 */
struct AppConfig
{
    QString portName = QStringLiteral("COM3");
    int baudRate = 115200;
    int dataBits = 8;
    int stopBits = 1;
    QString parity = QStringLiteral("None");
    int slaveId = 1;
    int refreshInterval = 100;
    bool simulationMode = true;
};

Q_DECLARE_METATYPE(AppConfig)

/**
 * @brief QSettings 配置管理器。
 *
 * 负责读取、保存可执行文件同目录下的 config.ini，并对用户输入做基础范围保护。
 */
class ConfigManager final : public QObject
{
    Q_OBJECT
public:
    explicit ConfigManager(const QString &applicationDir, QObject *parent = nullptr);

    AppConfig load();
    bool save(const AppConfig &config);
    QString configFilePath() const;

private:
    QString m_filePath;
};
