#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>

/**
 * @file alarmcsvexporttask.h
 * @brief 报警日志 CSV 导出线程池任务声明。
 *
 * 这个文件定义 AlarmCsvExportTask。它同时继承 QObject 和 QRunnable：
 * QObject 用于发信号，QRunnable 用于交给 QThreadPool 执行。任务只读 SQLite
 * 并写 CSV 文件，不直接操作 Backend、QML 或任何模型。
 */

/**
 * @brief 在线程池线程中把 alarm_log 表分页导出为 UTF-8 CSV。
 */
class AlarmCsvExportTask final : public QObject, public QRunnable
{
    Q_OBJECT
public:
    // dbPath 是 data.db 路径，filePath 是用户通过 QML FileDialog 选择的 CSV 路径。
    AlarmCsvExportTask(QString dbPath, QString filePath);

    // QThreadPool 调用的入口函数，真正耗时的数据库读取和文件写入都在这里完成。
    void run() override;

signals:
    // 通用任务状态信号，由 BackgroundTaskManager queued 转发给 Backend。
    void taskStarted(const QString &taskName);
    void taskProgressChanged(const QString &taskName, int progress, const QString &message);
    void taskFinished(const QString &taskName, const QString &message);
    void taskFailed(const QString &taskName, const QString &errorMessage);

private:
    // 可执行文件目录下 data.db 的完整路径。
    QString m_dbPath;
    // 用户选择的 CSV 输出文件完整路径。
    QString m_filePath;
};
