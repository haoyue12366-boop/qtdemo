#include "databaseworker.h"

#include "logger.h"

#include <QDateTime>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>

DatabaseWorker::DatabaseWorker(QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("qthmi_db_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

void DatabaseWorker::init(const QString &applicationDir)
{
    Logger::info(QStringLiteral("数据库子线程 ID: 0x%1")
                     .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16));

    m_databasePath = QDir(applicationDir).filePath(QStringLiteral("data.db"));
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);

    if (!m_database.open()) {
        const QString message = QStringLiteral("SQLite 打开失败: %1").arg(m_database.lastError().text());
        Logger::error(message);
        emit storageError(QStringLiteral("数据库打开异常"));
        emit initialized(false);
        return;
    }

    if (m_flushTimer == nullptr) {
        m_flushTimer = new QTimer(this);
        m_flushTimer->setInterval(kFlushIntervalMs);
        connect(m_flushTimer, &QTimer::timeout,
                this, &DatabaseWorker::flushPendingRecords,
                Qt::QueuedConnection);
        m_flushTimer->start();
    }

    m_initialized = ensureTable();
    emit initialized(m_initialized);
}

void DatabaseWorker::insertAlarmRecord(const AlarmRecordData &record)
{
    if (!m_initialized || !m_database.isOpen()) {
        Logger::error(QStringLiteral("SQLite 写入失败: 数据库未初始化"));
        emit storageError(QStringLiteral("数据库写入异常"));
        return;
    }

    handleQueueOverflow();
    m_pendingAlarmQueue.append(record);

    if (m_pendingAlarmQueue.size() >= kBatchSize) {
        flushPendingRecords();
    }
}

void DatabaseWorker::shutdown()
{
    Logger::info(QStringLiteral("数据库线程准备关闭"));
    if (m_flushTimer != nullptr) {
        m_flushTimer->stop();
    }

    while (!m_pendingAlarmQueue.isEmpty()) {
        if (!flushBatch()) {
            Logger::error(QStringLiteral("数据库关闭前 flush 失败，剩余 %1 条报警日志未写入")
                              .arg(m_pendingAlarmQueue.size()));
            break;
        }
    }
    closeDatabase();
}

bool DatabaseWorker::ensureTable()
{
    QSqlQuery query(m_database);
    const bool ok = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS alarm_log ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp TEXT NOT NULL,"
        "variable_name TEXT NOT NULL,"
        "alarm_value TEXT NOT NULL,"
        "status_text TEXT NOT NULL,"
        "acknowledged INTEGER NOT NULL,"
        "alarm_bit INTEGER NOT NULL,"
        "alarm_status INTEGER NOT NULL,"
        "temperature REAL NOT NULL,"
        "pressure REAL NOT NULL,"
        "level REAL NOT NULL,"
        "flow REAL NOT NULL)"));

    if (!ok) {
        const QString message = QStringLiteral("SQLite 建表失败: %1").arg(query.lastError().text());
        Logger::error(message);
        emit storageError(QStringLiteral("数据库写入异常"));
        return false;
    }
    Logger::info(QStringLiteral("SQLite 数据库初始化完成: %1").arg(m_databasePath));
    return true;
}

bool DatabaseWorker::flushBatch()//从待写队列里取一批报警记录，在数据库线程内用一个事务批量写入 SQLite；全部成功才从队列删除，任何一步失败都回滚并保留队列数据
{
    if (!m_initialized || !m_database.isOpen() || m_pendingAlarmQueue.isEmpty()) {
        return true;
    }

    const int batchCount = qMin(kBatchSize, m_pendingAlarmQueue.size());
    const QVector<AlarmRecordData> batch = m_pendingAlarmQueue.mid(0, batchCount);

    if (!m_database.transaction()) {
        Logger::error(QStringLiteral("SQLite 开启事务失败: %1").arg(m_database.lastError().text()));
        emit storageError(QStringLiteral("数据库写入异常"));
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO alarm_log("
        "timestamp, variable_name, alarm_value, status_text, acknowledged, alarm_bit, alarm_status, "
        "temperature, pressure, level, flow) "
        "VALUES("
        ":timestamp, :variable_name, :alarm_value, :status_text, :acknowledged, :alarm_bit, :alarm_status, "
        ":temperature, :pressure, :level, :flow)"));

    for (const AlarmRecordData &record : batch) {
        query.bindValue(QStringLiteral(":timestamp"), record.timestampText);
        query.bindValue(QStringLiteral(":variable_name"), record.variableName);
        query.bindValue(QStringLiteral(":alarm_value"), record.valueText);
        query.bindValue(QStringLiteral(":status_text"), record.statusText);
        query.bindValue(QStringLiteral(":acknowledged"), record.acknowledged ? 1 : 0);
        query.bindValue(QStringLiteral(":alarm_bit"), record.alarmBit);
        query.bindValue(QStringLiteral(":alarm_status"), record.alarmStatus);
        query.bindValue(QStringLiteral(":temperature"), record.temperature);
        query.bindValue(QStringLiteral(":pressure"), record.pressure);
        query.bindValue(QStringLiteral(":level"), record.level);
        query.bindValue(QStringLiteral(":flow"), record.flow);

        if (!query.exec()) {//只要 batch 里有任意一条 exec() 失败，就立刻：取错误对象 query.lastError()，rollback() 回滚整个事务，记日志，发 storageError，返回 false
            const QSqlError error = query.lastError();
            m_database.rollback();
            Logger::error(QStringLiteral("SQLite 报警日志写入失败: code=%1, text=%2")
                              .arg(error.nativeErrorCode(), error.text()));
            emit storageError(QStringLiteral("数据库写入异常"));
            return false;
        }
    }

    if (!m_database.commit()) {
        Logger::error(QStringLiteral("SQLite 提交事务失败: %1").arg(m_database.lastError().text()));
        emit storageError(QStringLiteral("数据库写入异常"));
        m_database.rollback();
        return false;
    }

    m_pendingAlarmQueue.remove(0, batchCount);
    return true;
}

void DatabaseWorker::handleQueueOverflow()
{
    if (m_pendingAlarmQueue.size() < kMaxQueueSize) {
        return;
    }

    flushPendingRecords();
    if (m_pendingAlarmQueue.size() < kMaxQueueSize) {
        return;
    }

    const int dropCount = (m_pendingAlarmQueue.size() - kMaxQueueSize) + 1;
    m_pendingAlarmQueue.remove(0, dropCount);
    m_droppedRecordCount += dropCount;

    const QString message = QStringLiteral("报警日志队列拥塞，部分旧记录已丢弃。累计丢弃 %1 条")
                                .arg(m_droppedRecordCount);
    Logger::error(message);
    emit storageError(QStringLiteral("报警日志队列拥塞，部分旧记录已丢弃"));
}

void DatabaseWorker::flushPendingRecords()
{
    if (!flushBatch()) {
        Logger::error(QStringLiteral("报警日志批量 flush 失败"));
    }
}

void DatabaseWorker::closeDatabase()
{
    if (!m_connectionName.isEmpty() && QSqlDatabase::contains(m_connectionName)) {
        if (m_database.isOpen()) {
            m_database.close();
        }
        m_database = QSqlDatabase();//把当前成员变量 m_database 持有的那个连接句柄引用先释放掉
        QSqlDatabase::removeDatabase(m_connectionName);
        m_initialized = false;
        Logger::info(QStringLiteral("SQLite 数据库连接已关闭"));
    }
}
