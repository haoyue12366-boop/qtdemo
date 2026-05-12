#pragma once

#include "backgroundtasktypes.h"

#include <QObject>
#include <QThreadPool>

/**
 * @file backgroundtaskmanager.h
 * @brief Backend 拥有的后台任务管理器声明。
 *
 * 这里定义 BackgroundTaskManager。它统一持有 QThreadPool，并提供提交
 * CSV 导出、历史查询、报警统计三类短生命周期任务的方法。串口通信和
 * 数据库写入仍然使用各自的 QThread，不经过这个线程池。
 */

/**
 * @brief 统一提交和转发 QThreadPool 后台任务结果。
 *
 * 对 Backend 来说，这个类是“后台任务入口”：Backend 调用提交方法，
 * 管理器创建 QRunnable 任务并 start 到线程池。任务完成后通过信号回到
 * 管理器，再由管理器把信号 queued 转发给 Backend。
 */
class BackgroundTaskManager final : public QObject
{
    Q_OBJECT
public:
    explicit BackgroundTaskManager(QObject *parent = nullptr);
    ~BackgroundTaskManager() override;

    // 提交报警 CSV 导出任务。filePath 是用户通过 FileDialog 选择的目标路径。
    void exportAlarmCsv(const QString &dbPath, const QString &filePath);
    // 提交报警历史查询任务。查询条件已经由 Backend 做过基础校验和归一化。
    void queryAlarmHistory(const QString &dbPath, const AlarmHistoryQueryOptions &options);
    // 提交报警统计任务。统计条件目前只包含可选时间范围。
    void calculateAlarmStats(const QString &dbPath, const AlarmStatsOptions &options);

signals:
    // 通用任务状态信号，Backend 用它更新进度条、状态栏等 UI 可见属性。
    void taskStarted(const QString &taskName);
    void taskProgressChanged(const QString &taskName, int progress, const QString &message);
    void taskFinished(const QString &taskName, const QString &message);
    void taskFailed(const QString &taskName, const QString &errorMessage);
    // 历史查询的专用结果信号，Backend 收到后更新 AlarmHistoryModel。
    void alarmHistoryQueryFinished(const QList<AlarmHistoryRow> &rows);
    // 统计分析的专用结果信号，Backend 收到后更新统计 Q_PROPERTY。
    void alarmStatsFinished(const AlarmStatsResult &result);

private:
    // 专属于后台短任务的线程池；不要把长生命周期串口/数据库写入任务放进来。
    QThreadPool m_threadPool;
};
