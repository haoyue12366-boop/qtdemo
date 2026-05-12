#pragma once

#include "alarmhistorymodel.h"
#include "alarmlogmodel.h"
#include "alarmrecorddata.h"
#include "backgroundtasktypes.h"
#include "communicationstats.h"
#include "configmanager.h"
#include "databaseworker.h"
#include "serialportworker.h"

#include <QObject>
#include <QDateTime>
#include <QThread>
#include <QTimer>

class BackgroundTaskManager;

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
    Q_PROPERTY(QObject *alarmHistoryModel READ alarmHistoryModel CONSTANT)
    Q_PROPERTY(bool backgroundTaskRunning READ backgroundTaskRunning NOTIFY backgroundTaskChanged)
    Q_PROPERTY(int backgroundTaskProgress READ backgroundTaskProgress NOTIFY backgroundTaskChanged)
    Q_PROPERTY(QString backgroundTaskMessage READ backgroundTaskMessage NOTIFY backgroundTaskChanged)
    Q_PROPERTY(bool alarmHistoryLoading READ alarmHistoryLoading NOTIFY alarmHistoryChanged)
    Q_PROPERTY(QString alarmHistoryMessage READ alarmHistoryMessage NOTIFY alarmHistoryChanged)
    Q_PROPERTY(bool alarmStatsLoading READ alarmStatsLoading NOTIFY alarmStatsChanged)
    Q_PROPERTY(int alarmTotalCount READ alarmTotalCount NOTIFY alarmStatsChanged)
    Q_PROPERTY(int alarmActiveCount READ alarmActiveCount NOTIFY alarmStatsChanged)
    Q_PROPERTY(int alarmAcknowledgedCount READ alarmAcknowledgedCount NOTIFY alarmStatsChanged)
    Q_PROPERTY(int temperatureAlarmCount READ temperatureAlarmCount NOTIFY alarmStatsChanged)
    Q_PROPERTY(int pressureAlarmCount READ pressureAlarmCount NOTIFY alarmStatsChanged)
    Q_PROPERTY(int levelAlarmCount READ levelAlarmCount NOTIFY alarmStatsChanged)
    Q_PROPERTY(int flowAlarmCount READ flowAlarmCount NOTIFY alarmStatsChanged)
    Q_PROPERTY(QString latestAlarmTime READ latestAlarmTime NOTIFY alarmStatsChanged)

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
    QObject *alarmHistoryModel();
    bool backgroundTaskRunning() const;
    int backgroundTaskProgress() const;
    QString backgroundTaskMessage() const;
    bool alarmHistoryLoading() const;
    QString alarmHistoryMessage() const;
    bool alarmStatsLoading() const;
    int alarmTotalCount() const;
    int alarmActiveCount() const;
    int alarmAcknowledgedCount() const;
    int temperatureAlarmCount() const;
    int pressureAlarmCount() const;
    int levelAlarmCount() const;
    int flowAlarmCount() const;
    QString latestAlarmTime() const;

    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE void connectToDevice();
    Q_INVOKABLE void disconnectFromDevice();
    Q_INVOKABLE void acknowledgeAlarm();
    Q_INVOKABLE void shutdown();
    Q_INVOKABLE void exportAlarmCsv(const QString &filePath);
    Q_INVOKABLE void queryAlarmHistory(const QString &startTime,
                                       const QString &endTime,
                                       const QString &keyword,
                                       const QString &status,
                                       int limit,
                                       int offset);
    Q_INVOKABLE void refreshAlarmStats(const QString &startTime, const QString &endTime);

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
    void requestInsertAlarmRecord(const AlarmRecordData &record);
    void backgroundTaskChanged();
    void alarmHistoryChanged();
    void alarmStatsChanged();

private slots:
    void handleTelemetry(float temperature, float pressure, float level, float flow, quint16 alarmStatus);
    void handleStatusMessage(const QString &message, bool error);
    void handleCommunicationStats(const CommunicationStats &stats);
    void updateUptime();
    void handleBackgroundTaskStarted(const QString &taskName);
    void handleBackgroundTaskProgress(const QString &taskName, int progress, const QString &message);
    void handleBackgroundTaskFinished(const QString &taskName, const QString &message);
    void handleBackgroundTaskFailed(const QString &taskName, const QString &errorMessage);
    void handleAlarmHistoryRows(const QList<AlarmHistoryRow> &rows);
    void handleAlarmStatsResult(const AlarmStatsResult &result);

private:
    void initializeBackgroundTasks(const QString &applicationDir);
    void startWorkers(const QString &applicationDir);
    void stopWorkers();
    void setStatus(const QString &message, bool error);
    void pushTemperature(float value);
    void pushAlarmRecordsForStorage(quint16 alarmStatus, float temperature, float pressure, float level, float flow);
    AppConfig normalizedConfig() const;

    ConfigManager m_configManager;
    AppConfig m_config;
    AlarmLogModel m_alarmModel;
    AlarmHistoryModel m_alarmHistoryModel;
    QThread m_serialThread;
    QThread m_dbThread;
    SerialPortWorker *m_serialWorker = nullptr;
    DatabaseWorker *m_databaseWorker = nullptr;
    BackgroundTaskManager *m_backgroundTaskManager = nullptr;
    QString m_databasePath;
    QTimer m_uptimeTimer;
    QDateTime m_startedAt;
    bool m_connected = false;
    bool m_stopping = false;
    QString m_statusText;
    QString m_recentError;
    QString m_uptimeText;
    bool m_statusError = false;
    CommunicationStats m_communicationStats;
    quint16 m_lastAlarmStatusForStorage = 0;
    quint16 m_currentAlarmStatus = 0;
    float m_temperature = 0.0f;
    float m_pressure = 0.0f;
    float m_level = 0.0f;
    float m_flow = 0.0f;
    QList<qreal> m_temperatureData;
    qreal m_temperatureMin = 0.0;
    qreal m_temperatureMax = 0.0;
    qreal m_temperatureAvg = 0.0;
    int m_backgroundTaskCount = 0;
    int m_backgroundTaskProgress = 0;
    QString m_backgroundTaskMessage;
    bool m_alarmHistoryLoading = false;
    QString m_alarmHistoryMessage;
    bool m_alarmStatsLoading = false;
    AlarmStatsResult m_alarmStatsResult;
};
