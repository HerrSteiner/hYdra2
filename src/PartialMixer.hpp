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

#include "HydraDocument.hpp"

#include <QtCore/QVariantList>
#include <QtQuick/QQuickItem>

namespace hydra2 {

// Interaction/controller item for the Qt Graphs partial mixer. Rendering is
// done by GraphsView/BarSeries in QML; this item only provides normalized data
// and retains the existing multi-selection + linked-drag behaviour.
class PartialMixer : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(HydraDocument* document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(QVariantList values READ values NOTIFY valuesChanged)
    Q_PROPERTY(QVariantList selectedBars READ selectedBars NOTIFY selectionValuesChanged)

public:
    explicit PartialMixer(QQuickItem* parent = nullptr);

    HydraDocument* document() const { return document_; }
    void setDocument(HydraDocument* document);

    QVariantList values() const;
    QVariantList selectedBars() const;

signals:
    void documentChanged();
    void valuesChanged();
    void selectionValuesChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRectF innerRect() const;
    int partialAt(qreal x) const;
    void editAt(const QPointF& pos);
    void rebuildAverageCache();
    void refreshAverageCache(const QVector<int>& partials);
    double maximumAverage() const;
    double naturalHeight(int partial, double maxAverage) const;

    HydraDocument* document_ = nullptr;
    int draggingPartial_ = -1;
    QPointF pressPos_;
    bool levelDragStarted_ = false;
    QVector<int> dragPartials_;
    QVector<double> dragStartGains_;
    QVector<double> dragNaturalHeights_;
    QVector<double> averageAmplitudes_;
    double maximumAverage_ = 1.0;
};

} // namespace hydra2
