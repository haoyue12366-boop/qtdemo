#pragma once

#include "backgroundtasktypes.h"

#include <QObject>
#include <QRunnable>
#include <QString>

/**
 * @file alarmhistoryquerytask.h
 * @brief 报警历史查询线程池任务声明。
 *
 * 这个文件定义 AlarmHistoryQueryTask。任务负责在线程池线程中查询 SQLite，
 * 返回 QList<AlarmHistoryRow> 纯数据结果。它不直接更新 AlarmHistoryModel，
 * 模型更新必须由 Backend 在主线程完成。
 */

/**
 * @brief 根据时间、关键字、状态和分页条件异步查询 alarm_log。
 */
class AlarmHistoryQueryTask final : public QObject, public QRunnable
{
    Q_OBJECT
public:
    // options 是 Backend 解析和限幅后的查询条件。
    AlarmHistoryQueryTask(QString dbPath, AlarmHistoryQueryOptions options);

    // QThreadPool 调用入口，里面创建独立 SQLite 连接并执行 SELECT。
    void run() override;

signals:
    // 通用任务状态信号。
    void taskStarted(const QString &taskName);
    void taskProgressChanged(const QString &taskName, int progress, const QString &message);
    void taskFinished(const QString &taskName, const QString &message);
    void taskFailed(const QString &taskName, const QString &errorMessage);
    // 查询成功后的纯数据结果，由 Backend 接收并写入 AlarmHistoryModel。
    void queryFinished(const QList<AlarmHistoryRow> &rows);

private:
    // data.db 完整路径。
    QString m_dbPath;
    // 查询条件 DTO，复制保存在任务对象内，避免访问 Backend 成员。
    AlarmHistoryQueryOptions m_options;
};
