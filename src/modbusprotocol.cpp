#include "modbusprotocol.h"

#include <QtEndian>
#include <QtMath>
#include <cstring>

namespace {
constexpr quint16 TemperatureAddress = 0;
constexpr quint16 PressureAddress = 2;
constexpr quint16 LevelAddress = 4;
constexpr quint16 FlowAddress = 6;
constexpr quint16 AlarmAddress = 8;
constexpr quint16 RegisterTableSize = 9;

quint16 readU16(const QByteArray &data, int offset)
{
    return qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + offset));
}

void appendU16(QByteArray *data, quint16 value)
{
    data->append(static_cast<char>((value >> 8) & 0xFF));
    data->append(static_cast<char>(value & 0xFF));
}

float readFloatRegisters(const QMap<quint16, quint16> &registers, quint16 startAddress)
{
    const quint32 bits = (quint32(registers.value(startAddress)) << 16)
                         | registers.value(startAddress + 1);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(float));
    return value;
}
}

ModbusProtocol::ModbusProtocol(quint8 slaveId)
    : m_slaveId(slaveId)
{
    for (quint16 address = 0; address < RegisterTableSize; ++address) {
        m_registers.insert(address, 0);
    }
    updateSimulationValues();
}

void ModbusProtocol::setSlaveId(quint8 slaveId)
{
    m_slaveId = qBound<quint8>(1, slaveId, 247);
}

quint8 ModbusProtocol::slaveId() const
{
    return m_slaveId;
}

void ModbusProtocol::updateSimulationValues()
{
    ++m_tick;
    const float temperature = 50.0f + 20.0f * qSin(m_tick / 18.0);
    const float pressure = 4.5f + 1.8f * qSin(m_tick / 25.0);
    const float level = 2.5f + 1.2f * qSin(m_tick / 32.0);
    const float flow = 55.0f + 25.0f * qSin(m_tick / 15.0);

    writeFloatRegisters(TemperatureAddress, qBound(0.0f, temperature, 100.0f));
    writeFloatRegisters(PressureAddress, qBound(0.0f, pressure, 10.0f));
    writeFloatRegisters(LevelAddress, qBound(0.0f, level, 5.0f));
    writeFloatRegisters(FlowAddress, qBound(0.0f, flow, 100.0f));

    m_registers[AlarmAddress] = m_alarmEvaluator.evaluate(temperature, pressure, level, flow);
}

QByteArray ModbusProtocol::buildReadRequest(quint16 startAddress, quint16 registerCount) const
{
    QByteArray frame;
    frame.append(static_cast<char>(m_slaveId));
    frame.append(static_cast<char>(0x03));
    appendU16(&frame, startAddress);
    appendU16(&frame, registerCount);
    return appendCrc(frame);
}

QByteArray ModbusProtocol::buildWriteSingleRequest(quint16 address, quint16 value) const
{
    QByteArray frame;
    frame.append(static_cast<char>(m_slaveId));
    frame.append(static_cast<char>(0x06));
    appendU16(&frame, address);
    appendU16(&frame, value);
    return appendCrc(frame);
}

ModbusProtocol::ProcessResult ModbusProtocol::processRequest(const QByteArray &request)
{
    ProcessResult result;
    if (request.size() < 4) {
        result.errorText = QStringLiteral("Modbus 帧长度过短");
        result.response = buildExceptionResponse(0x00, IllegalDataValue);
        return result;
    }

    const quint8 requestSlave = static_cast<quint8>(request.at(0));
    const quint8 functionCode = static_cast<quint8>(request.at(1));

    if (requestSlave != m_slaveId) {
        result.addressMismatch = true;
        result.errorText = QStringLiteral("从机地址不匹配");
        result.response = buildExceptionResponse(functionCode, IllegalDataAddress);
        return result;
    }

    if (!verifyCrc(request)) {
        result.crcError = true;
        result.errorText = QStringLiteral("CRC 校验失败");
        result.response = buildExceptionResponse(functionCode, SlaveDeviceFailure);
        return result;
    }

    if (functionCode == 0x03) {
        if (request.size() != 8) {
            result.response = buildExceptionResponse(functionCode, IllegalDataValue);
            result.errorText = QStringLiteral("读保持寄存器请求长度非法");
            return result;
        }
        const quint16 startAddress = readU16(request, 2);
        const quint16 registerCount = readU16(request, 4);
        if (!isReadableRange(startAddress, registerCount)) {
            result.response = buildExceptionResponse(functionCode, IllegalDataAddress);
            result.errorText = QStringLiteral("读保持寄存器地址越界");
            return result;
        }
        updateSimulationValues();
        result.response = buildReadResponse(startAddress, registerCount);
        result.ok = true;
        result.valuesUpdated = true;
    } else if (functionCode == 0x06) {
        if (request.size() != 8) {
            result.response = buildExceptionResponse(functionCode, IllegalDataValue);
            result.errorText = QStringLiteral("写单寄存器请求长度非法");
            return result;
        }
        const quint16 address = readU16(request, 2);
        const quint16 value = readU16(request, 4);
        if (address != AlarmAddress || value != 0xFF00) {
            result.response = buildExceptionResponse(functionCode, IllegalDataAddress);
            result.errorText = QStringLiteral("仅允许向 40009 写入 0xFF00 确认报警");
            return result;
        }
        m_alarmEvaluator.acknowledge(0xFFFF);
        m_registers[AlarmAddress] = m_alarmEvaluator.visibleMask();
        result.response = buildWriteSingleResponse(address, value);
        result.ok = true;
        result.valuesUpdated = true;
    } else {
        result.response = buildExceptionResponse(functionCode, IllegalFunction);
        result.errorText = QStringLiteral("非法功能码: 0x%1").arg(functionCode, 2, 16, QLatin1Char('0'));
        return result;
    }

    result.temperature = readFloatRegisters(m_registers, TemperatureAddress);
    result.pressure = readFloatRegisters(m_registers, PressureAddress);
    result.level = readFloatRegisters(m_registers, LevelAddress);
    result.flow = readFloatRegisters(m_registers, FlowAddress);
    result.alarmStatus = m_registers.value(AlarmAddress);
    return result;
}

ModbusProtocol::ProcessResult ModbusProtocol::processResponse(const QByteArray &response) const
{
    ProcessResult result;
    if (response.size() < 5) {
        result.errorText = QStringLiteral("Modbus 响应帧长度过短");
        return result;
    }
    if (static_cast<quint8>(response.at(0)) != m_slaveId) {
        result.addressMismatch = true;
        result.errorText = QStringLiteral("响应从机地址不匹配");
        return result;
    }
    if (!verifyCrc(response)) {
        result.crcError = true;
        result.errorText = QStringLiteral("响应 CRC 校验失败");
        return result;
    }

    const quint8 functionCode = static_cast<quint8>(response.at(1));
    if ((functionCode & 0x80) != 0) {
        result.errorText = QStringLiteral("设备返回 Modbus 异常码: 0x%1")
                               .arg(static_cast<quint8>(response.at(2)), 2, 16, QLatin1Char('0'));
        return result;
    }

    if (functionCode == 0x03) {
        const int byteCount = static_cast<quint8>(response.at(2));
        if (response.size() != byteCount + 5 || byteCount < 18) {
            result.errorText = QStringLiteral("读保持寄存器响应长度非法");
            return result;
        }

        QMap<quint16, quint16> registers;
        for (int i = 0; i < byteCount / 2; ++i) {
            registers.insert(static_cast<quint16>(i), readU16(response, 3 + i * 2));
        }
        result.temperature = readFloatRegisters(registers, TemperatureAddress);
        result.pressure = readFloatRegisters(registers, PressureAddress);
        result.level = readFloatRegisters(registers, LevelAddress);
        result.flow = readFloatRegisters(registers, FlowAddress);
        result.alarmStatus = registers.value(AlarmAddress);
        result.ok = true;
        result.valuesUpdated = true;
        return result;
    }

    if (functionCode == 0x06) {
        result.ok = response.size() == 8;
        if (!result.ok) {
            result.errorText = QStringLiteral("写单寄存器响应长度非法");
        }
        return result;
    }

    result.errorText = QStringLiteral("非法响应功能码: 0x%1").arg(functionCode, 2, 16, QLatin1Char('0'));
    return result;
}

quint16 ModbusProtocol::crc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (const char byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x0001) != 0) {
                crc = static_cast<quint16>((crc >> 1) ^ 0xA001);
            } else {
                crc = static_cast<quint16>(crc >> 1);
            }
        }
    }
    return crc;
}

bool ModbusProtocol::verifyCrc(const QByteArray &frame)
{
    if (frame.size() < 4) {
        return false;
    }
    const QByteArray body = frame.left(frame.size() - 2);
    const quint16 expected = crc16(body);
    const quint8 crcLow = static_cast<quint8>(frame.at(frame.size() - 2));
    const quint8 crcHigh = static_cast<quint8>(frame.at(frame.size() - 1));
    const quint16 actual = quint16(crcLow) | (quint16(crcHigh) << 8);
    return expected == actual;
}

QByteArray ModbusProtocol::appendCrc(const QByteArray &frameWithoutCrc)
{
    QByteArray frame = frameWithoutCrc;
    const quint16 crc = crc16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    return frame;
}

QByteArray ModbusProtocol::buildReadResponse(quint16 startAddress, quint16 registerCount) const
{
    QByteArray frame;
    frame.append(static_cast<char>(m_slaveId));
    frame.append(static_cast<char>(0x03));
    frame.append(static_cast<char>(registerCount * 2));
    for (quint16 address = startAddress; address < startAddress + registerCount; ++address) {
        appendU16(&frame, m_registers.value(address));
    }
    return appendCrc(frame);
}

QByteArray ModbusProtocol::buildWriteSingleResponse(quint16 address, quint16 value) const
{
    QByteArray frame;
    frame.append(static_cast<char>(m_slaveId));
    frame.append(static_cast<char>(0x06));
    appendU16(&frame, address);
    appendU16(&frame, value);
    return appendCrc(frame);
}

QByteArray ModbusProtocol::buildExceptionResponse(quint8 functionCode, ExceptionCode code) const
{
    QByteArray frame;
    frame.append(static_cast<char>(m_slaveId));
    frame.append(static_cast<char>(functionCode | 0x80));
    frame.append(static_cast<char>(code));
    return appendCrc(frame);
}

bool ModbusProtocol::isReadableRange(quint16 startAddress, quint16 registerCount) const
{
    if (registerCount == 0) {
        return false;
    }
    const quint16 endAddress = static_cast<quint16>(startAddress + registerCount);
    return startAddress < RegisterTableSize && endAddress <= RegisterTableSize;
}

void ModbusProtocol::writeFloatRegisters(quint16 startAddress, float value)
{
    quint32 bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float must be 32-bit");
    std::memcpy(&bits, &value, sizeof(float));
    m_registers[startAddress] = static_cast<quint16>((bits >> 16) & 0xFFFF);
    m_registers[startAddress + 1] = static_cast<quint16>(bits & 0xFFFF);
}
