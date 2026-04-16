#pragma once

#include <QFile>
#include <QMutex>
#include <QString>

/**
 * @brief 线程安全日志单例。
 *
 * 负责把业务日志、Qt 全局消息、Modbus 原始字节流同时输出到控制台和
 * 可执行文件同目录下的 log.txt。日志格式满足工业上位机排障需要：
 * [时间戳] [级别] [线程ID] 消息内容。
 */
class Logger final
{
public:
    static Logger &instance();

    static void initialize(const QString &applicationDir);
    static void installQtMessageHandler();

    static void debug(const QString &message);
    static void info(const QString &message);
    static void warning(const QString &message);
    static void error(const QString &message);
    static void bytes(const QString &prefix, const QByteArray &payload);

private:
    Logger() = default;
    ~Logger();

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    void write(const QString &level, const QString &message);
    static void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message);

    QFile m_file;
    QMutex m_mutex;
};
