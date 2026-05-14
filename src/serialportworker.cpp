#include "serialportworker.h"

#include "logger.h"

#include <QThread>

SerialPortWorker::SerialPortWorker(QObject *parent)
    : QObject(parent)
    , m_protocol(1)
{
}

void SerialPortWorker::applyConfig(const AppConfig &config)
{
    m_config = config;
    m_protocol.setSlaveId(static_cast<quint8>(config.slaveId));
    m_stats.simulationMode = config.simulationMode;
    if (m_timer != nullptr) {
        m_timer->setInterval(m_config.refreshInterval);
    }
    if (m_connected) {
        Logger::info(QStringLiteral("设备已连接，配置变更后执行安全重连"));
        stopInternal();
        openWithCurrentConfig();
    }
    Logger::info(QStringLiteral("通信线程配置已生效: %1, %2 bps, SlaveID=%3, Refresh=%4ms")
                     .arg(m_config.portName)
                     .arg(m_config.baudRate)
                     .arg(m_config.slaveId)
                     .arg(m_config.refreshInterval));
    emitStats();
}

void SerialPortWorker::init()//创建子线程才初始化，不经过构造函数
{
    Logger::info(QStringLiteral("通信子线程 ID: 0x%1")
                     .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16));
    if (m_serial == nullptr) {
        m_serial = new QSerialPort(this);
        connect(m_serial, &QSerialPort::readyRead,
                this, &SerialPortWorker::readReadyBytes,
                Qt::QueuedConnection);
    }
    if (m_timer == nullptr) {
        m_timer = new QTimer(this);
        m_timer->setTimerType(Qt::PreciseTimer);
        connect(m_timer, &QTimer::timeout,
                this, &SerialPortWorker::pollOnce,
                Qt::QueuedConnection);
    }
    if (m_responseTimer == nullptr) {
        m_responseTimer = new QTimer(this);
        m_responseTimer->setSingleShot(true);
        m_responseTimer->setInterval(300);
        connect(m_responseTimer, &QTimer::timeout,
                this, &SerialPortWorker::handleResponseTimeout,
                Qt::QueuedConnection);
    }
}

void SerialPortWorker::shutdown()
{
    Logger::info(QStringLiteral("通信线程准备关闭"));
    stopInternal();
}

void SerialPortWorker::connectToDevice()
{
    if (m_serial == nullptr || m_timer == nullptr) {
        emit statusMessage(QStringLiteral("通信线程未初始化"), true);
        return;
    }
    if (m_connected) {
        emit statusMessage(QStringLiteral("设备已连接"), false);
        return;
    }
    openWithCurrentConfig();
}

void SerialPortWorker::openWithCurrentConfig()
{
    QString errorMessage;
    if (!validateConfig(&errorMessage)) {
        Logger::error(errorMessage);
        emit statusMessage(errorMessage, true);
        return;
    }

    m_serial->setPortName(m_config.portName);
    m_serial->setBaudRate(m_config.baudRate);
    m_serial->setDataBits(toDataBits(m_config.dataBits));
    m_serial->setStopBits(toStopBits(m_config.stopBits));
    m_serial->setParity(toParity(m_config.parity));
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    m_usingRealSerial = !m_config.simulationMode && m_serial->open(QIODevice::ReadWrite);
    if (!m_usingRealSerial) {
        const QString message = m_config.simulationMode
                                    ? QStringLiteral("已进入内置 Modbus RTU 仿真模式")
                                    : QStringLiteral("串口打开失败: %1。已进入内置 Modbus RTU 仿真模式。")
                                          .arg(m_serial->errorString());
        if (m_config.simulationMode) {
            Logger::info(message);
            emit statusMessage(message, false);
        } else {
            Logger::error(message);
            emit statusMessage(message, true);
        }
        m_stats.simulationMode = true;
    } else {
        Logger::info(QStringLiteral("串口已打开: %1").arg(m_config.portName));
        emit statusMessage(QStringLiteral("串口已连接"), false);
        m_stats.simulationMode = false;
    }

    m_connected = true;
    m_timer->start(m_config.refreshInterval);
    emitStats();
    emit connectedChanged(true);
}

void SerialPortWorker::disconnectFromDevice()
{
    stopInternal();
}

void SerialPortWorker::acknowledgeAlarm()
{
    if (!m_connected) {
        emit statusMessage(QStringLiteral("请先连接设备"), true);
        return;
    }
    if (m_usingRealSerial && !m_pendingRequest.isEmpty()) {
        emit statusMessage(QStringLiteral("当前有未完成通信，请稍后再确认报警"), true);
        return;
    }
    sendRequest(m_protocol.buildWriteSingleRequest(8, 0xFF00));
}

void SerialPortWorker::pollOnce()
{
    if (!m_connected) {
        return;
    }
    if (m_usingRealSerial && !m_pendingRequest.isEmpty()) {
        return;
    }
    sendRequest(m_protocol.buildReadRequest(0, 9));
}

void SerialPortWorker::handleResponseTimeout()
{
    ++m_stats.timeoutCount;
    if (m_retryIndex < m_maxRetries && !m_pendingRequest.isEmpty()) {
        ++m_retryIndex;
        ++m_stats.retryCount;
        Logger::warning(QStringLiteral("Modbus 响应超时，执行第 %1 次重试").arg(m_retryIndex));
        const QByteArray retryFrame = m_pendingRequest;
        sendRequest(retryFrame);
        return;
    }

    emitStats();
    emit statusMessage(QStringLiteral("Modbus 响应超时，重试失败"), true);
    m_pendingRequest.clear();
    m_retryIndex = 0;
}

void SerialPortWorker::readReadyBytes()
{
    if (m_serial == nullptr) {
        return;
    }
    m_rxBuffer.append(m_serial->readAll());
    if (m_rxBuffer.size() < 5) {
        return;
    }
    const quint8 functionCode = static_cast<quint8>(m_rxBuffer.at(1));
    int expectedSize = 0;
    if (functionCode == 0x03 && m_rxBuffer.size() >= 3) {
        expectedSize = static_cast<quint8>(m_rxBuffer.at(2)) + 5;
    } else if (functionCode == 0x06 || (functionCode & 0x80) != 0) {
        expectedSize = (functionCode & 0x80) != 0 ? 5 : 8;
    }
    if (expectedSize == 0 || m_rxBuffer.size() < expectedSize) {
        return;
    }
    const QByteArray frame = m_rxBuffer.left(expectedSize);
    m_rxBuffer.remove(0, expectedSize);
    const bool crcOk = ModbusProtocol::verifyCrc(frame);
    logFrame(QStringLiteral("RX(real)"), frame, crcOk);
    if (!crcOk) {
        if (m_responseTimer != nullptr) {
            m_responseTimer->stop();
        }
        ++m_stats.crcErrorCount;
        m_pendingRequest.clear();
        m_retryIndex = 0;
        Logger::error(QStringLiteral("响应 CRC 校验失败，原始字节=%1")
                          .arg(QString::fromLatin1(frame.toHex(' ').toUpper())));
        emitStats();
        emit statusMessage(QStringLiteral("响应 CRC 校验失败"), true);
        return;
    }
    if (m_responseTimer != nullptr) {
        m_responseTimer->stop();
    }
    ++m_stats.rxCount;
    m_pendingRequest.clear();
    m_retryIndex = 0;
    const ModbusProtocol::ProcessResult result = m_protocol.processResponse(frame);
    if (!result.ok) {
        ++m_stats.exceptionCount;
        emitStats();
        emit statusMessage(result.errorText, true);
        return;
    }
    finishSuccessfulTransaction(result);
}

void SerialPortWorker::stopInternal()
{
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    if (m_serial != nullptr && m_serial->isOpen()) {
        m_serial->close();
    }
    if (m_responseTimer != nullptr) {
        m_responseTimer->stop();
    }
    m_rxBuffer.clear();
    m_pendingRequest.clear();
    m_retryIndex = 0;
    if (m_connected) {
        Logger::info(QStringLiteral("设备已断开"));
        emit connectedChanged(false);
        emit statusMessage(QStringLiteral("设备已断开"), false);
    }
    m_usingRealSerial = false;
    m_connected = false;
}

void SerialPortWorker::sendRequest(const QByteArray &request)
{
    logFrame(QStringLiteral("TX"), request, ModbusProtocol::verifyCrc(request));
    ++m_stats.txCount;
    if (m_usingRealSerial && m_serial != nullptr && m_serial->isOpen()) {
        const qint64 written = m_serial->write(request);
        if (written < 0) {
            ++m_stats.exceptionCount;
            const QString message = QStringLiteral("串口写入失败: %1").arg(m_serial->errorString());
            Logger::error(message);
            emitStats();
            emit statusMessage(message, true);
            return;
        }
        if (written != request.size()) {
            ++m_stats.exceptionCount;
            const QString message = QStringLiteral("串口写入字节数异常: %1/%2").arg(written).arg(request.size());
            Logger::error(message);
            emitStats();
            emit statusMessage(message, true);
            return;
        }
        m_pendingRequest = request;
        if (m_responseTimer != nullptr) {
            m_responseTimer->start();
        }
        emitStats();
        return;
    }

    processSimulatedFrame(request);
}

void SerialPortWorker::processSimulatedFrame(const QByteArray &request)
{
    const ModbusProtocol::ProcessResult result = m_protocol.processRequest(request);
    if (!result.response.isEmpty()) {
        logFrame(QStringLiteral("RX(sim)"), result.response, ModbusProtocol::verifyCrc(result.response));
        ++m_stats.rxCount;
    }
    if (!result.ok) {
        ++m_stats.exceptionCount;
        if (result.crcError) {
            ++m_stats.crcErrorCount;
        }
        emitStats();
        Logger::error(QStringLiteral("Modbus 异常: %1, 原始帧=%2")
                          .arg(result.errorText, QString::fromLatin1(request.toHex(' ').toUpper())));
        emit statusMessage(result.errorText, true);
        return;
    }

    finishSuccessfulTransaction(result);
}

void SerialPortWorker::finishSuccessfulTransaction(const ModbusProtocol::ProcessResult &result)
{
    if (result.valuesUpdated) {
        emit telemetryUpdated(result.temperature, result.pressure, result.level, result.flow, result.alarmStatus);
    }
    emitStats();
}

void SerialPortWorker::emitStats()
{
    emit communicationStatsUpdated(m_stats);
}

void SerialPortWorker::logFrame(const QString &direction, const QByteArray &frame, bool crcOk) const
{
    const quint8 slave = frame.isEmpty() ? 0 : static_cast<quint8>(frame.at(0));
    const quint8 function = frame.size() > 1 ? static_cast<quint8>(frame.at(1)) : 0;
    Logger::bytes(QStringLiteral("%1 Slave=%2 Func=0x%3 CRC=%4")
                      .arg(direction)
                      .arg(slave)
                      .arg(function, 2, 16, QLatin1Char('0'))
                      .arg(crcOk ? QStringLiteral("OK") : QStringLiteral("BAD")),
                  frame);
}

bool SerialPortWorker::validateConfig(QString *errorMessage) const
{
    if (m_config.portName.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("串口参数非法：端口号不能为空");
        }
        return false;
    }
    if (m_config.slaveId < 1 || m_config.slaveId > 247) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("串口参数非法：从机地址必须在 1 到 247 之间");
        }
        return false;
    }
    if (m_config.dataBits < 5 || m_config.dataBits > 8) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("串口参数非法：数据位仅支持 5/6/7/8");
        }
        return false;
    }
    if (m_config.stopBits != 1 && m_config.stopBits != 2) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("串口参数非法：停止位仅支持 1 或 2");
        }
        return false;
    }
    if (m_config.parity != QStringLiteral("None")
        && m_config.parity != QStringLiteral("Even")
        && m_config.parity != QStringLiteral("Odd")) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("串口参数非法：校验位仅支持 None/Even/Odd");
        }
        return false;
    }
    if (m_config.refreshInterval <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("串口参数非法：刷新周期必须大于 0");
        }
        return false;
    }
    return true;
}

QSerialPort::DataBits SerialPortWorker::toDataBits(int value) const
{
    switch (value) {
    case 5:
        return QSerialPort::Data5;
    case 6:
        return QSerialPort::Data6;
    case 7:
        return QSerialPort::Data7;
    default:
        return QSerialPort::Data8;
    }
}

QSerialPort::StopBits SerialPortWorker::toStopBits(int value) const
{
    return value == 2 ? QSerialPort::TwoStop : QSerialPort::OneStop;
}

QSerialPort::Parity SerialPortWorker::toParity(const QString &value) const
{
    if (value.compare(QStringLiteral("Even"), Qt::CaseInsensitive) == 0) {
        return QSerialPort::EvenParity;
    }
    if (value.compare(QStringLiteral("Odd"), Qt::CaseInsensitive) == 0) {
        return QSerialPort::OddParity;
    }
    return QSerialPort::NoParity;
}
