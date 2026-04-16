# qthmi 工业组态仿真监控系统

## 项目简介

qthmi 是一个基于 Qt 6.10.1、C++17 与 QML 的工业上位机 Demo。项目展示多线程串口通信、Modbus RTU 协议解析、配置持久化、专用数据库线程 SQLite 采样入库、50,000 行报警 Model/View，以及基于 `QQuickItem + QSGGeometryNode` 的高性能实时曲线。

默认串口为 `COM3`。如果本机没有对应串口，程序会记录串口打开失败，并进入内置 Modbus RTU 仿真模式，便于无硬件环境下直接运行和查看 UI。

## 技术栈

- Qt 6.10.1: Core、Quick、SerialPort、Sql、Concurrent
- CMake 3.20+
- C++17
- QML Controls
- QSettings INI 配置
- SQLite

## 快速开始

```bash
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.1/msvc2022_64
cmake --build . --config Release
```

首次运行会在可执行文件同目录生成：

- `config.ini`: 串口、站号、刷新周期配置
- `log.txt`: 运行日志与 Modbus 十六进制收发日志
- `data.db`: SQLite 采样数据库

## 核心特性

- UI 线程无阻塞串口操作，`SerialPortWorker` 在独立 `SerialThread` 中工作。
- UI 线程无 SQLite 写入，`DatabaseWorker` 在独立 `DbThread` 中创建、打开、写入并关闭数据库连接。
- Worker 采用两段式初始化：`moveToThread()` 后通过 queued 调用 `init()`，线程专属资源在线程内创建。
- 应用关闭采用 `shutdown -> quit -> wait`，不使用 `QThread::terminate()` 作为常规退出手段。
- `Backend` 通过 `QQmlContext::setContextProperty("backend", backend)` 注入 QML。
- `Backend` 在 `main.cpp` 中使用 `new` 创建，父对象为 `QQmlApplicationEngine`。
- Modbus RTU 支持功能码 `0x03` 读保持寄存器、`0x06` 写单寄存器。
- 支持 CRC 错误、非法功能码、从机地址不匹配等异常帧处理。
- 支持仿真模式开关，真实串口不可用时可在通信线程内生成 Modbus RTU 响应。
- 支持通信统计：TX、RX、响应超时、重试、CRC 错误、异常帧计数。
- 支持响应超时和最多 2 次自动重试。
- 40009 报警状态采用 bit 位表达多个报警，并带回差，避免阈值附近抖动。
- `AlarmLogModel` 继承 `QAbstractTableModel`，预生成 50,000 条报警记录。
- `CurvePlotProvider` 继承 `QQuickItem`，复用 `QSGGeometryNode` 绘制折线波形，并以约 30 FPS 合并刷新；QML 叠加坐标轴、网格和 min/avg/max 统计。

## Modbus 寄存器地址表

| 地址 | 内部地址 | 变量名 | 类型 | 单位 | 读写 | 说明 |
| --- | --- | --- | --- | --- | --- | --- |
| 40001 | 0 | 温度 | Float, 2 寄存器 | C | 只读 | 0-100 C |
| 40003 | 2 | 压力 | Float, 2 寄存器 | MPa | 只读 | 0-10 MPa |
| 40005 | 4 | 液位 | Float, 2 寄存器 | m | 只读 | 0-5 m |
| 40007 | 6 | 流量 | Float, 2 寄存器 | m3/h | 只读 | 0-100 m3/h |
| 40009 | 8 | 报警状态 | UINT16 bit mask | - | 读写 | 写 0xFF00 确认当前报警 |

## 报警 bit 位与回差

| bit | 报警 | 触发条件 | 恢复条件 |
| --- | --- | --- | --- |
| bit0 | 温度高高 | 温度 > 68.0 C | 温度 < 66.0 C |
| bit1 | 压力高高 | 压力 > 6.0 MPa | 压力 < 5.7 MPa |
| bit2 | 流量高高 | 流量 > 78.0 m3/h | 流量 < 74.0 m3/h |
| bit3 | 液位低低 | 液位 < 1.4 m | 液位 > 1.7 m |

报警判断由 `AlarmEvaluator` 负责。`ModbusProtocol` 只负责把判断结果写入 40009，避免协议解析和业务规则耦合。

## 文件结构

```text
qthmi/
  CMakeLists.txt
  README.md
  qml/
    Main.qml
  src/
    main.cpp
    backend.h/.cpp
    serialportworker.h/.cpp
    communicationstats.h
    databaseworker.h/.cpp
    telemetryrecord.h
    alarmevaluator.h/.cpp
    modbusprotocol.h/.cpp
    alarmlogmodel.h/.cpp
    alarmrecord.h
    curveplotprovider.h/.cpp
    configmanager.h/.cpp
    logger.h/.cpp
```

## 模块架构图

```text
┌─────────────────────────────────────────────┐
│                  QML UI 层                   │
│  仪表盘、波形图、TableView、连接按钮          │
└─────────────────────┬───────────────────────┘
                      │ 属性绑定 / 槽调用
┌─────────────────────▼───────────────────────┐
│              Backend C++ 业务逻辑            │
│  数据汇聚、配置管理、日志提示、线程调度        │
└───┬───────────────┬───────────────┬─────────┘
    │               │               │
┌───▼───┐     ┌─────▼─────┐   ┌─────▼─────┐
│Serial │     │AlarmLog   │   │CurvePlot  │
│Worker │     │Model      │   │Provider   │
│子线程 │     │主线程     │   │渲染线程   │
└───┬───┘     └───────────┘   └───────────┘
    │
┌───▼───────────┐
│ QSerialPort   │
│ Modbus RTU    │
└───────────────┘

┌─────────────────────┐
│ DatabaseWorker      │
│ DbThread            │
│ SQLite 独立连接      │
└─────────────────────┘
```

## 核心类图

- `Backend`: 连接通信线程、数据库线程、Model、配置和日志，向 QML 暴露属性与 `connectToDevice()`、`disconnectFromDevice()`、`saveConfig()`、`acknowledgeAlarm()`、`shutdown()`。
- `SerialPortWorker`: 在独立线程执行 `init()`、串口打开、周期轮询、Modbus 读写命令、异常帧处理和 `shutdown()`。
- `AlarmEvaluator`: 负责报警 bit 位、阈值和回差判断。
- `CommunicationStats`: 线程间传递的通信统计快照。
- `DatabaseWorker`: 在独立线程内创建 SQLite 连接、建表、插入采样数据、捕获 `QSqlError` 并关闭连接。
- `ModbusProtocol`: 维护模拟保持寄存器表，生成 `0x03`、`0x06` 响应帧与异常响应帧，计算 CRC16。
- `AlarmLogModel`: 继承 `QAbstractTableModel`，重写 `rowCount()`、`columnCount()`、`data()`、`headerData()`、`roleNames()`。
- `CurvePlotProvider`: 继承 `QQuickItem`，重写 `updatePaintNode()`，使用 `QSGGeometryNode` 和 `DrawLineStrip` 绘制曲线，使用 GUI 线程定时器做 30 FPS 刷新节流。
- `Logger`: 单例日志器，提供 `debug()`、`info()`、`warning()`、`error()`、`bytes()`，并通过 `qInstallMessageHandler` 捕获 Qt 日志。
- `ConfigManager`: 使用 `QSettings` 读写 `config.ini`。

## 运行截图说明

主界面左侧为串口与设备参数配置，右侧上方为温度、压力、液位、流量实时仪表，中部为温度波形，下方为报警日志表格，底部状态栏显示连接、异常和数据库状态。
