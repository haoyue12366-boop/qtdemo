#include "alarmevaluator.h"

quint16 AlarmEvaluator::evaluate(float temperature, float pressure, float level, float flow)
{
    updateHighAlarm(TemperatureHigh, temperature, 68.0f, 66.0f);
    updateHighAlarm(PressureHigh, pressure, 6.0f, 5.7f);
    updateHighAlarm(FlowHigh, flow, 78.0f, 74.0f);
    updateLowAlarm(LevelLow, level, 1.4f, 1.7f);
    m_acknowledgedMask &= m_activeMask;
    return visibleMask();
}

void AlarmEvaluator::acknowledge(quint16 mask)
{
    m_acknowledgedMask |= (m_activeMask & mask);
}

quint16 AlarmEvaluator::activeMask() const
{
    return m_activeMask;
}

quint16 AlarmEvaluator::acknowledgedMask() const
{
    return m_acknowledgedMask;
}

quint16 AlarmEvaluator::visibleMask() const
{
    return m_activeMask & ~m_acknowledgedMask;
}

void AlarmEvaluator::updateHighAlarm(quint16 bit, float value, float trigger, float recover)
{
    if ((m_activeMask & bit) == 0) {
        if (value > trigger) {
            m_activeMask |= bit;
        }
        return;
    }

    if (value < recover) {
        m_activeMask &= ~bit;
        m_acknowledgedMask &= ~bit;
    }
}

void AlarmEvaluator::updateLowAlarm(quint16 bit, float value, float trigger, float recover)
{
    if ((m_activeMask & bit) == 0) {
        if (value < trigger) {
            m_activeMask |= bit;
        }
        return;
    }

    if (value > recover) {
        m_activeMask &= ~bit;
        m_acknowledgedMask &= ~bit;
    }
}
