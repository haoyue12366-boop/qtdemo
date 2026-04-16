#pragma once

#include <QMetaType>

/**
 * @brief 跨线程传递的纯采样数据。
 *
 * 该结构体不包含 QObject、数据库连接或 UI 对象，只作为 queued signal 的值类型，
 * 由 Backend 从通信线程接收后转发给 DatabaseWorker 在线程内写入 SQLite。
 */
struct TelemetryRecord
{
    float temperature = 0.0f;
    float pressure = 0.0f;
    float level = 0.0f;
    float flow = 0.0f;
    quint16 alarmStatus = 0;
};

Q_DECLARE_METATYPE(TelemetryRecord)
