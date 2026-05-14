#pragma once

#include "alarmrecorddata.h"

#include <QObject>
#include <QSqlDatabase>
#include <QTimer>
#include <QVector>

/**
 * @brief 专用数据库线程工作对象。
 *
 * DatabaseWorker 在 moveToThread() 前不设置 parent，也不创建 QSqlDatabase。
 * init() 通过 queued 调用在数据库线程内执行，数据库连接的创建、打开、建表、
 * 插入、关闭和 removeDatabase 全部发生在同一个线程，避免 Qt SQL 跨线程连接问题。
 *
 * 报警日志采用“数据库线程内队列 + 定时批量 flush”设计：
 * - Backend 只投递纯数据 DTO，不直接触碰数据库连接
 * - 队列只在数据库线程中读写，因此不需要额外跨线程加锁
 * - flush 始终在数据库线程里用事务执行，避免阻塞 UI 线程
 */
class DatabaseWorker final : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseWorker(QObject *parent = nullptr);

public slots:
    void init(const QString &applicationDir);
    void insertAlarmRecord(const AlarmRecordData &record);
    void shutdown();

signals:
    void storageError(const QString &message);
    void initialized(bool ok);

private:
    static constexpr int kFlushIntervalMs = 200;//每 200ms 定时刷一次
    static constexpr int kBatchSize = 100;//每批最多写 100 条，并且队列达到 100 条时立即触发 flush
    static constexpr int kMaxQueueSize = 5000;//内存待写队列上限

    bool ensureTable();
    bool flushBatch();
    void closeDatabase();
    void handleQueueOverflow();
    QString m_connectionName;
    QString m_databasePath;
    QSqlDatabase m_database;
    QVector<AlarmRecordData> m_pendingAlarmQueue;
    QTimer *m_flushTimer = nullptr;
    quint64 m_droppedRecordCount = 0;
    bool m_initialized = false;

private slots:
    void flushPendingRecords();


};
