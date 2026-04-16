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

    m_initialized = ensureTable();
    emit initialized(m_initialized);
}

void DatabaseWorker::insertTelemetry(const TelemetryRecord &record)
{
    if (!m_initialized || !m_database.isOpen()) {
        Logger::error(QStringLiteral("SQLite 写入失败: 数据库未初始化"));
        emit storageError(QStringLiteral("数据库写入异常"));
        return;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO telemetry(timestamp, temperature, pressure, level, flow, alarm_status) "
        "VALUES(:timestamp, :temperature, :pressure, :level, :flow, :alarm_status)"));
    query.bindValue(QStringLiteral(":timestamp"),
                    QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")));
    query.bindValue(QStringLiteral(":temperature"), record.temperature);
    query.bindValue(QStringLiteral(":pressure"), record.pressure);
    query.bindValue(QStringLiteral(":level"), record.level);
    query.bindValue(QStringLiteral(":flow"), record.flow);
    query.bindValue(QStringLiteral(":alarm_status"), record.alarmStatus);

    if (!query.exec()) {
        const QSqlError error = query.lastError();
        Logger::error(QStringLiteral("SQLite 写入失败: code=%1, text=%2")
                          .arg(error.nativeErrorCode(), error.text()));
        emit storageError(QStringLiteral("数据库写入异常"));
    }
}

void DatabaseWorker::shutdown()
{
    Logger::info(QStringLiteral("数据库线程准备关闭"));
    closeDatabase();
}

bool DatabaseWorker::ensureTable()
{
    QSqlQuery query(m_database);
    const bool ok = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS telemetry ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp TEXT NOT NULL,"
        "temperature REAL NOT NULL,"
        "pressure REAL NOT NULL,"
        "level REAL NOT NULL,"
        "flow REAL NOT NULL,"
        "alarm_status INTEGER NOT NULL)"));

    if (!ok) {
        const QString message = QStringLiteral("SQLite 建表失败: %1").arg(query.lastError().text());
        Logger::error(message);
        emit storageError(QStringLiteral("数据库写入异常"));
        return false;
    }
    Logger::info(QStringLiteral("SQLite 数据库初始化完成: %1").arg(m_databasePath));
    return true;
}

void DatabaseWorker::closeDatabase()
{
    if (!m_connectionName.isEmpty() && QSqlDatabase::contains(m_connectionName)) {
        if (m_database.isOpen()) {
            m_database.close();
        }
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        m_initialized = false;
        Logger::info(QStringLiteral("SQLite 数据库连接已关闭"));
    }
}
