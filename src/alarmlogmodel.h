#pragma once

#include "alarmrecord.h"

#include <QAbstractTableModel>
#include <QList>

/**
 * @brief 海量报警数据模型。
 *
 * 严格继承 QAbstractTableModel，内部预生成 50,000 条模拟报警记录。
 * 与 40009 报警状态联动：读取到 0xFF00 时批量置为已确认，读取到激活状态时更新顶部记录。
 */
class AlarmLogModel final : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Roles {
        TimestampRole = Qt::UserRole + 1,
        VariableNameRole,
        AlarmValueRole,
        StatusTextRole
    };

    explicit AlarmLogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

public slots:
    void updateFromAlarmStatus(quint16 alarmStatus, float temperature, float pressure, float level, float flow);

private:
    void generateInitialData();
    void setAcknowledged(bool acknowledged);

    QList<AlarmRecord> m_records;
    quint16 m_lastAlarmStatus = 0;
};
