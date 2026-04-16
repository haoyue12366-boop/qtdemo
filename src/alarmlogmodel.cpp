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

QVariant AlarmLogModel::headerData(int section, Qt::Orientation orientation, int role) const
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
    };
}

void AlarmLogModel::updateFromAlarmStatus(quint16 alarmStatus, float temperature, float pressure, float level, float flow)
{
    if (alarmStatus == m_lastAlarmStatus) {
        return;
    }

    if (alarmStatus == 0x0000) {
        setAcknowledged(true);
        m_lastAlarmStatus = alarmStatus;
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

        beginInsertRows(QModelIndex(), 0, 0);
        m_records.prepend(record);
        endInsertRows();

        if (m_records.size() > 50000) {
            const int lastRow = m_records.size() - 1;
            beginRemoveRows(QModelIndex(), lastRow, lastRow);
            m_records.removeLast();
            endRemoveRows();
        }
    }

    m_lastAlarmStatus = alarmStatus;
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
        m_records.append(record);
    }
    endResetModel();
}

void AlarmLogModel::setAcknowledged(bool acknowledged)
{
    if (m_records.isEmpty()) {
        return;
    }
    const int updateCount = qMin(100, m_records.size());
    for (int row = 0; row < updateCount; ++row) {
        AlarmRecord &record = m_records[row];
        record.acknowledged = acknowledged;
        record.statusText = statusText(acknowledged);
        record.statusColor = statusColor(acknowledged);
    }
    emit dataChanged(index(0, 0), index(updateCount - 1, ColumnCount - 1));
}
