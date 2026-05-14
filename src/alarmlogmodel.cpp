#include "alarmlogmodel.h"

#include <QDateTime>

namespace {
constexpr int ColumnCount = 4;

QString statusText(bool acknowledged)
{
    return acknowledged ? QStringLiteral("已确认") : QStringLiteral("激活");
}

QColor statusColor(bool acknowledged)
{
    return acknowledged ? QColor(QStringLiteral("#1f8a4c")) : QColor(QStringLiteral("#b3261e"));
}

QColor foregroundColor(bool acknowledged)
{
    return acknowledged ? QColor(QStringLiteral("#d7efe1")) : QColor(QStringLiteral("#fff1f1"));
}

QString alarmName(quint16 bit)
{
    switch (bit) {
    case 0x0001:
        return QStringLiteral("温度高高");
    case 0x0002:
        return QStringLiteral("压力高高");
    case 0x0004:
        return QStringLiteral("流量高高");
    case 0x0008:
        return QStringLiteral("液位低低");
    default:
        return QStringLiteral("未知报警");
    }
}

QString alarmValueText(quint16 bit, float temperature, float pressure, float level, float flow)
{
    switch (bit) {
    case 0x0001:
        return QStringLiteral("%1 C").arg(QString::number(temperature, 'f', 1));
    case 0x0002:
        return QStringLiteral("%1 MPa").arg(QString::number(pressure, 'f', 2));
    case 0x0004:
        return QStringLiteral("%1 m3/h").arg(QString::number(flow, 'f', 1));
    case 0x0008:
        return QStringLiteral("%1 m").arg(QString::number(level, 'f', 2));
    default:
        return QStringLiteral("-");
    }
}
}

AlarmLogModel::AlarmLogModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    generateInitialData();
}

int AlarmLogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

int AlarmLogModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant AlarmLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size()) {
        return {};
    }

    const AlarmRecord &record = m_records.at(index.row());
    if (role == Qt::BackgroundRole && index.column() == 3) {
        return record.statusColor;
    }
    if (role == Qt::ForegroundRole) {
        return record.foregroundColor;
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return record.timestampText;
        case 1:
            return record.variableName;
        case 2:
            return record.valueText;
        case 3:
            return record.statusText;
        default:
            return {};
        }
    }

    switch (role) {
    case TimestampRole:
        return record.timestampText;
    case VariableNameRole:
        return record.variableName;
    case AlarmValueRole:
        return record.valueText;
    case StatusTextRole:
        return record.statusText;
    default:
        return {};
    }
}

QVariant AlarmLogModel::headerData(int section, Qt::Orientation orientation, int role) const//必须重写这个表头函数
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case 0:
        return QStringLiteral("时间戳");
    case 1:
        return QStringLiteral("变量名");
    case 2:
        return QStringLiteral("报警值");
    case 3:
        return QStringLiteral("状态");
    default:
        return {};
    }
}

QHash<int, QByteArray> AlarmLogModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {TimestampRole, "timestampText"},
        {VariableNameRole, "variableName"},
        {AlarmValueRole, "alarmValue"},
        {StatusTextRole, "statusText"},
        {Qt::BackgroundRole, "statusColor"},
        {Qt::ForegroundRole, "foregroundColor"},
    };
}

void AlarmLogModel::updateFromAlarmStatus(quint16 alarmStatus, float temperature, float pressure, float level, float flow)
{
    if (alarmStatus == m_lastAlarmStatus) {
        return;
    }

    const quint16 newAlarmBits = alarmStatus & ~m_lastAlarmStatus;
    for (quint16 bit : {quint16(0x0001), quint16(0x0002), quint16(0x0004), quint16(0x0008)}) {
        if ((newAlarmBits & bit) == 0) {
            continue;
        }
        AlarmRecord record;
        record.timestampText = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
        record.variableName = alarmName(bit);
        record.valueText = alarmValueText(bit, temperature, pressure, level, flow);
        record.acknowledged = false;
        record.statusText = statusText(false);
        record.statusColor = statusColor(false);
        record.foregroundColor = foregroundColor(false);
        record.alarmBit = bit;

        beginInsertRows(QModelIndex(), 0, 0);//告诉视图：我要在第 0 行插入 1 条
        m_records.prepend(record);//记录差入容器头部
        endInsertRows();//插入结束

        if (m_records.size() > 50000) {
            const int lastRow = m_records.size() - 1;
            beginRemoveRows(QModelIndex(), lastRow, lastRow);
            m_records.removeLast();
            endRemoveRows();
        }
    }

    m_lastAlarmStatus = alarmStatus;
}

void AlarmLogModel::acknowledgeByAlarmStatus(quint16 alarmStatusMask)//根据某个报警位掩码，把表里对应的未确认报警批量改成已确认，并通过 dataChanged 通知视图局部刷新。
{
    if (alarmStatusMask == 0 || m_records.isEmpty()) {
        return;
    }

    int firstChangedRow = -1;
    int lastChangedRow = -1;
    for (int row = 0; row < m_records.size(); ++row) {
        AlarmRecord &record = m_records[row];
        if (record.acknowledged || record.alarmBit == 0 || (record.alarmBit & alarmStatusMask) == 0) {
            continue;
        }

        record.acknowledged = true;
        record.statusText = statusText(true);
        record.statusColor = statusColor(true);
        record.foregroundColor = foregroundColor(true);

        if (firstChangedRow < 0) {
            firstChangedRow = row;
        }
        lastChangedRow = row;
    }

    if (firstChangedRow >= 0) {
        emit dataChanged(index(firstChangedRow, 0), index(lastChangedRow, ColumnCount - 1));
    }
}

void AlarmLogModel::generateInitialData()
{
    beginResetModel();
    m_records.clear();
    m_records.reserve(50000);

    const QStringList variables = {
        QStringLiteral("温度"),
        QStringLiteral("压力"),
        QStringLiteral("液位"),
        QStringLiteral("流量")
    };
    const QDateTime base = QDateTime::currentDateTime();
    for (int i = 0; i < 50000; ++i) {
        const bool acknowledged = (i % 3) != 0;
        AlarmRecord record;
        record.timestampText = base.addSecs(-i * 2).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        record.variableName = variables.at(i % variables.size());
        record.valueText = QStringLiteral("%1.%2").arg(20 + (i % 80)).arg(i % 10);
        record.acknowledged = acknowledged;
        record.statusText = statusText(acknowledged);
        record.statusColor = statusColor(acknowledged);
        record.foregroundColor = foregroundColor(acknowledged);
        m_records.append(record);
    }
    endResetModel();
}
