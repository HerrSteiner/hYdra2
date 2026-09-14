#pragma once

#include <QtCore/QPointF>

#include "HydraDocument.hpp"

#include <QtQuick/QQuickPaintedItem>

namespace hydra2 {

class PartialMixer : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(HydraDocument* document READ document WRITE setDocument NOTIFY documentChanged)

public:
    explicit PartialMixer(QQuickItem* parent = nullptr);

    HydraDocument* document() const { return document_; }
    void setDocument(HydraDocument* document);
    void paint(QPainter* painter) override;

signals:
    void documentChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRectF innerRect() const;
    int partialAt(qreal x) const;
    void editAt(const QPointF& pos, Qt::KeyboardModifiers modifiers, bool select);

    HydraDocument* document_ = nullptr;
    int draggingPartial_ = -1;
    QPointF pressPos_;
    bool levelDragStarted_ = false;
};

} // namespace hydra2
