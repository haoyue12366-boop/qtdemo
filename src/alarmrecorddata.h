#pragma once

#include <QMetaType>
#include <QString>

/**
 * @brief Backend 投递到 DatabaseWorker 的报警日志纯数据 DTO。
 *
 * 该结构体专门用于主线程到数据库线程之间的 queued 信号传输，
 * 不携带任何 UI Model 相关颜色、QObject 或视图状态。
 * 它描述的是一次需要落库的报警事件快照，而不是表格中的展示项。
 */
struct AlarmRecordData
{
    QString timestampText;
    QString variableName;
    QString valueText;
    QString statusText;
    bool acknowledged = false;
    quint16 alarmBit = 0;
    quint16 alarmStatus = 0;
    float temperature = 0.0f;
    float pressure = 0.0f;
    float level = 0.0f;
    float flow = 0.0f;
};

Q_DECLARE_METATYPE(AlarmRecordData)
