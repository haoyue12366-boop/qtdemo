#include "alarmhistoryquerytask.h"

#include "logger.h"
#include "tasksqlconnection.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <utility>

namespace {
// 与 BackgroundTaskManager/Backend 约定的任务名称。
const QString TaskName = QStringLiteral("报警历史查询");

/**
 * @brief 把 QDateTime 转成 alarm_log.timestamp 使用的文本格式。
 *
 * 数据库里 timestamp 是 TEXT，因此查询条件也绑定同格式字符串。
 */
QString timestampText(const QDateTime &dateTime)
{
    return dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

/**
 * @brief 判断 UI 传入的状态文本是否表示“激活/未确认”。
 *
 * 这里兼容中文和英文，便于后续 UI 文案调整。
 */
bool wantsActive(const QString &status)
{
    const QString value = status.trimmed().toLower();
    return value == QStringLiteral("激活")
           || value == QStringLiteral("未确认")
           || value == QStringLiteral("active");
}

/**
 * @brief 判断 UI 传入的状态文本是否表示“已确认”。
 */
bool wantsAcknowledged(const QString &status)
{
    const QString value = status.trimmed().toLower();
    return value == QStringLiteral("已确认")
           || value == QStringLiteral("acknowledged")
           || value == QStringLiteral("ack");
}
}

/**
 * @brief 保存历史查询任务的输入参数。
 *
 * options 是纯 DTO，复制到任务后，run() 不需要也不允许访问 Backend 的可变状态。
 */
AlarmHistoryQueryTask::AlarmHistoryQueryTask(QString dbPath, AlarmHistoryQueryOptions options)
    : m_dbPath(std::move(dbPath))
    , m_options(std::move(options))
{
}

/**
 * @brief 在线程池线程中执行历史报警查询。
 *
 * 实现逻辑：
 * 1. 通知 Backend 任务开始。
 * 2. 用 TaskSqlConnection 创建本任务独立 SQLite 连接。
 * 3. 根据 options 动态组合 WHERE 条件，但所有用户输入都用 bindValue。
 * 4. 执行分页 SELECT，并把每行转换成 AlarmHistoryRow。
 * 5. 通过 queryFinished 把纯数据结果发回主线程。
 */
void AlarmHistoryQueryTask::run()
{
    emit taskStarted(TaskName);
    emit taskProgressChanged(TaskName, 0, QStringLiteral("正在查询报警历史"));
    Logger::info(QStringLiteral("开始查询报警历史"));

    TaskSqlConnection connection(m_dbPath, QStringLiteral("alarm_history_query"));
    QString openError;
    if (!connection.open(&openError)) {
        const QString error = QStringLiteral("SQLite 打开失败: dbPath=%1, error=%2").arg(m_dbPath, openError);
        Logger::error(error);
        emit taskFailed(TaskName, QStringLiteral("历史查询失败: 数据库打开异常"));
        return;
    }

    // where 保存 SQL 条件片段，初始 1=1 方便后续统一追加 AND 条件。
    QStringList where = {QStringLiteral("1=1")};
    if (m_options.startTime.isValid()) {
        where.append(QStringLiteral("timestamp >= :startTime"));
    }
    if (m_options.endTime.isValid()) {
        where.append(QStringLiteral("timestamp <= :endTime"));
    }
    if (!m_options.keyword.trimmed().isEmpty()) {
        where.append(QStringLiteral(
            "(variable_name LIKE :keyword OR alarm_value LIKE :keyword OR status_text LIKE :keyword)"));
    }
    if (wantsActive(m_options.status)) {
        where.append(QStringLiteral("acknowledged = 0"));
    } else if (wantsAcknowledged(m_options.status)) {
        where.append(QStringLiteral("acknowledged = 1"));
    }

    // db 和 query 都只在当前线程池线程内使用。
    QSqlDatabase db = connection.database();
    QSqlQuery query(db);
    // 表名和列名来自固定代码；用户输入不会拼进 SQL，只拼接安全的条件片段。
    query.prepare(QStringLiteral(
                      "SELECT id, timestamp, variable_name, alarm_value, status_text, acknowledged, "
                      "alarm_bit, alarm_status, temperature, pressure, level, flow "
                      "FROM alarm_log WHERE %1 ORDER BY id DESC LIMIT :limit OFFSET :offset")
                      .arg(where.join(QStringLiteral(" AND "))));

    // 时间、关键字、分页值全部绑定，避免 SQL 注入和字符串转义问题。
    if (m_options.startTime.isValid()) {
        query.bindValue(QStringLiteral(":startTime"), timestampText(m_options.startTime));
    }
    if (m_options.endTime.isValid()) {
        query.bindValue(QStringLiteral(":endTime"), timestampText(m_options.endTime));
    }
    if (!m_options.keyword.trimmed().isEmpty()) {
        query.bindValue(QStringLiteral(":keyword"),
                        QStringLiteral("%") + m_options.keyword.trimmed() + QStringLiteral("%"));
    }
    query.bindValue(QStringLiteral(":limit"), m_options.limit);
    query.bindValue(QStringLiteral(":offset"), m_options.offset);

    if (!query.exec()) {
        const QString error = QStringLiteral("报警历史查询失败: %1").arg(query.lastError().text());
        Logger::error(error);
        emit taskFailed(TaskName, QStringLiteral("历史查询失败: SQL执行异常"));
        return;
    }

    // rows 是任务内部临时结果容器，发信号时会复制给接收方。
    QList<AlarmHistoryRow> rows;
    rows.reserve(m_options.limit);
    while (query.next()) {
        // 列索引必须与 SELECT 字段顺序保持一致。
        AlarmHistoryRow row;
        row.id = query.value(0).toLongLong();
        row.timestamp = query.value(1).toString();
        row.variableName = query.value(2).toString();
        row.alarmValue = query.value(3).toString();
        row.statusText = query.value(4).toString();
        row.acknowledged = query.value(5).toInt() == 1;
        row.alarmBit = query.value(6).toInt();
        row.alarmStatus = query.value(7).toInt();
        row.temperature = query.value(8).toDouble();
        row.pressure = query.value(9).toDouble();
        row.level = query.value(10).toDouble();
        row.flow = query.value(11).toDouble();
        rows.append(row);
    }

    emit queryFinished(rows);
    emit taskProgressChanged(TaskName, 100, QStringLiteral("历史查询完成"));
    const QString message = QStringLiteral("历史查询完成: %1 条").arg(rows.size());
    Logger::info(message);
    emit taskFinished(TaskName, message);
}
