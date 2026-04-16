#pragma once

#include <QList>
#include <QMutex>
#include <QQuickItem>
#include <QTimer>

/**
 * @brief 高性能曲线控件。
 *
 * 不使用 QQuickPaintedItem，直接在 updatePaintNode() 中复用 QSGGeometryNode，
 * 通过折线顶点绘制高频波形，降低 CPU 到 GPU 的纹理上传开销。
 */
class CurvePlotProvider : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QList<qreal> dataPoints READ dataPoints WRITE setDataPoints NOTIFY dataPointsChanged)

public:
    explicit CurvePlotProvider(QQuickItem *parent = nullptr);

    QList<qreal> dataPoints() const;
    void setDataPoints(const QList<qreal> &points);

signals:
    void dataPointsChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private slots:
    void commitPendingData();

private:
    QList<qreal> m_dataPoints;
    QList<qreal> m_pendingPoints;
    mutable QMutex m_mutex;
    QTimer m_updateTimer;
    bool m_hasPendingPoints = false;
    int m_maxPoints = 1000;
};
