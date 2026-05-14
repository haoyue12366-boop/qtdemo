#include "backgroundtaskmanager.h"

#include "tasks/alarmcsvexporttask.h"
#include "tasks/alarmhistoryquerytask.h"
#include "tasks/alarmstatstask.h"

#include <QThread>
#include <QtGlobal>

/**
 * @brief 初始化后台任务线程池。
 *
 * 线程池属于 Backend 所在线程中的 QObject，但 QRunnable::run() 会在线程池工作线程执行。
 * 最大线程数限制在 2 到 4 之间，避免 CSV 导出、历史查询、统计同时运行时把 CPU 或 SQLite
 * 读连接开得太多，也给 UI、串口线程、数据库写入线程留出调度空间。
 */
BackgroundTaskManager::BackgroundTaskManager(QObject *parent)
    : QObject(parent)
{
    // ideal 是系统建议线程数；ideal - 1 是给主线程和其它专用线程留余量。
    const int ideal = QThread::idealThreadCount();
    m_threadPool.setMaxThreadCount(qBound(2, ideal - 1, 4));
}

/**
 * @brief 关闭管理器时等待线程池任务收尾。
 *
 * clear() 只清理尚未开始的任务；已经开始的任务会继续运行。waitForDone(3000)
 * 给正在运行的 SQLite 读任务最多 3 秒收尾时间，降低程序退出时连接未释放的风险。
 */
BackgroundTaskManager::~BackgroundTaskManager()
{
    m_threadPool.clear();
    m_threadPool.waitForDone(3000);
}

/**
 * @brief 创建并提交报警 CSV 导出任务。
 *
 * 实现逻辑：
 * 1. new 一个 AlarmCsvExportTask，并交给 QThreadPool 自动删除。
 * 2. 把任务发出的 started/progress/finished/failed 信号 queued 转发给自己。
 * 3. start() 后任务的 run() 在线程池线程执行，不阻塞 UI。
 */
void BackgroundTaskManager::exportAlarmCsv(const QString &dbPath, const QString &filePath)
{
    auto *task = new AlarmCsvExportTask(dbPath, filePath);
    // autoDelete=true 表示 run() 结束后由 QThreadPool 删除 task 对象。
    task->setAutoDelete(true);
    connect(task, &AlarmCsvExportTask::taskStarted,
            this, &BackgroundTaskManager::taskStarted, Qt::QueuedConnection);
    connect(task, &AlarmCsvExportTask::taskProgressChanged,
            this, &BackgroundTaskManager::taskProgressChanged, Qt::QueuedConnection);
    connect(task, &AlarmCsvExportTask::taskFinished,
            this, &BackgroundTaskManager::taskFinished, Qt::QueuedConnection);
    connect(task, &AlarmCsvExportTask::taskFailed,
            this, &BackgroundTaskManager::taskFailed, Qt::QueuedConnection);
    m_threadPool.start(task);
}

/**
 * @brief 创建并提交报警历史查询任务。
 *
 * 实现逻辑与 CSV 导出类似，但多连接一个 queryFinished 信号。任务只负责查出
 * QList<AlarmHistoryRow>，真正更新 QAbstractTableModel 的动作由 Backend 在主线程完成。
 */
void BackgroundTaskManager::queryAlarmHistory(const QString &dbPath, const AlarmHistoryQueryOptions &options)
{
    auto *task = new AlarmHistoryQueryTask(dbPath, options);
    // 查询条件 options 是纯 DTO，复制给任务后不再依赖 Backend 内部状态。
    task->setAutoDelete(true);
    connect(task, &AlarmHistoryQueryTask::taskStarted,
            this, &BackgroundTaskManager::taskStarted, Qt::QueuedConnection);
    connect(task, &AlarmHistoryQueryTask::taskProgressChanged,
            this, &BackgroundTaskManager::taskProgressChanged, Qt::QueuedConnection);
    connect(task, &AlarmHistoryQueryTask::taskFinished,
            this, &BackgroundTaskManager::taskFinished, Qt::QueuedConnection);
    connect(task, &AlarmHistoryQueryTask::taskFailed,
            this, &BackgroundTaskManager::taskFailed, Qt::QueuedConnection);
    connect(task, &AlarmHistoryQueryTask::queryFinished,
            this, &BackgroundTaskManager::alarmHistoryQueryFinished, Qt::QueuedConnection);
    m_threadPool.start(task);
}

/**
 * @brief 创建并提交报警统计任务。
 *
 * 实现逻辑：
 * 1. 任务在线程池线程中执行多条 COUNT/MAX SQL。
 * 2. 结果通过 statsFinished 传回。
 * 3. Backend 收到后只更新统计属性，不让任务直接碰 UI。
 */
void BackgroundTaskManager::calculateAlarmStats(const QString &dbPath, const AlarmStatsOptions &options)
{
    auto *task = new AlarmStatsTask(dbPath, options);
    // 统计条件同样是纯 DTO，可以安全跨线程复制传递。
    task->setAutoDelete(true);
    connect(task, &AlarmStatsTask::taskStarted,
            this, &BackgroundTaskManager::taskStarted, Qt::QueuedConnection);
    connect(task, &AlarmStatsTask::taskProgressChanged,
            this, &BackgroundTaskManager::taskProgressChanged, Qt::QueuedConnection);
    connect(task, &AlarmStatsTask::taskFinished,
            this, &BackgroundTaskManager::taskFinished, Qt::QueuedConnection);
    connect(task, &AlarmStatsTask::taskFailed,
            this, &BackgroundTaskManager::taskFailed, Qt::QueuedConnection);
    connect(task, &AlarmStatsTask::statsFinished,
            this, &BackgroundTaskManager::alarmStatsFinished, Qt::QueuedConnection);
    m_threadPool.start(task);
}
