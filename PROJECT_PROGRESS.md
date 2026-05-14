# qthmi 当前进度摘要

项目路径：`D:\qt_demo\qthmi`

项目类型：Qt 6.10.1 + C++17 + QML 工业上位机仿真监控 Demo

构建方式：CMake

最近验证：MSVC2022 Debug 构建通过。PowerShell 中需要先加载 VS 环境：

```powershell
cmd /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Preview\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul && cmake --build build\Desktop_Qt_6_10_1_MSVC2022_64bit-Debug --config Debug'
```

## 已实现功能

1. QML + C++ 前后端分离
   - QML 负责 UI 展示和事件转发
   - Backend 负责业务汇聚
   - `main.cpp` 中通过 `setContextProperty("backend", backend)` 注入 QML

2. 通信线程
   - `SerialPortWorker + QThread`
   - `QSerialPort` / `QTimer` 在线程内 `init()` 创建
   - 支持仿真模式和真实串口模式
   - 支持 Modbus RTU `0x03` 读保持寄存器
   - 支持 Modbus RTU `0x06` 写单寄存器
   - 支持 CRC 校验、异常帧处理、响应超时和最多 2 次重试
   - 支持通信统计：TX、RX、超时、重试、CRC 错误、异常

3. 数据库线程
   - `DatabaseWorker + QThread`
   - SQLite 连接在数据库线程内创建、写入、关闭
   - 报警日志写入可执行文件同目录 `data.db` 的 `alarm_log` 表
   - 报警日志采用线程内队列 + 200ms 定时批量 flush
   - 当前普通遥测采样数据尚未持续落库
   - 数据库错误写入 `log.txt` 并通知 UI

4. QThreadPool 后台任务
   - 新增 `BackgroundTaskManager` 统一管理 `QThreadPool`
   - 线程池只用于短生命周期后台任务，不替代串口线程或数据库写入线程
   - 支持报警日志 CSV 异步导出
   - 支持报警历史异步查询
   - 支持报警统计异步分析
   - 每个 SQLite 后台读任务独立创建唯一连接，并在线程池线程内关闭和 `removeDatabase`
   - 后台任务通过 queued signal 回到 Backend，再更新属性或模型

5. 报警日志 CSV 导出
   - `AlarmCsvExportTask` 分页读取 `alarm_log`
   - QML 使用 `FileDialog` 让用户选择保存路径，不使用固定路径
   - CSV 使用 UTF-8
   - 支持字段转义：逗号、换行、双引号
   - 空表也会导出表头
   - 导出进度和结果显示到 UI

6. 报警历史查询
   - `AlarmHistoryQueryTask` 在线程池中查询 SQLite
   - 支持时间范围、关键字、状态、分页
   - 查询结果写入 `AlarmHistoryModel`
   - QML 中有历史查询筛选区和历史结果表

7. 报警统计分析
   - `AlarmStatsTask` 在线程池中统计 SQLite
   - 统计总报警数、激活数、已确认数、温度/压力/液位/流量报警数、最近报警时间
   - QML 中显示统计摘要

8. 配置持久化
   - `ConfigManager + QSettings`
   - 保存到 `config.ini`
   - 参数包括端口号、波特率、数据位、停止位、校验位、从机号、刷新周期、仿真模式

9. 日志系统
   - `Logger` 单例
   - `log.txt` 自动保存到可执行文件同目录
   - 记录启动、配置、线程、串口、Modbus、CRC、超时、重试、SQLite 和后台任务异常

10. Modbus 寄存器表
    - 40001 温度 Float，2 寄存器
    - 40003 压力 Float，2 寄存器
    - 40005 液位 Float，2 寄存器
    - 40007 流量 Float，2 寄存器
    - 40009 报警状态 UINT16 bit mask

11. 报警逻辑
    - `AlarmEvaluator` 独立模块
    - 40009 使用 bit mask
    - bit0 温度高高：`>68.0 C` 触发，`<66.0 C` 恢复
    - bit1 压力高高：`>6.0 MPa` 触发，`<5.7 MPa` 恢复
    - bit2 流量高高：`>78.0 m3/h` 触发，`<74.0 m3/h` 恢复
    - bit3 液位低低：`<1.4 m` 触发，`>1.7 m` 恢复
    - 支持报警确认
    - 报警日志按 bit 位插入独立记录

12. 报警表格
    - `AlarmLogModel` 继承 `QAbstractTableModel`
    - 预生成 50,000 条模拟报警数据
    - 新报警插入实时报警表顶部
    - 确认报警后状态变为已确认
    - QML 表格自动回到顶部显示新报警

13. 实时波形
    - `CurvePlotProvider` 继承 `QQuickItem`
    - 使用 `QSGGeometryNode` 绘制折线
    - 约 30 FPS 节流刷新
    - QML 中叠加坐标轴、网格
    - 显示温度 Min / Avg / Max

14. UI
    - 左侧配置区有中文标签和说明
    - 右侧支持窗口变小时滚动
    - 顶部状态栏显示连接状态、从机号、运行时间、最近错误
    - 右侧有实时仪表、通信统计、波形图、报警功能区
    - 报警功能区包含 CSV 导出、历史查询、统计摘要、实时报警表

## alarm_log 实际表结构

`DatabaseWorker::ensureTable()` 当前创建的字段：

- `id INTEGER PRIMARY KEY AUTOINCREMENT`
- `timestamp TEXT NOT NULL`
- `variable_name TEXT NOT NULL`
- `alarm_value TEXT NOT NULL`
- `status_text TEXT NOT NULL`
- `acknowledged INTEGER NOT NULL`
- `alarm_bit INTEGER NOT NULL`
- `alarm_status INTEGER NOT NULL`
- `temperature REAL NOT NULL`
- `pressure REAL NOT NULL`
- `level REAL NOT NULL`
- `flow REAL NOT NULL`

后续查询、导出、统计必须以这些实际字段为准。

## 主要源码文件

- `CMakeLists.txt`
- `qml/Main.qml`
- `qml/AlarmPanel.qml`
- `src/main.cpp`
- `src/backend.h/.cpp`
- `src/backend_backgroundtasks.cpp`
- `src/backgroundtaskmanager.h/.cpp`
- `src/backgroundtasktypes.h`
- `src/tasks/tasksqlconnection.h`
- `src/tasks/alarmcsvexporttask.h/.cpp`
- `src/tasks/alarmhistoryquerytask.h/.cpp`
- `src/tasks/alarmstatstask.h/.cpp`
- `src/alarmhistorymodel.h/.cpp`
- `src/serialportworker.h/.cpp`
- `src/modbusprotocol.h/.cpp`
- `src/alarmevaluator.h/.cpp`
- `src/communicationstats.h`
- `src/databaseworker.h/.cpp`
- `src/alarmlogmodel.h/.cpp`
- `src/curveplotprovider.h/.cpp`
- `src/configmanager.h/.cpp`
- `src/logger.h/.cpp`
- `src/alarmrecord.h`
- `src/alarmrecorddata.h`
- `src/telemetryrecord.h`

## 当前开发约束

- 不把项目改成 QWidget
- 不重写整个 QML UI
- 不删除仿真模式
- 不删除现有 `ModbusProtocol`
- 串口通信继续使用 `SerialPortWorker + QThread`
- 报警日志写入继续使用 `DatabaseWorker + QThread`
- `QThreadPool` 只用于短生命周期后台任务
- 后台任务不得直接操作 QML、Backend 属性、`QQuickItem` 或 Model
- 后台 SQLite 任务不得复用 `DatabaseWorker` 的 `QSqlDatabase`
- 每个源码/QML 文件保持不超过 500 行；当前最大源码文件是 `src/backend.cpp`，约 460 行
