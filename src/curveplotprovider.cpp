#include "curveplotprovider.h"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <algorithm>

CurvePlotProvider::CurvePlotProvider(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
    m_updateTimer.setInterval(33);
    m_updateTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_updateTimer, &QTimer::timeout,
            this, &CurvePlotProvider::commitPendingData,
            Qt::QueuedConnection);
}

QList<qreal> CurvePlotProvider::dataPoints() const
{
    QMutexLocker locker(&m_mutex);
    return m_dataPoints;
}

void CurvePlotProvider::setDataPoints(const QList<qreal> &points)
{
    {
        QMutexLocker locker(&m_mutex);
        m_pendingPoints = points;
        if (m_pendingPoints.size() > m_maxPoints) {
            m_pendingPoints = m_pendingPoints.mid(m_pendingPoints.size() - m_maxPoints);
        }
        m_hasPendingPoints = true;
    }
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start();
    }
}

void CurvePlotProvider::commitPendingData()
{
    {
        QMutexLocker locker(&m_mutex);
        if (!m_hasPendingPoints) {
            m_updateTimer.stop();
            return;
        }
        m_dataPoints = m_pendingPoints;
        m_hasPendingPoints = false;
    }
    emit dataPointsChanged();
    update();
}

QSGNode *CurvePlotProvider::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    // updatePaintNode() 运行在 Qt Quick 场景图同步/渲染阶段。threaded render loop 下
    // GUI 线程会在同步阶段被阻塞，读取 width()/height() 这类 QQuickItem 基本属性是安全的；
    // 但波形点缓冲仍用互斥锁保护，避免 GUI 线程 setDataPoints() 与渲染阶段读数据竞争。
    QSGGeometryNode *node = static_cast<QSGGeometryNode *>(oldNode);

    QList<qreal> points;
    {
        QMutexLocker locker(&m_mutex);
        points = m_dataPoints;
    }

    const int pointCount = qMin(points.size(), m_maxPoints);
    if (pointCount < 2) {
        return node;
    }

    if (node == nullptr) {
        node = new QSGGeometryNode;
        QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), pointCount);
        geometry->setDrawingMode(QSGGeometry::DrawLineStrip);
        geometry->setLineWidth(2.0f);
        geometry->setVertexDataPattern(QSGGeometry::StreamPattern);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);

        QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
        material->setColor(QColor(0, 255, 157));
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    }

    QSGGeometry *geometry = node->geometry();
    if (geometry->vertexCount() != pointCount) {
        geometry->allocate(pointCount);
    }

    QSGGeometry::Point2D *vertices = geometry->vertexDataAsPoint2D();
    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    const float margin = 20.0f;
    const float plotW = qMax(1.0f, w - 2.0f * margin);
    const float plotH = qMax(1.0f, h - 2.0f * margin);

    const auto begin = points.cbegin();
    const auto end = points.cbegin() + pointCount;
    const auto minMax = std::minmax_element(begin, end);
    const qreal minVal = *minMax.first;
    const qreal maxVal = *minMax.second;
    const qreal range = qFuzzyCompare(minVal, maxVal) ? 1.0 : (maxVal - minVal);

    for (int i = 0; i < pointCount; ++i) {
        const float x = margin + (static_cast<float>(i) * plotW) / static_cast<float>(pointCount - 1);
        const float normalized = static_cast<float>((points.at(i) - minVal) / range);
        const float y = margin + plotH - (normalized * plotH);
        vertices[i].set(x, y);
    }

    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
