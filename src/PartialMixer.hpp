/*
Copyright (C) 2026 Malte Steiner

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
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
    QVector<int> dragPartials_;
    QVector<double> dragStartGains_;
    QVector<double> dragNaturalHeights_;
};

} // namespace hydra2
