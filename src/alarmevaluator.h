#pragma once

#include <QtGlobal>

/**
 * 工业报警 bit 位与回差判断器。
 *
 * 40009 报警状态使用 bit 位表达多个报警：
 * bit0 温度高高报警：触发 > 68.0 C，恢复 < 66.0 C
 * bit1 压力高高报警：触发 > 6.0 MPa，恢复 < 5.7 MPa
 * bit2 流量高高报警：触发 > 78.0 m3/h，恢复 < 74.0 m3/h
 * bit3 液位低低报警：触发 < 1.4 m，恢复 > 1.7 m
 *
 * 回差避免过程量在阈值附近抖动时频繁触发/恢复。
 */
class AlarmEvaluator final
{
public:
    enum AlarmBit : quint16 {//bitmask 报警表达法
        TemperatureHigh = 0x0001,
        PressureHigh = 0x0002,
        FlowHigh = 0x0004,
        LevelLow = 0x0008
    };

    quint16 evaluate(float temperature, float pressure, float level, float flow);
    void acknowledge(quint16 mask);
    quint16 activeMask() const;
    quint16 acknowledgedMask() const;
    quint16 visibleMask() const;

private:
    void updateHighAlarm(quint16 bit, float value, float trigger, float recover);
    void updateLowAlarm(quint16 bit, float value, float trigger, float recover);

    quint16 m_activeMask = 0;//已经激活的报警
    quint16 m_acknowledgedMask = 0;//已经确认的报警，m_activeMask&~m_acknowledgedMask得到激活但是未确认的报警
};
