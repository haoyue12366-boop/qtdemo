#pragma once

#include <QMetaType>

/**
 * @brief 通信线程向 UI 汇报的统计快照。
 *
 * SerialPortWorker 独占修改统计，Backend 只通过 queued signal 接收快照并暴露给 QML。
 */
struct CommunicationStats
{
    quint64 txCount = 0;
    quint64 rxCount = 0;
    quint64 timeoutCount = 0;
    quint64 retryCount = 0;
    quint64 crcErrorCount = 0;
    quint64 exceptionCount = 0;
    bool simulationMode = true;
};

Q_DECLARE_METATYPE(CommunicationStats)
