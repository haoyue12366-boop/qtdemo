#pragma once

#include <QSqlDatabase>
#include <QSqlError>
#include <QString>
#include <QThread>
#include <QUuid>

/**
 * @file tasksqlconnection.h
 * @brief 线程池任务专用 SQLite 连接 RAII 辅助类。
 *
 * Qt SQL 的连接与线程强相关，不能把 DatabaseWorker 里的 QSqlDatabase
 * 拿到线程池任务里复用。每个后台任务都应该在自己的 run() 线程里创建、
 * 打开、使用、关闭并 removeDatabase。这个类把这套收尾规则集中起来，
 * 避免每个任务重复写连接释放代码。
 */

/**
 * @brief 在线程池任务内部创建和销毁一个独立 SQLite 连接。
 *
 * 使用方式：
 * 1. 在 QRunnable::run() 中创建 TaskSqlConnection 局部变量。
 * 2. 调用 open() 打开数据库。
 * 3. 用 database() 创建 QSqlQuery。
 * 4. run() 退出时析构函数自动 close + removeDatabase。
 */
class TaskSqlConnection final
{
public:
    /**
     * @brief 构造唯一 connectionName，并创建 QSQLITE 连接句柄。
     * @param dbPath data.db 的完整路径。
     * @param prefix 任务类型前缀，用来让日志或调试时更容易识别连接来源。
     */
    TaskSqlConnection(const QString &dbPath, const QString &prefix)
        : m_connectionName(QStringLiteral("%1_%2_%3")
                               .arg(prefix)
                               .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16)
                               .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
    {
        // m_connectionName 同时包含任务前缀、当前线程 ID、UUID，避免不同任务连接重名。
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        m_database.setDatabaseName(dbPath);
    }

    /**
     * @brief 关闭并移除连接。
     *
     * 注意 removeDatabase 前必须先释放 QSqlDatabase 句柄，所以这里先把
     * m_database 置为空对象。任务代码中也不要让 QSqlQuery 活得比本对象更久。
     */
    ~TaskSqlConnection()
    {
        if (!m_connectionName.isEmpty() && QSqlDatabase::contains(m_connectionName)) {
            if (m_database.isOpen()) {
                m_database.close();
            }
            m_database = QSqlDatabase();
            QSqlDatabase::removeDatabase(m_connectionName);
        }
    }

    TaskSqlConnection(const TaskSqlConnection &) = delete;
    TaskSqlConnection &operator=(const TaskSqlConnection &) = delete;

    /**
     * @brief 打开 SQLite 数据库。
     * @param errorMessage 打开失败时写入 Qt SQL 错误文本，可为 nullptr。
     * @return true 表示打开成功。
     */
    bool open(QString *errorMessage)
    {
        if (m_database.open()) {
            return true;
        }
        if (errorMessage != nullptr) {
            *errorMessage = m_database.lastError().text();
        }
        return false;
    }

    /**
     * @brief 返回当前任务线程内可用的数据库句柄。
     *
     * 只能在创建它的同一个线程里使用，不要保存到 Backend 或其它线程。
     */
    QSqlDatabase database() const
    {
        return m_database;
    }

private:
    // Qt SQL 连接名，析构时用它 removeDatabase。
    QString m_connectionName;
    // 当前后台任务独占的 SQLite 连接句柄。
    QSqlDatabase m_database;
};
