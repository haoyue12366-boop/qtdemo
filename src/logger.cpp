#include "logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QTextStream>
#include <QThread>

Logger::~Logger()
{
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }
}

Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::initialize(const QString &applicationDir)
{
    Logger &logger = instance();
    QMutexLocker locker(&logger.m_mutex);

    if (logger.m_file.isOpen()) {
        logger.m_file.close();
    }

    const QDir dir(applicationDir);
    logger.m_file.setFileName(dir.filePath(QStringLiteral("log.txt")));
    if (!logger.m_file.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Cannot open log file:" << logger.m_file.fileName() << logger.m_file.errorString();
    }
}

void Logger::installQtMessageHandler()
{
    qInstallMessageHandler(Logger::qtMessageHandler);
}

void Logger::debug(const QString &message)
{
    instance().write(QStringLiteral("DEBUG"), message);
}

void Logger::info(const QString &message)
{
    instance().write(QStringLiteral("INFO"), message);
}

void Logger::warning(const QString &message)
{
    instance().write(QStringLiteral("WARN"), message);
}

void Logger::error(const QString &message)
{
    instance().write(QStringLiteral("ERROR"), message);
}

void Logger::bytes(const QString &prefix, const QByteArray &payload)
{
    instance().write(QStringLiteral("DEBUG"),
                     QStringLiteral("%1 %2").arg(prefix, QString::fromLatin1(payload.toHex(' ').toUpper())));
}

void Logger::write(const QString &level, const QString &message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QString threadId = QStringLiteral("0x%1")
                                 .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);
    const QString line = QStringLiteral("[%1] [%2] [Thread %3] %4")
                             .arg(timestamp, level, threadId, message);

    QMutexLocker locker(&m_mutex);
    QTextStream console(stdout);
    console << line << Qt::endl;

    if (m_file.isOpen()) {
        QTextStream stream(&m_file);
        stream << line << Qt::endl;
        m_file.flush();
    }
}

void Logger::qtMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    switch (type) {
    case QtDebugMsg:
        debug(message);
        break;
    case QtInfoMsg:
        info(message);
        break;
    case QtWarningMsg:
        warning(message);
        break;
    case QtCriticalMsg:
        error(message);
        break;
    case QtFatalMsg:
        error(message);
        abort();
    }
}
