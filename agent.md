# qthmi 项目上下文说明

## 1. 项目定位

`qthmi` 是一个基于 Qt 6.10.1 + C++17 + QML 的工业上位机仿真监控 Demo，用于展示工业 HMI 开发能力。

项目核心能力：

- QML 工业监控界面
- C++ `Backend` 作为 QML 与业务层桥接
- 串口通信与 Modbus RTU 协议处理
- 内置仿真模式，方便无真实设备调试
- 实时遥测数据显示
- Scene Graph 实时趋势曲线
- 报警判断、报警确认、报警实时大表展示
- SQLite 报警日志落库
- QThreadPool 短生命周期后台任务
- 报警日志 CSV 异步导出
- 报警历史查询
- 报警统计分析
- QSettings 配置保存
- 通信统计与日志记录

## 2. 技术栈

- Qt 6.10.1
- C++17
- QML / Qt Quick / Qt Quick Controls / Qt Quick Dialogs
- CMake
- QSerialPort
- SQLite / Qt SQL
- QThread + QObject Worker
- QThreadPool + QRunnable
- QQuickItem + QSGGeometryNode
- QSettings

## 3. 当前目录结构

```text
qml/
  AlarmPanel.qml
  CommunicationStatsPanel.qml
  ConfigPanel.qml
  Main.qml
  TelemetrySummaryPanel.qml
  TrendPanel.qml
src/
  main.cpp
  backend.h/.cpp
  backend_backgroundtasks.cpp
  backgroundtaskmanager.h/.cpp
  backgroundtasktypes.h
  alarmhistorymodel.h/.cpp
  tasks/
    tasksqlconnection.h
    alarmcsvexporttask.h/.cpp
    alarmhistoryquerytask.h/.cpp
    alarmstatstask.h/.cpp
  serialportworker.h/.cpp
  modbusprotocol.h/.cpp
  databaseworker.h/.cpp
  alarmevaluator.h/.cpp
  alarmlogmodel.h/.cpp
  curveplotprovider.h/.cpp
  configmanager.h/.cpp
  logger.h/.cpp
  communicationstats.h
  alarmrecord.h
  alarmrecorddata.h
  telemetryrecord.h
```

## 4. 总体架构

```text
QML UI 主线程
  |
  | 绑定 Q_PROPERTY / 调用 Q_INVOKABLE
  v
Backend
  |\
  | \ queued signal/slot
  |  \
  v   v
SerialPortWorker        DatabaseWorker
串口线程                数据库写入线程
  |                      |
QSerialPort / Modbus     SQLite / alarm_log

Backend
  |
  v
BackgroundTaskManager
  |
  v
QThreadPool
  |
  +-- AlarmCsvExportTask
  +-- AlarmHistoryQueryTask
  +-- AlarmStatsTask
```

核心原则：

1. QML 只负责界面展示和用户交互。
2. `Backend` 是 QML 与 C++ 服务层之间的桥接层。
3. 串口操作只放在 `SerialPortWorker`，继续使用专用 `QThread`。
4. 报警日志写入只放在 `DatabaseWorker`，继续使用专用 `QThread`。
5. `QThreadPool` 只用于短生命周期后台任务，例如 CSV 导出、历史查询、统计分析。
6. 跨线程只传基础类型或 DTO，例如 `AppConfig`、`AlarmRecordData`、`CommunicationStats`、`AlarmHistoryRow`、`AlarmStatsResult`。
7. 不要跨线程直接操作 `QObject`、`QSqlDatabase`、`QSqlQuery`、`QQuickItem`。
8. 后台任务不得直接修改 Backend 的 `Q_PROPERTY` 或任何 QML Model。
9. 后台任务完成后必须 queued 回到 Backend 主线程，再更新属性、Model 或 UI 可见状态。
10. 不要在 UI 线程执行阻塞操作。

## 5. 关键文件职责

| 文件 | 职责 |
|---|---|
| `src/main.cpp` | 应用入口、注册 QML 类型、注入 Backend |
| `src/backend.h/.cpp` | QML 门面层，管理配置、状态、遥测、趋势、报警模型和 worker 生命周期 |
| `src/backend_backgroundtasks.cpp` | Backend 的后台任务接口、属性 getter、任务回调处理 |
| `src/backgroundtaskmanager.h/.cpp` | 统一持有和管理 QThreadPool，提交后台任务并转发信号 |
| `src/backgroundtasktypes.h` | 后台任务 DTO 和 metatype 声明 |
| `src/tasks/tasksqlconnection.h` | 后台任务 SQLite 独立连接 RAII 辅助 |
| `src/tasks/alarmcsvexporttask.h/.cpp` | 报警日志 CSV 异步导出 |
| `src/tasks/alarmhistoryquerytask.h/.cpp` | 报警历史异步查询 |
| `src/tasks/alarmstatstask.h/.cpp` | 报警统计异步分析 |
| `src/alarmhistorymodel.h/.cpp` | 历史查询结果模型，供 QML TableView 使用 |
| `src/serialportworker.h/.cpp` | 串口线程对象，负责连接、断开、轮询、仿真模式和真实串口模式 |
| `src/modbusprotocol.h/.cpp` | Modbus RTU 构帧、解析、CRC、异常帧、仿真寄存器 |
| `src/databaseworker.h/.cpp` | 数据库线程对象，负责 SQLite 初始化、报警日志入队和批量写入 |
| `src/alarmevaluator.h/.cpp` | 根据温度、压力、液位、流量判断报警 bit mask |
| `src/alarmlogmodel.h/.cpp` | 实时报警大表模型，继承 `QAbstractTableModel` |
| `src/curveplotprovider.h/.cpp` | 自定义 QQuickItem，使用 Scene Graph 绘制实时趋势曲线 |
| `src/configmanager.h/.cpp` | 使用 QSettings 读写 `config.ini` |
| `src/logger.h/.cpp` | 线程安全日志系统，写入 `log.txt` |
| `qml/Main.qml` | 主界面布局和按钮入口 |
| `qml/ConfigPanel.qml` | 串口参数和仿真模式配置 |
| `qml/TrendPanel.qml` | 实时趋势曲线展示 |
| `qml/AlarmPanel.qml` | CSV 导出、历史查询、统计摘要、实时报警表 |
| `qml/TelemetrySummaryPanel.qml` | 当前遥测数据显示 |
| `qml/CommunicationStatsPanel.qml` | 通信统计展示 |

## 6. 当前已实现功能

### 6.1 串口与 Modbus

- 支持仿真模式和真实串口模式
- 支持 Modbus RTU `0x03` 读保持寄存器
- 支持 Modbus RTU `0x06` 写单寄存器
- 支持 CRC 校验
- 支持异常帧处理
- 支持响应超时和重试
- 支持通信统计：TX、RX、超时、重试、CRC 错误、异常帧

当前 Modbus 寄存器映射：

| 工业地址 | 内部地址 | 数据 | 类型 |
|---|---:|---|---|
| 40001 | 0 | 温度 | Float，2 寄存器 |
| 40003 | 2 | 压力 | Float，2 寄存器 |
| 40005 | 4 | 液位 | Float，2 寄存器 |
| 40007 | 6 | 流量 | Float，2 寄存器 |
| 40009 | 8 | 报警状态 | UINT16 bit mask |

### 6.2 报警

- `AlarmEvaluator` 独立判断报警
- 报警状态使用 `quint16` bit mask
- 支持报警确认
- 新报警插入实时报警表顶部
- 报警日志写入 SQLite 的 `alarm_log` 表
- 报警日志按 bit 位插入独立记录

### 6.3 SQLite 报警日志

`alarm_log` 实际字段：

```sql
id INTEGER PRIMARY KEY AUTOINCREMENT,
timestamp TEXT NOT NULL,
variable_name TEXT NOT NULL,
alarm_value TEXT NOT NULL,
status_text TEXT NOT NULL,
acknowledged INTEGER NOT NULL,
alarm_bit INTEGER NOT NULL,
alarm_status INTEGER NOT NULL,
temperature REAL NOT NULL,
pressure REAL NOT NULL,
level REAL NOT NULL,
flow REAL NOT NULL
```

注意：当前数据库主要保存报警日志，不保存全部普通遥测采样点。如果后续要做历史曲线或遥测 CSV 导出，需要新增采样数据表和对应写入链路。

### 6.4 QThreadPool 后台任务

- `BackgroundTaskManager` 持有 `QThreadPool`
- 最大线程数按 `qBound(2, ideal - 1, 4)` 设置
- `AlarmCsvExportTask` 分页导出 `alarm_log` 到 CSV
- `AlarmHistoryQueryTask` 支持时间范围、关键字、状态、分页查询
- `AlarmStatsTask` 统计报警总数、激活/已确认数量、四类报警数量和最近报警时间
- 所有后台 SQLite 读任务都使用独立 connectionName
- 所有后台结果通过 queued signal 回到 Backend 主线程

### 6.5 趋势曲线

- `CurvePlotProvider` 继承 `QQuickItem`
- 使用 `QSGGeometryNode` 绘制折线
- 趋势数据由 Backend 管理
- QML 只负责绑定和展示

## 7. 修改代码时必须遵守的规则

1. 不要把项目改成 QWidget。
2. 不要重写整个 QML UI。
3. 不要删除现有仿真模式。
4. 不要删除现有 `ModbusProtocol`。
5. 不要让 QML 直接操作串口或数据库。
6. 不要在 UI 线程 sleep、wait 或 while 阻塞轮询。
7. 不要跨线程复用 `QSqlDatabase`。
8. 不要把 `DatabaseWorker` 的 SQLite 连接交给线程池任务。
9. 线程池任务必须在线程池线程内创建、关闭、移除自己的 SQLite 连接。
10. 不要在 `updatePaintNode()` 中访问串口、数据库、文件或网络。
11. 新增 QML 属性时，必须同步修改 Backend 的 `Q_PROPERTY`、getter 和 notify signal。
12. 修改配置项时，必须同步修改 `AppConfig`、`ConfigManager::load/save`、Backend 和 `ConfigPanel.qml`。
13. 修改跨线程 DTO 时，必须确认是否需要 `Q_DECLARE_METATYPE` 和 `qRegisterMetaType`。
14. 修改模型数据时，必须正确使用 `beginInsertRows/endInsertRows`、`beginResetModel/endResetModel`、`dataChanged` 等模型通知。
15. 每个源码或 QML 文件保持不超过 500 行；如果 Backend 继续增长，优先拆到专题 `.cpp` 文件。

## 8. Codex 修改任务流程

每次修改代码时按这个顺序执行：

1. 先阅读相关文件。
2. 说明准备修改哪些文件。
3. 保持最小改动集。
4. 修改后检查是否破坏：
   - QML 属性绑定
   - Worker 线程归属
   - 数据库线程使用规则
   - 线程池任务 SQLite 独立连接规则
   - 串口/仿真功能
   - 报警表模型通知
   - 历史查询模型通知
   - 趋势曲线刷新
5. 最后给出：
   - 修改文件清单
   - 每个文件做了什么
   - 如何编译运行
   - 如何验证功能

## 9. 基础验证清单

### 编译验证

- MSVC2022 Debug 构建通过。
- 在普通 PowerShell 中构建 MSVC 版本前，先加载 VS 环境：

```powershell
cmd /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Preview\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul && cmake --build build\Desktop_Qt_6_10_1_MSVC2022_64bit-Debug --config Debug'
```

### 启动验证

- 应用能启动。
- `Main.qml` 能加载。
- 日志文件能生成。
- 顶部状态栏能显示状态。

### 仿真模式验证

- `simulationMode=true`。
- 点击连接后进入仿真模式。
- 温度、压力、液位、流量持续变化。
- 趋势曲线持续刷新。
- 通信统计 TX/RX 增长。
- 报警触发后实时报警表插入记录。
- 报警触发后 `alarm_log` 表写入记录。

### 真实串口验证

- `simulationMode=false`。
- 端口名、波特率、校验位、数据位、停止位、从机号与从站一致。
- 能发送 Modbus `0x03` 请求。
- 能解析响应并更新遥测。
- CRC 错误、超时、异常帧能进入统计。

### 数据库验证

- `data.db` 能生成。
- 报警触发后 `alarm_log` 表能写入记录。
- 程序关闭前能 flush 剩余报警记录。
- 数据库线程退出不报错。

### 后台任务验证

- 点击“导出 CSV”后弹出保存文件对话框。
- 取消保存时不启动导出任务。
- 选择路径后 UI 不冻结，进度条更新。
- CSV 文件生成，内容包含 `alarm_log` 数据。
- 点击“刷新历史”后 UI 不冻结，历史结果表刷新。
- 点击“刷新统计”后 UI 不冻结，统计摘要刷新。
- 关闭程序时没有 SQLite connection 泄漏或线程退出报错。

### UI 验证

- 小窗口下布局不遮挡。
- 配置区可以滚动。
- 右侧内容可以滚动。
- 报警历史表和实时报警表可以滚动。
- 连接、断开、确认报警、导出 CSV、刷新历史、刷新统计按钮可见且可用。
