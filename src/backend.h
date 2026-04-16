#pragma once

#include "alarmlogmodel.h"
#include "communicationstats.h"
#include "configmanager.h"
#include "databaseworker.h"
#include "serialportworker.h"
#include "telemetryrecord.h"

#include <QObject>
#include <QDateTime>
#include <QThread>
#include <QTimer>

/**
 * @brief C++ 业务汇聚层。
 *
 * Backend 暴露 Q_PROPERTY 给 QML，桥接 SerialPortWorker、AlarmLogModel、
 * ConfigManager、DatabaseWorker。QML 不直接访问串口、数据库或协议对象。
 */
class Backend final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString portName READ portName WRITE setPortName NOTIFY configChanged)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY configChanged)
    Q_PROPERTY(int dataBits READ dataBits WRITE setDataBits NOTIFY configChanged)
    Q_PROPERTY(int stopBits READ stopBits WRITE setStopBits NOTIFY configChanged)
    Q_PROPERTY(QString parity READ parity WRITE setParity NOTIFY configChanged)
    Q_PROPERTY(int slaveId READ slaveId WRITE setSlaveId NOTIFY configChanged)
    Q_PROPERTY(int refreshInterval READ refreshInterval WRITE setRefreshInterval NOTIFY configChanged)
    Q_PROPERTY(bool simulationMode READ simulationMode WRITE setSimulationMode NOTIFY configChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString communicationModeText READ communicationModeText NOTIFY communicationStatsChanged)
    Q_PROPERTY(qulonglong txCount READ txCount NOTIFY communicationStatsChanged)
    Q_PROPERTY(qulonglong rxCount READ rxCount NOTIFY communicationStatsChanged)
    Q_PROPERTY(qulonglong timeoutCount READ timeoutCount NOTIFY communicationStatsChanged)
    Q_PROPERTY(qulonglong retryCount READ retryCount NOTIFY communicationStatsChanged)
    Q_PROPERTY(qulonglong crcErrorCount READ crcErrorCount NOTIFY communicationStatsChanged)
    Q_PROPERTY(qulonglong exceptionCount READ exceptionCount NOTIFY communicationStatsChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool statusError READ statusError NOTIFY statusChanged)
    Q_PROPERTY(QString recentError READ recentError NOTIFY statusChanged)
    Q_PROPERTY(QString uptimeText READ uptimeText NOTIFY uptimeChanged)
    Q_PROPERTY(float temperature READ temperature NOTIFY telemetryChanged)
    Q_PROPERTY(float pressure READ pressure NOTIFY telemetryChanged)
    Q_PROPERTY(float level READ level NOTIFY telemetryChanged)
    Q_PROPERTY(float flow READ flow NOTIFY telemetryChanged)
    Q_PROPERTY(QList<qreal> temperatureData READ temperatureData NOTIFY temperatureDataChanged)
    Q_PROPERTY(qreal temperatureMin READ temperatureMin NOTIFY temperatureDataChanged)
    Q_PROPERTY(qreal temperatureMax READ temperatureMax NOTIFY temperatureDataChanged)
    Q_PROPERTY(qreal temperatureAvg READ temperatureAvg NOTIFY temperatureDataChanged)
    Q_PROPERTY(QObject *alarmModel READ alarmModel CONSTANT)

public:
    explicit Backend(const QString &applicationDir, QObject *parent = nullptr);
    ~Backend() override;

    QString portName() const;
    void setPortName(const QString &value);
    int baudRate() const;
    void setBaudRate(int value);
    int dataBits() const;
    void setDataBits(int value);
    int stopBits() const;
    void setStopBits(int value);
    QString parity() const;
    void setParity(const QString &value);
    int slaveId() const;
    void setSlaveId(int value);
    int refreshInterval() const;
    void setRefreshInterval(int value);
    bool simulationMode() const;
    void setSimulationMode(bool value);

    bool connected() const;
    QString communicationModeText() const;
    qulonglong txCount() const;
    qulonglong rxCount() const;
    qulonglong timeoutCount() const;
    qulonglong retryCount() const;
    qulonglong crcErrorCount() const;
    qulonglong exceptionCount() const;
    QString statusText() const;
    bool statusError() const;
    QString recentError() const;
    QString uptimeText() const;
    float temperature() const;
    float pressure() const;
    float level() const;
    float flow() const;
    QList<qreal> temperatureData() const;
    qreal temperatureMin() const;
    qreal temperatureMax() const;
    qreal temperatureAvg() const;
    QObject *alarmModel();

    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE void connectToDevice();
    Q_INVOKABLE void disconnectFromDevice();
    Q_INVOKABLE void acknowledgeAlarm();
    Q_INVOKABLE void shutdown();

signals:
    void configChanged();
    void statusChanged();
    void telemetryChanged();
    void temperatureDataChanged();
    void connectedChanged();
    void communicationStatsChanged();
    void uptimeChanged();
    void requestApplyConfig(const AppConfig &config);
    void requestConnect();
    void requestDisconnect();
    void requestAcknowledgeAlarm();
    void requestInsertTelemetry(const TelemetryRecord &record);

private slots:
    void handleTelemetry(float temperature, float pressure, float level, float flow, quint16 alarmStatus);
    void handleStatusMessage(const QString &message, bool error);
    void handleCommunicationStats(const CommunicationStats &stats);
    void updateUptime();

private:
    void startWorkers(const QString &applicationDir);
    void stopWorkers();
    void setStatus(const QString &message, bool error);
    void pushTemperature(float value);
    AppConfig normalizedConfig() const;

    ConfigManager m_configManager;
    AppConfig m_config;
    AlarmLogModel m_alarmModel;
    QThread m_serialThread;
    QThread m_dbThread;
    SerialPortWorker *m_serialWorker = nullptr;
    DatabaseWorker *m_databaseWorker = nullptr;
    QTimer m_uptimeTimer;
    QDateTime m_startedAt;
    bool m_connected = false;
    QString m_statusText;
    QString m_recentError;
    QString m_uptimeText;
    bool m_statusError = false;
    CommunicationStats m_communicationStats;
    float m_temperature = 0.0f;
    float m_pressure = 0.0f;
    float m_level = 0.0f;
    float m_flow = 0.0f;
    QList<qreal> m_temperatureData;
    qreal m_temperatureMin = 0.0;
    qreal m_temperatureMax = 0.0;
    qreal m_temperatureAvg = 0.0;
};
