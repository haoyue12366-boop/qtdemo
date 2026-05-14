#include "alarmcsvexporttask.h"

#include "logger.h"
#include "tasksqlconnection.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringConverter>
#include <QTextStream>
#include <utility>

namespace {
// 每页导出 1000 条，避免一次性把 alarm_log 全部读入内存。
constexpr int PageSize = 1000;
// 任务名会回传给 Backend，用来区分进度来源。
const QString TaskName = QStringLiteral("报警CSV导出");

/**
 * @brief 对单个 CSV 字段做 RFC 常见规则转义。
 *
 * 实现逻辑：
 * 1. 如果字段包含逗号、双引号、换行，则整个字段用双引号包裹。
 * 2. 字段内部的双引号要写成两个双引号。
 */
QString csvEscape(QString value)
{
    const bool needsQuotes = value.contains(QLatin1Char(','))
                             || value.contains(QLatin1Char('"'))
                             || value.contains(QLatin1Char('\n'))
                             || value.contains(QLatin1Char('\r'));
    value.replace(QLatin1String("\""), QLatin1String("\"\""));
    return needsQuotes ? QStringLiteral("\"%1\"").arg(value) : value;
}

/**
 * @brief 把多个字段拼成一行 CSV 文本。
 *
 * 每个字段先走 csvEscape()，最后用逗号连接。这里不追加换行，调用方负责写 '\n'。
 */
QString csvLine(const QList<QString> &fields)
{
    QStringList escaped;
    escaped.reserve(fields.size());
    for (const QString &field : fields) {
        escaped.append(csvEscape(field));
    }
    return escaped.join(QLatin1Char(','));
}
}

/**
 * @brief 保存导出任务的输入参数。
 *
 * dbPath 和 filePath 都是值类型字符串，复制到任务对象后，即使 Backend 后续状态变化，
 * 当前导出任务也只按创建时的参数执行。
 */
AlarmCsvExportTask::AlarmCsvExportTask(QString dbPath, QString filePath)
    : m_dbPath(std::move(dbPath))
    , m_filePath(std::move(filePath))
{
}

/**
 * @brief 在线程池线程中执行 CSV 导出。
 *
 * 实现逻辑：
 * 1. 发出开始信号并记录日志。
 * 2. 用 TaskSqlConnection 在线程池线程内创建独立 SQLite 连接。
 * 3. 先查询 alarm_log 总数，用于进度计算。
 * 4. 打开用户选择的 CSV 文件，写 UTF-8 表头。
 * 5. 按 PageSize 分页查询 alarm_log，逐行转义并写入文件。
 * 6. flush 文件，发出 100% 进度和完成信号。
 * 7. 任意步骤失败都发 taskFailed，Backend 收到后负责更新 UI 错误状态。
 */
void AlarmCsvExportTask::run()
{
    emit taskStarted(TaskName);
    emit taskProgressChanged(TaskName, 0, QStringLiteral("正在准备导出"));
    Logger::info(QStringLiteral("开始导出报警CSV: %1").arg(m_filePath));

    // connection 是本任务独占的 SQLite 连接，析构时会自动关闭并 removeDatabase。
    TaskSqlConnection connection(m_dbPath, QStringLiteral("alarm_csv_export"));
    QString openError;
    if (!connection.open(&openError)) {
        const QString error = QStringLiteral("SQLite 打开失败: dbPath=%1, error=%2").arg(m_dbPath, openError);
        Logger::error(error);
        emit taskFailed(TaskName, QStringLiteral("CSV导出失败: 数据库打开异常"));
        return;
    }

    // db 只在当前 run() 所在线程使用，不跨线程保存。
    QSqlDatabase db = connection.database();
    // 先查总数，后续每导出一页就能按 exported / totalCount 计算进度。
    QSqlQuery countQuery(db);
    if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM alarm_log")) || !countQuery.next()) {
        const QString error = QStringLiteral("CSV导出统计总数失败: %1").arg(countQuery.lastError().text());
        Logger::error(error);
        emit taskFailed(TaskName, QStringLiteral("CSV导出失败: 查询报警日志失败"));
        return;
    }

    const int totalCount = countQuery.value(0).toInt();
    // m_filePath 已经由 Backend 校验目录存在，这里负责真正打开并覆盖写入。
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        const QString error = QStringLiteral("CSV文件打开失败: filePath=%1, error=%2")
                                  .arg(m_filePath, file.errorString());
        Logger::error(error);
        emit taskFailed(TaskName, QStringLiteral("CSV导出失败: 文件无法写入"));
        return;
    }

    QTextStream stream(&file);
    // 明确使用 UTF-8，避免中文表头和报警文本在 Excel/文本编辑器中乱码。
    stream.setEncoding(QStringConverter::Utf8);
    stream << csvLine({QStringLiteral("ID"),
                       QStringLiteral("时间"),
                       QStringLiteral("变量名"),
                       QStringLiteral("报警值"),
                       QStringLiteral("状态文本"),
                       QStringLiteral("已确认"),
                       QStringLiteral("报警位"),
                       QStringLiteral("报警状态"),
                       QStringLiteral("温度"),
                       QStringLiteral("压力"),
                       QStringLiteral("液位"),
                       QStringLiteral("流量")})
           << '\n';

    // exported 是已经成功写入 CSV 的数据行数，不包含表头。
    int exported = 0;
    for (int offset = 0; offset < totalCount; offset += PageSize) {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT id, timestamp, variable_name, alarm_value, status_text, acknowledged, "
            "alarm_bit, alarm_status, temperature, pressure, level, flow "
            "FROM alarm_log ORDER BY id DESC LIMIT :limit OFFSET :offset"));
        query.bindValue(QStringLiteral(":limit"), PageSize);
        query.bindValue(QStringLiteral(":offset"), offset);

        if (!query.exec()) {
            const QString error = QStringLiteral("CSV导出分页查询失败: %1").arg(query.lastError().text());
            Logger::error(error);
            emit taskFailed(TaskName, QStringLiteral("CSV导出失败: 分页查询异常"));
            return;
        }

        while (query.next()) {
            // 字段顺序必须与 CSV 表头一致，且按 alarm_log 实际列名读取。
            stream << csvLine({query.value(0).toString(),
                               query.value(1).toString(),
                               query.value(2).toString(),
                               query.value(3).toString(),
                               query.value(4).toString(),
                               query.value(5).toInt() == 1 ? QStringLiteral("是") : QStringLiteral("否"),
                               query.value(6).toString(),
                               query.value(7).toString(),
                               query.value(8).toString(),
                               query.value(9).toString(),
                               query.value(10).toString(),
                               query.value(11).toString()})
                   << '\n';
            ++exported;
        }

        // 进度最多报到 99，最后 flush 成功后再统一报 100。
        const int progress = totalCount == 0 ? 100 : qMin(99, exported * 100 / totalCount);
        emit taskProgressChanged(TaskName, progress,
                                 QStringLiteral("已导出 %1 / %2 条").arg(exported).arg(totalCount));
    }

    // 确保 QTextStream/QFile 缓冲落盘，避免 UI 提示成功但文件尚未完整写出。
    if (!file.flush()) {
        const QString error = QStringLiteral("CSV文件刷新失败: filePath=%1, error=%2")
                                  .arg(m_filePath, file.errorString());
        Logger::error(error);
        emit taskFailed(TaskName, QStringLiteral("CSV导出失败: 文件写入异常"));
        return;
    }

    emit taskProgressChanged(TaskName, 100, QStringLiteral("CSV导出完成"));
    const QString message = QStringLiteral("CSV导出完成: %1 条，%2").arg(exported).arg(m_filePath);
    Logger::info(message);
    emit taskFinished(TaskName, message);
}
