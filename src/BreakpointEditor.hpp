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

#include <QtQuick/QQuickPaintedItem>

namespace hydra2 {

class BreakpointEditor : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(HydraDocument* document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    explicit BreakpointEditor(QQuickItem* parent = nullptr);

    HydraDocument* document() const { return document_; }
    void setDocument(HydraDocument* document);

    int mode() const { return static_cast<int>(mode_); }
    void setMode(int mode);

    void paint(QPainter* painter) override;

signals:
    void documentChanged();
    void modeChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRectF plotRect() const;
    double maxY() const;
    QPointF toScreen(int partial, const Breakpoint& point,
                     double durationMs, double yMaximum) const;
    PointRef hitTest(const QPointF& pos) const;
    QVector<PointRef> refsInside(const QRectF& rect) const;
    QString activeLabel() const;

    HydraDocument* document_ = nullptr;
    TrackKind mode_ = TrackKind::Amplitude;
    bool draggingPoints_ = false;
    bool lassoing_ = false;
    QPointF lastMousePos_;
    QPointF lassoStart_;
    QRectF lassoRect_;
    PointRef activePoint_;
};

} // namespace hydra2
