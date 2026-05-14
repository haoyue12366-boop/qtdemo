#include "alarmhistorymodel.h"

namespace {
// 历史报警表当前显示 6 列：时间、变量、报警值、状态、报警位、ID。
constexpr int ColumnCount = 6;
}

/**
 * @brief 构造历史模型。
 *
 * 模型初始为空；只有 Backend 收到 AlarmHistoryQueryTask 结果后才会调用 setRows() 填充。
 */
AlarmHistoryModel::AlarmHistoryModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

/**
 * @brief 返回当前历史查询结果行数。
 *
 * Qt 约定：如果 parent 有效，普通表格模型应返回 0，表示没有树形子节点。
 */
int AlarmHistoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

/**
 * @brief 返回历史表固定列数。
 */
int AlarmHistoryModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

/**
 * @brief 返回 QML TableView 请求的单元格或 role 数据。
 *
 * DisplayRole 按列号返回表格文本；自定义 role 让 QML delegate 也可以按名字读取字段。
 */
QVariant AlarmHistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const AlarmHistoryRow &row = m_rows.at(index.row());
    if (role == Qt::DisplayRole) {
        // 列顺序要和 headerData() 保持一致。
        switch (index.column()) {
        case 0:
            return row.timestamp;
        case 1:
            return row.variableName;
        case 2:
            return row.alarmValue;
        case 3:
            return row.statusText;
        case 4:
            return row.alarmBit;
        case 5:
            return row.id;
        default:
            return {};
        }
    }

    // 自定义 role 给 QML 使用，避免 QML 依赖列号解析复杂数据。
    switch (role) {
    case IdRole:
        return row.id;
    case TimestampRole:
        return row.timestamp;
    case VariableNameRole:
        return row.variableName;
    case AlarmValueRole:
        return row.alarmValue;
    case StatusTextRole:
        return row.statusText;
    case AcknowledgedRole:
        return row.acknowledged;
    case AlarmBitRole:
        return row.alarmBit;
    default:
        return {};
    }
}

/**
 * @brief 返回水平表头文本。
 *
 * HorizontalHeaderView 通过这个函数显示每一列标题。
 */
QVariant AlarmHistoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
    case 0:
        return QStringLiteral("时间");
    case 1:
        return QStringLiteral("变量名");
    case 2:
        return QStringLiteral("报警值");
    case 3:
        return QStringLiteral("状态");
    case 4:
        return QStringLiteral("报警位");
    case 5:
        return QStringLiteral("ID");
    default:
        return {};
    }
}

/**
 * @brief 声明 QML 可见的 role 名称。
 *
 * roleNames() 返回的名字会变成 delegate 中的 model.id、model.timestamp 等属性。
 */
QHash<int, QByteArray> AlarmHistoryModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {IdRole, "id"},
        {TimestampRole, "timestamp"},
        {VariableNameRole, "variableName"},
        {AlarmValueRole, "alarmValue"},
        {StatusTextRole, "statusText"},
        {AcknowledgedRole, "acknowledged"},
        {AlarmBitRole, "alarmBit"},
    };
}

/**
 * @brief 用新的历史查询结果整体刷新模型。
 *
 * 实现逻辑：
 * 1. beginResetModel() 通知视图旧数据即将失效。
 * 2. 替换 m_rows。
 * 3. endResetModel() 通知视图重新读取行列数据。
 *
 * 这个函数只能在 Backend/GUI 主线程调用，不能在线程池任务中直接调用。
 */
void AlarmHistoryModel::setRows(const QList<AlarmHistoryRow> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}
