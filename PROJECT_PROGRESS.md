# qthmi 当前进度摘要

项目路径：`d:\qtdemo\qthmi`

项目类型：Qt 6.10.1 + C++17 + QML 工业上位机仿真监控 Demo

构建方式：CMake

已验证：Release 构建通过

## 已实现功能

1. QML + C++ 前后端分离
   - QML 负责 UI 展示和事件转发
   - Backend 负责业务汇聚
   - main.cpp 中通过 `setContextProperty("backend", backend)` 注入 QML

2. 通信线程
   - `SerialPortWorker + QThread`
   - `QSerialPort` / `QTimer` 在线程内 `init()` 创建
   - 支持仿真模式和真实串口模式
   - 支持 Modbus RTU `0x03` 读保持寄存器
   - 支持 Modbus RTU `0x06` 写单寄存器
   - 支持 CRC 校验
   - 支持异常帧处理
   - 支持响应超时和最多 2 次重试
   - 支持通信统计：TX、RX、超时、重试、CRC 错误、异常

3. 数据库线程
   - `DatabaseWorker + QThread`
   - SQLite 连接在数据库线程内创建、写入、关闭
   - 采样数据写入 `data.db`
   - 数据库错误写入 `log.txt` 并通知 UI

4. 配置持久化
   - `ConfigManager + QSettings`
   - 保存到 `config.ini`
   - 参数包括端口号、波特率、数据位、停止位、校验位、从机号、刷新周期、仿真模式

5. 日志系统
   - `Logger` 单例
   - `log.txt` 自动保存到可执行文件同目录
   - 记录启动、配置、线程、串口、Modbus TX/RX、CRC、超时、重试、SQLite 异常等

6. Modbus 寄存器表
   - 40001 温度 Float，2 寄存器
   - 40003 压力 Float，2 寄存器
   - 40005 液位 Float，2 寄存器
   - 40007 流量 Float，2 寄存器
   - 40009 报警状态 UINT16 bit mask

7. 报警逻辑
   - `AlarmEvaluator` 独立模块
   - 40009 使用 bit mask
   - bit0 温度高高：`>68.0 C` 触发，`<66.0 C` 恢复
   - bit1 压力高高：`>6.0 MPa` 触发，`<5.7 MPa` 恢复
   - bit2 流量高高：`>78.0 m3/h` 触发，`<74.0 m3/h` 恢复
   - bit3 液位低低：`<1.4 m` 触发，`>1.7 m` 恢复
   - 支持报警确认
   - 报警日志按 bit 位插入独立记录

8. 报警表格
   - `AlarmLogModel` 继承 `QAbstractTableModel`
   - 预生成 50,000 条模拟报警数据
   - 新报警插入表格顶部
   - 确认报警后状态变为已确认
   - QML 表格自动回到顶部显示新报警

9. 实时波形
   - `CurvePlotProvider` 继承 `QQuickItem`
   - 使用 `QSGGeometryNode` 绘制折线
   - 约 30 FPS 节流刷新
   - QML 中叠加坐标轴、网格
   - 显示温度 Min / Avg / Max

10. UI
    - 左侧配置区有中文标签和说明
    - 右侧支持窗口变小时滚动
    - 顶部状态栏显示连接状态、从机号、运行时间、最近错误
    - 右侧有实时仪表、通信统计、波形图、报警表格

## 主要源码文件

- `CMakeLists.txt`
- `qml/Main.qml`
- `src/main.cpp`
- `src/backend.h/.cpp`
- `src/serialportworker.h/.cpp`
- `src/modbusprotocol.h/.cpp`
- `src/alarmevaluator.h/.cpp`
- `src/communicationstats.h`
- `src/databaseworker.h/.cpp`
- `src/alarmlogmodel.h/.cpp`
- `src/curveplotprovider.h/.cpp`
- `src/configmanager.h/.cpp`
- `src/logger.h/.cpp`
- `src/telemetryrecord.h`

## 当前可继续优化方向

1. 增加日志导出 / 清空 / 打开日志目录功能
2. 增加通信日志窗口
3. 增加报警筛选、报警统计、未确认数量
4. 增加阈值配置界面，把报警阈值从代码移到配置文件
5. 增加 CSV 导出
6. 增加单元测试：CRC、Modbus、AlarmEvaluator、ConfigManager
7. 增加真实 Modbus Slave 工具联调说明
