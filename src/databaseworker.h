#pragma once

#include "telemetryrecord.h"

#include <QObject>
#include <QSqlDatabase>

/**
 * @brief 专用数据库线程工作对象。
 *
 * DatabaseWorker 在 moveToThread() 前不设置 parent，也不创建 QSqlDatabase。
 * init() 通过 queued 调用在数据库线程内执行，数据库连接的创建、打开、建表、
 * 插入、关闭和 removeDatabase 全部发生在同一个线程，避免 Qt SQL 跨线程连接问题。
 */
class DatabaseWorker final : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseWorker(QObject *parent = nullptr);

public slots:
    void init(const QString &applicationDir);
    void insertTelemetry(const TelemetryRecord &record);
    void shutdown();

signals:
    void storageError(const QString &message);
    void initialized(bool ok);

private:
    bool ensureTable();
    void closeDatabase();

    QString m_connectionName;
    QString m_databasePath;
    QSqlDatabase m_database;
    bool m_initialized = false;
};
