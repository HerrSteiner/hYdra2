/*
Copyright (C) 2026 Malte Steiner

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#pragma once

#include "HydraDocument.hpp"

#include <QtCore/QRectF>
#include <QtQuick/QQuickItem>

class QSGNode;
class QMouseEvent;
class QWheelEvent;

namespace hydra2 {

class BreakpointEditor : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(HydraDocument* document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool logicSnap READ logicSnap WRITE setLogicSnap NOTIFY logicSnapChanged)
    Q_PROPERTY(bool focusOnSelected READ focusOnSelected WRITE setFocusOnSelected NOTIFY focusOnSelectedChanged)

    Q_PROPERTY(double viewTimeMinimum READ viewTimeMinimum NOTIFY viewChanged)
    Q_PROPERTY(double viewTimeMaximum READ viewTimeMaximum NOTIFY viewChanged)
    Q_PROPERTY(double viewValueMinimum READ viewValueMinimum NOTIFY viewChanged)
    Q_PROPERTY(double viewValueMaximum READ viewValueMaximum NOTIFY viewChanged)

    Q_PROPERTY(QString activeLabel READ activeLabel NOTIFY overlayChanged)
    Q_PROPERTY(qreal activeLabelX READ activeLabelX NOTIFY overlayChanged)
    Q_PROPERTY(qreal activeLabelY READ activeLabelY NOTIFY overlayChanged)

public:
    explicit BreakpointEditor(QQuickItem* parent = nullptr);

    HydraDocument* document() const { return document_; }
    void setDocument(HydraDocument* document);

    int mode() const { return static_cast<int>(mode_); }
    void setMode(int mode);

    bool logicSnap() const { return logicSnap_; }
    void setLogicSnap(bool enabled);

    bool focusOnSelected() const { return focusOnSelected_; }
    void setFocusOnSelected(bool enabled);

    double viewTimeMinimum() const { return viewTimeMin_; }
    double viewTimeMaximum() const { return viewTimeMax_; }
    double viewValueMinimum() const { return viewValueMin_; }
    double viewValueMaximum() const { return viewValueMax_; }

    QString activeLabel() const;
    qreal activeLabelX() const;
    qreal activeLabelY() const;

    Q_INVOKABLE void fitView();

signals:
    void documentChanged();
    void modeChanged();
    void logicSnapChanged();
    void focusOnSelectedChanged();
    void viewChanged();
    void overlayChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QRectF plotRect() const;
    double dataMaximumY() const;
    QPointF toScreen(int partial, const Breakpoint& point) const;
    double screenToTime(qreal x) const;
    double screenToValue(qreal y) const;
    PointRef hitTestPoint(const QPointF& pos) const;
    int hitTestStroke(const QPointF& pos) const;
    QVector<PointRef> refsInside(const QRectF& rect) const;
    bool focusActive() const;
    void updateAfterDocumentChange();
    void updateOverlay();
    void panByPixels(const QPointF& delta);
    void clampView();

    HydraDocument* document_ = nullptr;
    TrackKind mode_ = TrackKind::Amplitude;
    bool logicSnap_ = true;
    bool focusOnSelected_ = false;

    bool draggingPoints_ = false;
    bool selectingRect_ = false;
    bool panning_ = false;
    bool viewFitted_ = true;

    QPointF lastMousePos_;
    QPointF selectionStart_;
    QRectF selectionRect_;
    PointRef activePoint_;

    double viewTimeMin_ = 0.0;
    double viewTimeMax_ = 1.0;
    double viewValueMin_ = 0.0;
    double viewValueMax_ = 32767.0;
};

} // namespace hydra2
