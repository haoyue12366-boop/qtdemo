#include "alarmstatstask.h"

#include "logger.h"
#include "tasksqlconnection.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include <utility>

namespace {
// 与 Backend 识别任务类型用的名称保持一致。
const QString TaskName = QStringLiteral("报警统计分析");

/**
 * @brief 把 QDateTime 转成数据库 timestamp 文本格式。
 */
QString timestampText(const QDateTime &dateTime)
{
    return dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

/**
 * @brief 生成统计 SQL 的 WHERE 片段。
 *
 * 实现逻辑：
 * 1. 起始条件固定为 1=1，方便统一拼接 AND。
 * 2. startTime/endTime 有效时追加时间范围条件。
 * 3. extraCondition 用于追加 acknowledged 或 alarm_bit 等固定条件。
 */
QString whereSql(const AlarmStatsOptions &options, const QString &extraCondition = {})
{
    QStringList conditions = {QStringLiteral("1=1")};
    if (options.startTime.isValid()) {
        conditions.append(QStringLiteral("timestamp >= :startTime"));
    }
    if (options.endTime.isValid()) {
        conditions.append(QStringLiteral("timestamp <= :endTime"));
    }
    if (!extraCondition.isEmpty()) {
        conditions.append(extraCondition);
    }
    return QStringLiteral(" WHERE %1").arg(conditions.join(QStringLiteral(" AND ")));
}

/**
 * @brief 给统计 SQL 绑定时间范围参数。
 *
 * whereSql() 只生成占位符，这里负责把实际 QDateTime 文本绑定进去。
 */
void bindTimeRange(QSqlQuery &query, const AlarmStatsOptions &options)
{
    if (options.startTime.isValid()) {
        query.bindValue(QStringLiteral(":startTime"), timestampText(options.startTime));
    }
    if (options.endTime.isValid()) {
        query.bindValue(QStringLiteral(":endTime"), timestampText(options.endTime));
    }
}

/**
 * @brief 执行返回单个值的 SELECT 语句。
 *
 * 统计任务中 COUNT(*) 和 MAX(timestamp) 都是单值查询，所以抽出这个小函数。
 * db 是当前线程池任务自己的连接，sql 中只能包含代码生成的固定条件和占位符。
 */
bool scalarValue(QSqlDatabase db, const QString &sql, const AlarmStatsOptions &options,
                 QVariant *value, QString *error)
{
    QSqlQuery query(db);
    query.prepare(sql);
    bindTimeRange(query, options);
    if (!query.exec() || !query.next()) {
        if (error != nullptr) {
            *error = query.lastError().text();
        }
        return false;
    }
    if (value != nullptr) {
        *value = query.value(0);
    }
    return true;
}

/**
 * @brief 执行 COUNT(*) 查询并转成 int。
 *
 * extraCondition 是代码内固定条件，例如 acknowledged = 0 或 alarm_bit = 1，
 * 不接收用户输入。
 */
bool countValue(QSqlDatabase db, const AlarmStatsOptions &options, const QString &extraCondition,
                int *value, QString *error)
{
    QVariant result;
    const QString sql = QStringLiteral("SELECT COUNT(*) FROM alarm_log%1").arg(whereSql(options, extraCondition));
    if (!scalarValue(db, sql, options, &result, error)) {
        return false;
    }
    *value = result.toInt();
    return true;
}
}

/**
 * @brief 保存统计任务的输入参数。
 */
AlarmStatsTask::AlarmStatsTask(QString dbPath, AlarmStatsOptions options)
    : m_dbPath(std::move(dbPath))
    , m_options(std::move(options))
{
}

/**
 * @brief 在线程池线程中执行报警统计。
 *
 * 实现逻辑：
 * 1. 通知任务开始。
 * 2. 创建本任务独立 SQLite 连接。
 * 3. 分别统计总数、未确认、已确认、四类 alarm_bit 数量。
 * 4. 查询 MAX(timestamp) 得到最近报警时间。
 * 5. 通过 statsFinished 把 AlarmStatsResult 发回 Backend。
 */
void AlarmStatsTask::run()
{
    emit taskStarted(TaskName);
    emit taskProgressChanged(TaskName, 0, QStringLiteral("正在统计报警"));
    Logger::info(QStringLiteral("开始统计报警"));

    TaskSqlConnection connection(m_dbPath, QStringLiteral("alarm_stats"));
    QString openError;
    if (!connection.open(&openError)) {
        const QString error = QStringLiteral("SQLite 打开失败: dbPath=%1, error=%2").arg(m_dbPath, openError);
        Logger::error(error);
        emit taskFailed(TaskName, QStringLiteral("报警统计失败: 数据库打开异常"));
        return;
    }

    // db 只属于当前任务线程；result 是最终要回传给 Backend 的统计 DTO。
    QSqlDatabase db = connection.database();
    AlarmStatsResult result;
    // error 保存任意一条 SQL 失败时的 Qt SQL 错误文本，便于日志定位。
    QString error;
    if (!countValue(db, m_options, {}, &result.totalCount, &error)
        || !countValue(db, m_options, QStringLiteral("acknowledged = 0"), &result.activeCount, &error)
        || !countValue(db, m_options, QStringLiteral("acknowledged = 1"), &result.acknowledgedCount, &error)
        || !countValue(db, m_options, QStringLiteral("alarm_bit = 1"), &result.temperatureAlarmCount, &error)
        || !countValue(db, m_options, QStringLiteral("alarm_bit = 2"), &result.pressureAlarmCount, &error)
        || !countValue(db, m_options, QStringLiteral("alarm_bit = 8"), &result.levelAlarmCount, &error)
        || !countValue(db, m_options, QStringLiteral("alarm_bit = 4"), &result.flowAlarmCount, &error)) {
        Logger::error(QStringLiteral("报警统计查询失败: %1").arg(error));
        emit taskFailed(TaskName, QStringLiteral("报警统计失败: SQL执行异常"));
        return;
    }

    // latest 保存 MAX(timestamp) 的临时 QVariant，最后转成 QString 写入结果。
    QVariant latest;
    const QString latestSql = QStringLiteral("SELECT MAX(timestamp) FROM alarm_log%1").arg(whereSql(m_options));
    if (!scalarValue(db, latestSql, m_options, &latest, &error)) {
        Logger::error(QStringLiteral("报警最近时间查询失败: %1").arg(error));
        emit taskFailed(TaskName, QStringLiteral("报警统计失败: 最近报警查询异常"));
        return;
    }

    result.latestAlarmTime = latest.toString();
    emit statsFinished(result);
    emit taskProgressChanged(TaskName, 100, QStringLiteral("报警统计完成"));
    const QString message = QStringLiteral("报警统计完成: 共 %1 条").arg(result.totalCount);
    Logger::info(message);
    emit taskFinished(TaskName, message);
}
