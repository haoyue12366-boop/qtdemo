#include "backend.h"

#include "backgroundtaskmanager.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QTime>
#include <QUrl>

namespace {
// 这些任务名来自各 QRunnable。Backend 用它们判断应该更新哪组 UI 状态。
const QString CsvTaskName = QStringLiteral("报警CSV导出");
const QString HistoryTaskName = QStringLiteral("报警历史查询");
const QString StatsTaskName = QStringLiteral("报警统计分析");

/**
 * @brief 解析 QML 输入的时间字符串。
 *
 * 实现逻辑：
 * 1. 空字符串表示不限制时间范围，返回无效 QDateTime。
 * 2. 支持毫秒、秒、日期三种格式。
 * 3. 如果只输入日期，开始时间补 00:00:00.000，结束时间补 23:59:59.999。
 * 4. 如果结束时间只到秒，补 999ms，避免漏掉该秒内的报警。
 */
QDateTime parseInputDateTime(const QString &text, bool endOfRange)
{
    const QString value = text.trimmed();
    if (value.isEmpty()) {
        return {};
    }

    const QStringList formats = {
        QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"),
        QStringLiteral("yyyy-MM-dd HH:mm:ss"),
        QStringLiteral("yyyy-MM-dd")
    };

    for (const QString &format : formats) {
        QDateTime dateTime = QDateTime::fromString(value, format);
        if (!dateTime.isValid()) {
            continue;
        }
        if (format == QStringLiteral("yyyy-MM-dd")) {
            dateTime.setTime(endOfRange ? QTime(23, 59, 59, 999) : QTime(0, 0, 0, 0));
        } else if (endOfRange && format == QStringLiteral("yyyy-MM-dd HH:mm:ss")) {
            dateTime = dateTime.addMSecs(999);
        }
        return dateTime;
    }
    return {};
}

/**
 * @brief 把 QML FileDialog 返回值转换为本地文件路径。
 *
 * FileDialog 的 selectedFile 常见格式是 file:///D:/xxx.csv；Backend 和 QFile
 * 需要 D:/xxx.csv 这种本地路径，所以这里统一转换。
 */
QString localPathFromQmlValue(const QString &filePath)
{
    const QString trimmed = filePath.trimmed();
    if (trimmed.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) {
        return QUrl(trimmed).toLocalFile();
    }
    return trimmed;
}

/**
 * @brief 判断任务名是否是历史查询。
 *
 * Backend 的通用进度槽会收到所有任务的状态，需要按任务类型决定是否更新
 * alarmHistoryLoading/alarmHistoryMessage。
 */
bool isHistoryTask(const QString &taskName)
{
    return taskName == HistoryTaskName;
}

/**
 * @brief 判断任务名是否是统计分析。
 */
bool isStatsTask(const QString &taskName)
{
    return taskName == StatsTaskName;
}
}

/**
 * @brief 初始化 Backend 的后台任务管理器。
 *
 * 实现逻辑：
 * 1. 记录 data.db 的完整路径，后续所有后台读任务都用这个路径打开自己的连接。
 * 2. 创建 BackgroundTaskManager，父对象是 Backend，生命周期随 Backend 结束。
 * 3. 用 queued connection 接收任务状态和结果，确保属性和模型都在 Backend 主线程更新。
 */
void Backend::initializeBackgroundTasks(const QString &applicationDir)
{
    // m_databasePath 是后台读任务的数据库路径，不是数据库连接对象。
    m_databasePath = QDir(applicationDir).filePath(QStringLiteral("data.db"));
    m_backgroundTaskManager = new BackgroundTaskManager(this);

    connect(m_backgroundTaskManager, &BackgroundTaskManager::taskStarted,
            this, &Backend::handleBackgroundTaskStarted, Qt::QueuedConnection);
    connect(m_backgroundTaskManager, &BackgroundTaskManager::taskProgressChanged,
            this, &Backend::handleBackgroundTaskProgress, Qt::QueuedConnection);
    connect(m_backgroundTaskManager, &BackgroundTaskManager::taskFinished,
            this, &Backend::handleBackgroundTaskFinished, Qt::QueuedConnection);
    connect(m_backgroundTaskManager, &BackgroundTaskManager::taskFailed,
            this, &Backend::handleBackgroundTaskFailed, Qt::QueuedConnection);
    connect(m_backgroundTaskManager, &BackgroundTaskManager::alarmHistoryQueryFinished,
            this, &Backend::handleAlarmHistoryRows, Qt::QueuedConnection);
    connect(m_backgroundTaskManager, &BackgroundTaskManager::alarmStatsFinished,
            this, &Backend::handleAlarmStatsResult, Qt::QueuedConnection);
}

/**
 * @brief 返回历史查询模型给 QML。
 *
 * 模型对象属于 Backend 主线程，线程池任务只返回 QList<AlarmHistoryRow>，不直接操作模型。
 */
QObject *Backend::alarmHistoryModel()
{
    return &m_alarmHistoryModel;
}

/**
 * @brief QML 用来判断是否有后台任务正在执行。
 */
bool Backend::backgroundTaskRunning() const
{
    return m_backgroundTaskCount > 0;
}

/**
 * @brief 返回最近一个后台任务的进度百分比。
 */
int Backend::backgroundTaskProgress() const
{
    return m_backgroundTaskProgress;
}

/**
 * @brief 返回后台任务状态文本。
 */
QString Backend::backgroundTaskMessage() const
{
    return m_backgroundTaskMessage;
}

/**
 * @brief 返回历史查询是否正在执行。
 */
bool Backend::alarmHistoryLoading() const
{
    return m_alarmHistoryLoading;
}

/**
 * @brief 返回历史查询状态或结果说明。
 */
QString Backend::alarmHistoryMessage() const
{
    return m_alarmHistoryMessage;
}

/**
 * @brief 返回统计任务是否正在执行。
 */
bool Backend::alarmStatsLoading() const
{
    return m_alarmStatsLoading;
}

/**
 * @brief 返回 alarm_log 总记录数统计。
 */
int Backend::alarmTotalCount() const
{
    return m_alarmStatsResult.totalCount;
}

/**
 * @brief 返回未确认/激活报警数量。
 */
int Backend::alarmActiveCount() const
{
    return m_alarmStatsResult.activeCount;
}

/**
 * @brief 返回已确认报警数量。
 */
int Backend::alarmAcknowledgedCount() const
{
    return m_alarmStatsResult.acknowledgedCount;
}

/**
 * @brief 返回温度报警数量。
 */
int Backend::temperatureAlarmCount() const
{
    return m_alarmStatsResult.temperatureAlarmCount;
}

/**
 * @brief 返回压力报警数量。
 */
int Backend::pressureAlarmCount() const
{
    return m_alarmStatsResult.pressureAlarmCount;
}

/**
 * @brief 返回液位报警数量。
 */
int Backend::levelAlarmCount() const
{
    return m_alarmStatsResult.levelAlarmCount;
}

/**
 * @brief 返回流量报警数量。
 */
int Backend::flowAlarmCount() const
{
    return m_alarmStatsResult.flowAlarmCount;
}

/**
 * @brief 返回最近一次报警时间。
 */
QString Backend::latestAlarmTime() const
{
    return m_alarmStatsResult.latestAlarmTime;
}

/**
 * @brief QML 请求导出 CSV 时调用。
 *
 * 实现逻辑：
 * 1. 把 FileDialog 返回的 URL 转为本地路径。
 * 2. 校验空路径、补 .csv 后缀、确认父目录存在。
 * 3. 如果已有后台任务在跑，则拒绝重复启动。
 * 4. 把 data.db 路径和目标 CSV 路径交给 BackgroundTaskManager。
 */
void Backend::exportAlarmCsv(const QString &filePath)
{
    QString path = localPathFromQmlValue(filePath);
    if (path.isEmpty()) {
        setStatus(QStringLiteral("CSV导出路径为空"), true);
        return;
    }
    if (!path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) {
        path.append(QStringLiteral(".csv"));
    }

    // fileInfo 用于统一得到绝对路径和父目录对象。
    const QFileInfo fileInfo(path);
    if (!fileInfo.absoluteDir().exists()) {
        setStatus(QStringLiteral("CSV导出目录不存在"), true);
        return;
    }
    if (backgroundTaskRunning()) {
        setStatus(QStringLiteral("后台任务正在执行，请稍后再试"), false);
        return;
    }
    m_backgroundTaskManager->exportAlarmCsv(m_databasePath, fileInfo.absoluteFilePath());
}

/**
 * @brief QML 请求查询报警历史时调用。
 *
 * 实现逻辑：
 * 1. 防止同一时间重复启动历史查询。
 * 2. 把字符串时间解析为 QDateTime。
 * 3. 对 limit/offset 做限幅，避免一次查询过多数据。
 * 4. 提交线程池任务，查询完成后由 handleAlarmHistoryRows() 更新模型。
 */
void Backend::queryAlarmHistory(const QString &startTime,
                                const QString &endTime,
                                const QString &keyword,
                                const QString &status,
                                int limit,
                                int offset)
{
    if (m_alarmHistoryLoading) {
        setStatus(QStringLiteral("报警历史正在查询"), false);
        return;
    }

    AlarmHistoryQueryOptions options;
    options.startTime = parseInputDateTime(startTime, false);
    options.endTime = parseInputDateTime(endTime, true);
    // keyword/status 保持为文本，由任务内部决定 SQL 条件。
    options.keyword = keyword.trimmed();
    options.status = status.trimmed();
    options.limit = qBound(1, limit, 5000);
    options.offset = qMax(0, offset);
    m_backgroundTaskManager->queryAlarmHistory(m_databasePath, options);
}

/**
 * @brief QML 请求刷新报警统计时调用。
 *
 * 统计任务只需要时间范围，空时间表示统计全部 alarm_log。
 */
void Backend::refreshAlarmStats(const QString &startTime, const QString &endTime)
{
    if (m_alarmStatsLoading) {
        setStatus(QStringLiteral("报警统计正在刷新"), false);
        return;
    }

    AlarmStatsOptions options;
    options.startTime = parseInputDateTime(startTime, false);
    options.endTime = parseInputDateTime(endTime, true);
    m_backgroundTaskManager->calculateAlarmStats(m_databasePath, options);
}

/**
 * @brief 后台任务开始时更新通用和分类 loading 状态。
 *
 * m_backgroundTaskCount 用来兼容未来多个后台任务并发的情况；当前 UI 会阻止多数重复任务。
 */
void Backend::handleBackgroundTaskStarted(const QString &taskName)
{
    ++m_backgroundTaskCount;
    m_backgroundTaskProgress = 0;
    m_backgroundTaskMessage = QStringLiteral("%1已启动").arg(taskName);
    if (isHistoryTask(taskName)) {
        m_alarmHistoryLoading = true;
        m_alarmHistoryMessage = QStringLiteral("正在查询报警历史");
        emit alarmHistoryChanged();
    } else if (isStatsTask(taskName)) {
        m_alarmStatsLoading = true;
        emit alarmStatsChanged();
    }
    emit backgroundTaskChanged();
}

/**
 * @brief 后台任务进度变化时更新 QML 可见状态。
 *
 * progress 被夹到 0..100，避免任务误传异常值影响 ProgressBar。
 */
void Backend::handleBackgroundTaskProgress(const QString &taskName, int progress, const QString &message)
{
    m_backgroundTaskProgress = qBound(0, progress, 100);
    m_backgroundTaskMessage = message;
    if (isHistoryTask(taskName)) {
        m_alarmHistoryMessage = message;
        emit alarmHistoryChanged();
    }
    emit backgroundTaskChanged();
}

/**
 * @brief 后台任务成功结束时清理 loading 状态并刷新状态栏。
 */
void Backend::handleBackgroundTaskFinished(const QString &taskName, const QString &message)
{
    m_backgroundTaskCount = qMax(0, m_backgroundTaskCount - 1);
    m_backgroundTaskProgress = 100;
    m_backgroundTaskMessage = message;
    if (isHistoryTask(taskName)) {
        m_alarmHistoryLoading = false;
        m_alarmHistoryMessage = message;
        emit alarmHistoryChanged();
    } else if (isStatsTask(taskName)) {
        m_alarmStatsLoading = false;
        emit alarmStatsChanged();
    }
    setStatus(message, false);
    emit backgroundTaskChanged();
}

/**
 * @brief 后台任务失败时清理 loading 状态并把错误暴露给 UI。
 *
 * 查询失败时不清空已有历史模型数据；这样用户还能看到上一次成功结果。
 */
void Backend::handleBackgroundTaskFailed(const QString &taskName, const QString &errorMessage)
{
    m_backgroundTaskCount = qMax(0, m_backgroundTaskCount - 1);
    m_backgroundTaskProgress = 0;
    m_backgroundTaskMessage = errorMessage;
    if (isHistoryTask(taskName)) {
        m_alarmHistoryLoading = false;
        m_alarmHistoryMessage = errorMessage;
        emit alarmHistoryChanged();
    } else if (isStatsTask(taskName)) {
        m_alarmStatsLoading = false;
        emit alarmStatsChanged();
    } else if (taskName == CsvTaskName) {
        emit backgroundTaskChanged();
    }
    setStatus(errorMessage, true);
    emit backgroundTaskChanged();
}

/**
 * @brief 历史查询任务返回结果时更新模型。
 *
 * 这个槽运行在 Backend 所在线程，因此可以安全调用 AlarmHistoryModel::setRows()。
 */
void Backend::handleAlarmHistoryRows(const QList<AlarmHistoryRow> &rows)
{
    m_alarmHistoryModel.setRows(rows);
    m_alarmHistoryMessage = QStringLiteral("历史查询返回 %1 条").arg(rows.size());
    emit alarmHistoryChanged();
}

/**
 * @brief 统计任务返回结果时更新统计属性。
 *
 * QML 中的统计摘要绑定这些 Q_PROPERTY，emit alarmStatsChanged() 后会自动刷新。
 */
void Backend::handleAlarmStatsResult(const AlarmStatsResult &result)
{
    m_alarmStatsResult = result;
    emit alarmStatsChanged();
}
