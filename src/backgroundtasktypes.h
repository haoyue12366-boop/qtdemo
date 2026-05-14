#pragma once

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>

/**
 * @file backgroundtasktypes.h
 * @brief 后台线程池任务使用的纯数据类型定义。
 *
 * 这个文件只放 DTO（Data Transfer Object），不放 QObject、不放数据库连接、
 * 不放 UI 模型对象。它们用于 Backend 与 QThreadPool 任务之间通过 queued signal
 * 传递参数或结果，所以底部需要 Q_DECLARE_METATYPE，并在 Backend 构造函数里注册。
 */

/**
 * @brief 报警历史查询条件。
 *
 * Backend 从 QML 收到字符串参数后，会先解析为这个结构体，再交给线程池任务。
 */
struct AlarmHistoryQueryOptions
{
    // 查询开始时间；无效值表示不限制开始时间。
    QDateTime startTime;
    // 查询结束时间；无效值表示不限制结束时间。
    QDateTime endTime;
    // 关键字会匹配 variable_name / alarm_value / status_text。
    QString keyword;
    // 状态筛选，当前支持“全部 / 激活 / 已确认”等文本。
    QString status;
    // 单次返回条数，上层会限制到 1..5000。
    int limit = 500;
    // 分页偏移量，上层会保证不小于 0。
    int offset = 0;
};

/**
 * @brief alarm_log 表中一条历史报警记录的展示数据。
 *
 * 这里字段名称贴近 QML 和模型显示，不直接暴露数据库列名。
 */
struct AlarmHistoryRow
{
    // alarm_log.id，便于定位具体数据库记录。
    qint64 id = 0;
    // alarm_log.timestamp，保持数据库里的文本格式。
    QString timestamp;
    // alarm_log.variable_name，报警变量名称。
    QString variableName;
    // alarm_log.alarm_value，触发报警时的值。
    QString alarmValue;
    // alarm_log.status_text，例如“激活”。
    QString statusText;
    // alarm_log.acknowledged，true 表示已确认。
    bool acknowledged = false;
    // alarm_log.alarm_bit，1/2/4/8 对应四类报警位。
    int alarmBit = 0;
    // alarm_log.alarm_status，当时完整的报警 bit mask。
    int alarmStatus = 0;
    // 以下四个值是报警发生时的遥测快照。
    double temperature = 0.0;
    double pressure = 0.0;
    double level = 0.0;
    double flow = 0.0;
};

/**
 * @brief 报警统计查询条件。
 *
 * 当前只按时间范围过滤；空时间表示统计全部报警日志。
 */
struct AlarmStatsOptions
{
    // 统计开始时间；无效值表示不限制开始时间。
    QDateTime startTime;
    // 统计结束时间；无效值表示不限制结束时间。
    QDateTime endTime;
};

/**
 * @brief 报警统计任务返回给 Backend 的汇总结果。
 */
struct AlarmStatsResult
{
    // alarm_log 总记录数。
    int totalCount = 0;
    // acknowledged = 0 的记录数。
    int activeCount = 0;
    // acknowledged = 1 的记录数。
    int acknowledgedCount = 0;
    // alarm_bit = 1 的记录数。
    int temperatureAlarmCount = 0;
    // alarm_bit = 2 的记录数。
    int pressureAlarmCount = 0;
    // alarm_bit = 8 的记录数。
    int levelAlarmCount = 0;
    // alarm_bit = 4 的记录数。
    int flowAlarmCount = 0;
    // MAX(timestamp)，没有记录时为空字符串。
    QString latestAlarmTime;
};

// 这些类型会跨线程 queued signal 传递，必须声明为 Qt metatype。
Q_DECLARE_METATYPE(AlarmHistoryQueryOptions)
Q_DECLARE_METATYPE(AlarmHistoryRow)
Q_DECLARE_METATYPE(QList<AlarmHistoryRow>)
Q_DECLARE_METATYPE(AlarmStatsOptions)
Q_DECLARE_METATYPE(AlarmStatsResult)
