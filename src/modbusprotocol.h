#pragma once

#include "alarmevaluator.h"

#include <QByteArray>
#include <QMap>
#include <QObject>

/**
 * @brief Modbus RTU 协议工具与模拟寄存器表。
 *
 * 保持寄存器地址映射：
 * 40001 温度 Float(2寄存器) ℃ 只读 范围 0~100℃
 * 40003 压力 Float(2寄存器) MPa 只读 范围 0~10 MPa
 * 40005 液位 Float(2寄存器) m 只读 范围 0~5 m
 * 40007 流量 Float(2寄存器) m3/h 只读 范围 0~100 m3/h
 * 40009 报警状态 UINT16 - 读写 写 0xFF00 确认报警
 *
 * 代码内部使用 Modbus PDU 起始地址 0 表示 40001，因此：
 * 40001 -> 0, 40003 -> 2, 40005 -> 4, 40007 -> 6, 40009 -> 8。
 */
class ModbusProtocol final
{
public:
    enum ExceptionCode : quint8 {
        IllegalFunction = 0x01,
        IllegalDataAddress = 0x02,
        IllegalDataValue = 0x03,
        SlaveDeviceFailure = 0x04
    };

    struct ProcessResult {
        bool ok = false;
        bool crcError = false;
        bool addressMismatch = false;
        bool valuesUpdated = false;
        QByteArray response;
        QString errorText;
        float temperature = 0.0f;
        float pressure = 0.0f;
        float level = 0.0f;
        float flow = 0.0f;
        quint16 alarmStatus = 0;
    };

    explicit ModbusProtocol(quint8 slaveId = 1);

    void setSlaveId(quint8 slaveId);
    quint8 slaveId() const;

    void updateSimulationValues();
    QByteArray buildReadRequest(quint16 startAddress, quint16 registerCount) const;
    QByteArray buildWriteSingleRequest(quint16 address, quint16 value) const;
    ProcessResult processRequest(const QByteArray &request);
    ProcessResult processResponse(const QByteArray &response) const;

    static quint16 crc16(const QByteArray &data);
    static bool verifyCrc(const QByteArray &frame);
    static QByteArray appendCrc(const QByteArray &frameWithoutCrc);

private:
    QByteArray buildReadResponse(quint16 startAddress, quint16 registerCount) const;
    QByteArray buildWriteSingleResponse(quint16 address, quint16 value) const;
    QByteArray buildExceptionResponse(quint8 functionCode, ExceptionCode code) const;
    bool isReadableRange(quint16 startAddress, quint16 registerCount) const;
    void writeFloatRegisters(quint16 startAddress, float value);

    quint8 m_slaveId = 1;
    QMap<quint16, quint16> m_registers;
    AlarmEvaluator m_alarmEvaluator;
    int m_tick = 0;
};
