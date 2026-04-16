#pragma once

#include "communicationstats.h"
#include "configmanager.h"
#include "modbusprotocol.h"

#include <QObject>
#include <QSerialPort>
#include <QTimer>

/**
 * @brief 串口通信工作对象。
 *
 * 该对象移动到独立 QThread 后运行，负责连接/断开、周期读寄存器、写报警确认命令。
 * UI 线程只通过 queued signal/slot 与它交互，避免任何阻塞串口操作进入 QML 线程。
 */
class SerialPortWorker final : public QObject
{
    Q_OBJECT
public:
    explicit SerialPortWorker(QObject *parent = nullptr);

public slots:
    void init();
    void shutdown();
    void applyConfig(const AppConfig &config);
    void connectToDevice();
    void disconnectFromDevice();
    void acknowledgeAlarm();

signals:
    void telemetryUpdated(float temperature, float pressure, float level, float flow, quint16 alarmStatus);
    void statusMessage(const QString &message, bool error);
    void connectedChanged(bool connected);
    void communicationStatsUpdated(const CommunicationStats &stats);

private slots:
    void pollOnce();
    void handleResponseTimeout();
    void readReadyBytes();

private:
    void stopInternal();
    void openWithCurrentConfig();
    void sendRequest(const QByteArray &request);
    void processSimulatedFrame(const QByteArray &request);
    void finishSuccessfulTransaction(const ModbusProtocol::ProcessResult &result);
    void emitStats();
    void logFrame(const QString &direction, const QByteArray &frame, bool crcOk) const;
    QSerialPort::DataBits toDataBits(int value) const;
    QSerialPort::StopBits toStopBits(int value) const;
    QSerialPort::Parity toParity(const QString &value) const;

    AppConfig m_config;
    ModbusProtocol m_protocol;
    QSerialPort *m_serial = nullptr;
    QTimer *m_timer = nullptr;
    QTimer *m_responseTimer = nullptr;
    QByteArray m_rxBuffer;
    QByteArray m_pendingRequest;
    CommunicationStats m_stats;
    bool m_connected = false;
    bool m_usingRealSerial = false;
    int m_retryIndex = 0;
    int m_maxRetries = 2;
};
