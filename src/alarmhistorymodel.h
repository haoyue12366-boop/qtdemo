#pragma once

#include "backgroundtasktypes.h"

#include <QAbstractTableModel>
#include <QList>

/**
 * @file alarmhistorymodel.h
 * @brief 报警历史查询结果模型声明。
 *
 * 这个文件定义 AlarmHistoryModel。它只负责把线程池查询回来的
 * QList<AlarmHistoryRow> 暴露给 QML TableView。后台任务不能直接操作这个模型，
 * 必须由 Backend 在主线程调用 setRows()。
 */

/**
 * @brief QML 历史报警表使用的只读表格模型。
 */
class AlarmHistoryModel final : public QAbstractTableModel
{
    Q_OBJECT
public:
    // QML delegate 可通过这些 role 名称读取每列数据。
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TimestampRole,
        VariableNameRole,
        AlarmValueRole,
        StatusTextRole,
        AcknowledgedRole,
        AlarmBitRole
    };

    explicit AlarmHistoryModel(QObject *parent = nullptr);

    // QAbstractTableModel 基本接口，供 TableView 查询行列数和单元格数据。
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Backend 在主线程调用，用新的查询结果整体替换模型内容。
    void setRows(const QList<AlarmHistoryRow> &rows);

private:
    // 当前展示的历史查询结果；只在主线程读写。
    QList<AlarmHistoryRow> m_rows;
};
