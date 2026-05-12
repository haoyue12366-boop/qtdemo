#include "backend.h"

#include "alarmevaluator.h"
#include "logger.h"

#include <QDateTime>
#include <QMetaObject>
#include <utility>

namespace {
QString alarmNameForBit(quint16 bit)
{
    switch (bit) {
    case AlarmEvaluator::TemperatureHigh:
        return QStringLiteral("温度高");
    case AlarmEvaluator::PressureHigh:
        return QStringLiteral("压力高");
    case AlarmEvaluator::FlowHigh:
        return QStringLiteral("流量高");
    case AlarmEvaluator::LevelLow:
        return QStringLiteral("液位低");
    default:
        return QStringLiteral("未知报警");
    }
}

QString alarmValueForBit(quint16 bit, float temperature, float pressure, float level, float flow)
{
    switch (bit) {
    case AlarmEvaluator::TemperatureHigh:
        return QStringLiteral("%1 C").arg(QString::number(temperature, 'f', 1));
    case AlarmEvaluator::PressureHigh:
        return QStringLiteral("%1 MPa").arg(QString::number(pressure, 'f', 2));
    case AlarmEvaluator::FlowHigh:
        return QStringLiteral("%1 m3/h").arg(QString::number(flow, 'f', 1));
    case AlarmEvaluator::LevelLow:
        return QStringLiteral("%1 m").arg(QString::number(level, 'f', 2));
    default:
        return QStringLiteral("-");
    }
}
}

Backend::Backend(const QString &applicationDir, QObject *parent)
    : QObject(parent)
    , m_configManager(applicationDir, this)
    , m_config(m_configManager.load())
    , m_alarmModel(this)
    , m_alarmHistoryModel(this)
    , m_serialWorker(new SerialPortWorker)
    , m_databaseWorker(new DatabaseWorker)
    , m_startedAt(QDateTime::currentDateTime())
{
    qRegisterMetaType<AppConfig>("AppConfig");
    qRegisterMetaType<AlarmRecordData>("AlarmRecordData");
    qRegisterMetaType<CommunicationStats>("CommunicationStats");
    qRegisterMetaType<AlarmHistoryQueryOptions>("AlarmHistoryQueryOptions");
    qRegisterMetaType<AlarmHistoryRow>("AlarmHistoryRow");
    qRegisterMetaType<QList<AlarmHistoryRow>>("QList<AlarmHistoryRow>");
    qRegisterMetaType<AlarmStatsOptions>("AlarmStatsOptions");
    qRegisterMetaType<AlarmStatsResult>("AlarmStatsResult");
    qRegisterMetaType<quint16>("quint16");

    m_statusText = QStringLiteral("系统就绪");
    m_recentError = QStringLiteral("无");
    m_uptimeText = QStringLiteral("00:00:00");
    m_backgroundTaskMessage = QStringLiteral("后台任务就绪");
    m_alarmHistoryMessage = QStringLiteral("尚未查询历史报警");

    connect(&m_uptimeTimer, &QTimer::timeout,
            this, &Backend::updateUptime,
            Qt::QueuedConnection);
    m_uptimeTimer.start(1000);

    initializeBackgroundTasks(applicationDir);
    startWorkers(applicationDir);
}

Backend::~Backend()
{
    shutdown();
}

void Backend::startWorkers(const QString &applicationDir)
{
    m_serialThread.setObjectName(QStringLiteral("SerialThread"));
    m_dbThread.setObjectName(QStringLiteral("DbThread"));

    m_serialWorker->moveToThread(&m_serialThread);
    m_databaseWorker->moveToThread(&m_dbThread);

    connect(&m_serialThread, &QThread::finished,
            m_serialWorker, &QObject::deleteLater);
    connect(&m_dbThread, &QThread::finished,
            m_databaseWorker, &QObject::deleteLater);

    connect(this, &Backend::requestApplyConfig,
            m_serialWorker, &SerialPortWorker::applyConfig,
            Qt::QueuedConnection);
    connect(this, &Backend::requestConnect,
            m_serialWorker, &SerialPortWorker::connectToDevice,
            Qt::QueuedConnection);
    connect(this, &Backend::requestDisconnect,
            m_serialWorker, &SerialPortWorker::disconnectFromDevice,
            Qt::QueuedConnection);
    connect(this, &Backend::requestAcknowledgeAlarm,
            m_serialWorker, &SerialPortWorker::acknowledgeAlarm,
            Qt::QueuedConnection);
    connect(this, &Backend::requestInsertAlarmRecord,
            m_databaseWorker, &DatabaseWorker::insertAlarmRecord,
            Qt::QueuedConnection);

    connect(m_serialWorker, &SerialPortWorker::telemetryUpdated,
            this, &Backend::handleTelemetry,
            Qt::QueuedConnection);
    connect(m_serialWorker, &SerialPortWorker::statusMessage,
            this, &Backend::handleStatusMessage,
            Qt::QueuedConnection);
    connect(m_serialWorker, &SerialPortWorker::connectedChanged, this, [this](bool connected) {
        if (m_connected == connected) {
            return;
        }
        m_connected = connected;
        emit connectedChanged();
    }, Qt::QueuedConnection);
    connect(m_serialWorker, &SerialPortWorker::communicationStatsUpdated,
            this, &Backend::handleCommunicationStats,
            Qt::QueuedConnection);

    connect(m_databaseWorker, &DatabaseWorker::storageError, this, [this](const QString &message) {
        setStatus(message, true);
    }, Qt::QueuedConnection);
    connect(m_databaseWorker, &DatabaseWorker::initialized, this, [this](bool ok) {
        if (!ok) {
            setStatus(QStringLiteral("数据库初始化异常"), true);
        }
    }, Qt::QueuedConnection);

    m_serialThread.start();
    m_dbThread.start();

    QMetaObject::invokeMethod(m_serialWorker, "init", Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_databaseWorker, "init", Qt::QueuedConnection, Q_ARG(QString, applicationDir));

    emit requestApplyConfig(m_config);
}

QString Backend::portName() const { return m_config.portName; }
void Backend::setPortName(const QString &value)
{
    if (m_config.portName == value) {
        return;
    }
    m_config.portName = value;
    emit configChanged();
}

int Backend::baudRate() const { return m_config.baudRate; }
void Backend::setBaudRate(int value)
{
    if (m_config.baudRate == value) {
        return;
    }
    m_config.baudRate = value;
    emit configChanged();
}

int Backend::dataBits() const { return m_config.dataBits; }
void Backend::setDataBits(int value)
{
    value = qBound(5, value, 8);
    if (m_config.dataBits == value) {
        return;
    }
    m_config.dataBits = value;
    emit configChanged();
}

int Backend::stopBits() const { return m_config.stopBits; }
void Backend::setStopBits(int value)
{
    value = value == 2 ? 2 : 1;
    if (m_config.stopBits == value) {
        return;
    }
    m_config.stopBits = value;
    emit configChanged();
}

QString Backend::parity() const { return m_config.parity; }
void Backend::setParity(const QString &value)
{
    if (m_config.parity == value) {
        return;
    }
    m_config.parity = value;
    emit configChanged();
}

int Backend::slaveId() const { return m_config.slaveId; }
void Backend::setSlaveId(int value)
{
    value = qBound(1, value, 247);
    if (m_config.slaveId == value) {
        return;
    }
    m_config.slaveId = value;
    emit configChanged();
}

int Backend::refreshInterval() const { return m_config.refreshInterval; }
void Backend::setRefreshInterval(int value)
{
    value = qMax(20, value);
    if (m_config.refreshInterval == value) {
        return;
    }
    m_config.refreshInterval = value;
    emit configChanged();
}

bool Backend::simulationMode() const { return m_config.simulationMode; }
void Backend::setSimulationMode(bool value)
{
    if (m_config.simulationMode == value) {
        return;
    }
    m_config.simulationMode = value;
    emit configChanged();
}

bool Backend::connected() const { return m_connected; }
QString Backend::communicationModeText() const
{
    return m_communicationStats.simulationMode ? QStringLiteral("仿真模式") : QStringLiteral("真实串口");
}
qulonglong Backend::txCount() const { return m_communicationStats.txCount; }
qulonglong Backend::rxCount() const { return m_communicationStats.rxCount; }
qulonglong Backend::timeoutCount() const { return m_communicationStats.timeoutCount; }
qulonglong Backend::retryCount() const { return m_communicationStats.retryCount; }
qulonglong Backend::crcErrorCount() const { return m_communicationStats.crcErrorCount; }
qulonglong Backend::exceptionCount() const { return m_communicationStats.exceptionCount; }
QString Backend::statusText() const { return m_statusText; }
bool Backend::statusError() const { return m_statusError; }
QString Backend::recentError() const { return m_recentError; }
QString Backend::uptimeText() const { return m_uptimeText; }
float Backend::temperature() const { return m_temperature; }
float Backend::pressure() const { return m_pressure; }
float Backend::level() const { return m_level; }
float Backend::flow() const { return m_flow; }
QList<qreal> Backend::temperatureData() const { return m_temperatureData; }
qreal Backend::temperatureMin() const { return m_temperatureMin; }
qreal Backend::temperatureMax() const { return m_temperatureMax; }
qreal Backend::temperatureAvg() const { return m_temperatureAvg; }
QObject *Backend::alarmModel() { return &m_alarmModel; }

void Backend::saveConfig()
{
    m_config = normalizedConfig();
    if (!m_configManager.save(m_config)) {
        setStatus(QStringLiteral("配置保存失败"), true);
        return;
    }
    emit requestApplyConfig(m_config);
    setStatus(QStringLiteral("配置已保存并生效"), false);
    emit configChanged();
}

void Backend::connectToDevice()
{
    emit requestApplyConfig(normalizedConfig());
    emit requestConnect();
}

void Backend::disconnectFromDevice()
{
    emit requestDisconnect();
}

void Backend::acknowledgeAlarm()
{
    if (m_currentAlarmStatus == 0) {
        setStatus(QStringLiteral("当前无未确认报警"), false);
        return;
    }
    m_alarmModel.acknowledgeByAlarmStatus(m_currentAlarmStatus);
    emit requestAcknowledgeAlarm();
    setStatus(QStringLiteral("已发送报警确认命令"), false);
}

void Backend::shutdown()
{
    if (m_stopping) {
        return;
    }
    m_stopping = true;
    stopWorkers();
}

void Backend::handleTelemetry(float temperature, float pressure, float level, float flow, quint16 alarmStatus)
{
    m_temperature = temperature;
    m_pressure = pressure;
    m_level = level;
    m_flow = flow;
    m_currentAlarmStatus = alarmStatus;

    pushTemperature(temperature);
    m_alarmModel.updateFromAlarmStatus(alarmStatus, temperature, pressure, level, flow);
    pushAlarmRecordsForStorage(alarmStatus, temperature, pressure, level, flow);

    emit telemetryChanged();
    setStatus(QStringLiteral("采集正常: T=%1 C, P=%2 MPa, L=%3 m, F=%4 m3/h")
                  .arg(QString::number(temperature, 'f', 1),
                       QString::number(pressure, 'f', 2),
                       QString::number(level, 'f', 2),
                       QString::number(flow, 'f', 1)),
              false);
}

void Backend::handleStatusMessage(const QString &message, bool error)
{
    setStatus(message, error);
}

void Backend::handleCommunicationStats(const CommunicationStats &stats)
{
    m_communicationStats = stats;
    emit communicationStatsChanged();
}

void Backend::updateUptime()
{
    const qint64 seconds = m_startedAt.secsTo(QDateTime::currentDateTime());
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    const qint64 secs = seconds % 60;
    const QString next = QStringLiteral("%1:%2:%3")
                             .arg(hours, 2, 10, QLatin1Char('0'))
                             .arg(minutes, 2, 10, QLatin1Char('0'))
                             .arg(secs, 2, 10, QLatin1Char('0'));
    if (m_uptimeText == next) {
        return;
    }
    m_uptimeText = next;
    emit uptimeChanged();
}

void Backend::stopWorkers()
{
    if (m_serialThread.isRunning() && m_serialWorker != nullptr) {
        const bool invoked = QMetaObject::invokeMethod(m_serialWorker, "shutdown", Qt::BlockingQueuedConnection);
        if (!invoked) {
            Logger::error(QStringLiteral("通信线程 shutdown 调用失败"));
        }
        m_serialThread.quit();
        if (!m_serialThread.wait(3000)) {
            Logger::error(QStringLiteral("通信线程退出超时，请检查串口资源是否阻塞"));
            setStatus(QStringLiteral("通信线程退出超时"), true);
        }
        m_serialWorker = nullptr;
    }

    if (m_dbThread.isRunning() && m_databaseWorker != nullptr) {
        const bool invoked = QMetaObject::invokeMethod(m_databaseWorker, "shutdown", Qt::BlockingQueuedConnection);
        if (!invoked) {
            Logger::error(QStringLiteral("数据库线程 shutdown 调用失败"));
        }
        m_dbThread.quit();
        if (!m_dbThread.wait(3000)) {
            Logger::error(QStringLiteral("数据库线程退出超时，请检查 SQLite 写入状态"));
            setStatus(QStringLiteral("数据库线程退出超时"), true);
        }
        m_databaseWorker = nullptr;
    }
}

void Backend::setStatus(const QString &message, bool error)
{
    if (m_statusText == message && m_statusError == error) {
        return;
    }
    m_statusText = message;
    m_statusError = error;
    if (error) {
        m_recentError = message;
        Logger::error(QStringLiteral("用户可见异常: %1").arg(message));
    }
    emit statusChanged();
}

void Backend::pushTemperature(float value)
{
    m_temperatureData.append(value);
    while (m_temperatureData.size() > 1000) {
        m_temperatureData.removeFirst();
    }
    if (!m_temperatureData.isEmpty()) {
        qreal sum = 0.0;
        m_temperatureMin = m_temperatureData.first();
        m_temperatureMax = m_temperatureData.first();
        for (qreal point : std::as_const(m_temperatureData)) {
            sum += point;
            m_temperatureMin = qMin(m_temperatureMin, point);
            m_temperatureMax = qMax(m_temperatureMax, point);
        }
        m_temperatureAvg = sum / m_temperatureData.size();
    }
    emit temperatureDataChanged();
}

void Backend::pushAlarmRecordsForStorage(quint16 alarmStatus, float temperature, float pressure, float level, float flow)
{
    const quint16 newBits = alarmStatus & static_cast<quint16>(~m_lastAlarmStatusForStorage);
    if (newBits == 0) {
        m_lastAlarmStatusForStorage = alarmStatus;
        return;
    }

    const QString timestampText = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    for (quint16 bit : {quint16(AlarmEvaluator::TemperatureHigh),
                        quint16(AlarmEvaluator::PressureHigh),
                        quint16(AlarmEvaluator::FlowHigh),
                        quint16(AlarmEvaluator::LevelLow)}) {
        if ((newBits & bit) == 0) {
            continue;
        }

        AlarmRecordData record;
        record.timestampText = timestampText;
        record.variableName = alarmNameForBit(bit);
        record.valueText = alarmValueForBit(bit, temperature, pressure, level, flow);
        record.statusText = QStringLiteral("激活");
        record.acknowledged = false;
        record.alarmBit = bit;
        record.alarmStatus = alarmStatus;
        record.temperature = temperature;
        record.pressure = pressure;
        record.level = level;
        record.flow = flow;
        emit requestInsertAlarmRecord(record);
    }

    m_lastAlarmStatusForStorage = alarmStatus;
}

AppConfig Backend::normalizedConfig() const
{
    AppConfig config = m_config;
    config.slaveId = qBound(1, config.slaveId, 247);
    config.refreshInterval = qMax(20, config.refreshInterval);
    config.dataBits = qBound(5, config.dataBits, 8);
    config.stopBits = config.stopBits == 2 ? 2 : 1;
    if (config.parity != QStringLiteral("None")
        && config.parity != QStringLiteral("Even")
        && config.parity != QStringLiteral("Odd")) {
        config.parity = QStringLiteral("None");
    }
    return config;
}
