#pragma once

#include <QColor>
#include <QString>

/**
 * @brief 报警表格的一行预处理数据。
 *
 * timestampText、valueText、statusText 都在插入时计算完成，data() 只做字段读取，
 * 避免 50,000 行模型在 QML TableView 滚动时反复格式化字符串。
 */
struct AlarmRecord
{
    QString timestampText;
    QString variableName;
    QString valueText;
    QString statusText;
    QColor statusColor;
    QColor foregroundColor;
    bool acknowledged = false;
    quint16 alarmBit = 0;
};
