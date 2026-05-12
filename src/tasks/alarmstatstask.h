#pragma once

#include "backgroundtasktypes.h"

#include <QObject>
#include <QRunnable>
#include <QString>

/**
 * @file alarmstatstask.h
 * @brief 报警统计分析线程池任务声明。
 *
 * 这个文件定义 AlarmStatsTask。任务在线程池线程中对 alarm_log 执行多条
 * COUNT/MAX 查询，把结果打包为 AlarmStatsResult。它不直接改 Backend 属性，
 * 只通过 signal 把结果交回主线程。
 */

/**
 * @brief 按可选时间范围异步统计报警日志。
 */
class AlarmStatsTask final : public QObject, public QRunnable
{
    Q_OBJECT
public:
    // options 当前包含开始/结束时间，后续扩展统计维度也应放入 DTO。
    AlarmStatsTask(QString dbPath, AlarmStatsOptions options);

    // QThreadPool 调用入口，里面打开独立 SQLite 连接并执行统计 SQL。
    void run() override;

signals:
    // 通用任务状态信号。
    void taskStarted(const QString &taskName);
    void taskProgressChanged(const QString &taskName, int progress, const QString &message);
    void taskFinished(const QString &taskName, const QString &message);
    void taskFailed(const QString &taskName, const QString &errorMessage);
    // 统计成功后的结果 DTO。
    void statsFinished(const AlarmStatsResult &result);

private:
    // data.db 完整路径。
    QString m_dbPath;
    // 统计条件 DTO。
    AlarmStatsOptions m_options;
};
