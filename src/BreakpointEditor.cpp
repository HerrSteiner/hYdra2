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

#include "BreakpointEditor.hpp"

#include <QtGui/QColor>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtQuick/QSGGeometry>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGVertexColorMaterial>

#include <algorithm>
#include <cmath>
#include <utility>

namespace hydra2 {
namespace {

constexpr qreal kLeft = 58.0;
constexpr qreal kTop = 18.0;
constexpr qreal kRight = 16.0;
constexpr qreal kBottom = 38.0;
constexpr qreal kHitRadius = 8.0;
constexpr qreal kStrokeHitRadius = 5.0;

struct RenderVertex {
    float x = 0.0f;
    float y = 0.0f;
    QColor color;
};

bool validRef(const PointRef& ref)
{
    return ref.partial >= 0 && ref.id != 0;
}

double pointToSegmentDistanceSquared(const QPointF& point,
                                     const QPointF& a,
                                     const QPointF& b)
{
    const QPointF ab = b - a;
    const QPointF ap = point - a;
    const double lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();
    if (lengthSquared <= 1.0e-12) {
        const QPointF d = point - a;
        return d.x() * d.x() + d.y() * d.y();
    }

    const double t = std::clamp(
        (ap.x() * ab.x() + ap.y() * ab.y()) / lengthSquared,
        0.0, 1.0);
    const QPointF nearest = a + ab * t;
    const QPointF d = point - nearest;
    return d.x() * d.x() + d.y() * d.y();
}

void addRect(QVector<RenderVertex>& out, const QRectF& rect, const QColor& color)
{
    const float l = static_cast<float>(rect.left());
    const float r = static_cast<float>(rect.right());
    const float t = static_cast<float>(rect.top());
    const float b = static_cast<float>(rect.bottom());

    out.push_back({l, t, color});
    out.push_back({r, t, color});
    out.push_back({l, b, color});
    out.push_back({l, b, color});
    out.push_back({r, t, color});
    out.push_back({r, b, color});
}

void addLine(QVector<RenderVertex>& out,
             const QPointF& a,
             const QPointF& b,
             qreal width,
             const QColor& color)
{
    const QPointF d = b - a;
    const double len = std::hypot(d.x(), d.y());
    if (len < 1.0e-6)
        return;

    const double half = width * 0.5;
    const QPointF n(-d.y() / len * half, d.x() / len * half);
    const QPointF a1 = a + n;
    const QPointF a2 = a - n;
    const QPointF b1 = b + n;
    const QPointF b2 = b - n;

    out.push_back({static_cast<float>(a1.x()), static_cast<float>(a1.y()), color});
    out.push_back({static_cast<float>(a2.x()), static_cast<float>(a2.y()), color});
    out.push_back({static_cast<float>(b1.x()), static_cast<float>(b1.y()), color});
    out.push_back({static_cast<float>(b1.x()), static_cast<float>(b1.y()), color});
    out.push_back({static_cast<float>(a2.x()), static_cast<float>(a2.y()), color});
    out.push_back({static_cast<float>(b2.x()), static_cast<float>(b2.y()), color});
}

bool clipLineToRect(QPointF& a, QPointF& b, const QRectF& r)
{
    double x0 = a.x(), y0 = a.y(), x1 = b.x(), y1 = b.y();
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    double u1 = 0.0, u2 = 1.0;

    auto clip = [&](double p, double q) {
        if (std::abs(p) < 1.0e-12)
            return q >= 0.0;
        const double t = q / p;
        if (p < 0.0) {
            if (t > u2) return false;
            if (t > u1) u1 = t;
        } else {
            if (t < u1) return false;
            if (t < u2) u2 = t;
        }
        return true;
    };

    if (!clip(-dx, x0 - r.left()) ||
        !clip( dx, r.right() - x0) ||
        !clip(-dy, y0 - r.top()) ||
        !clip( dy, r.bottom() - y0))
        return false;

    a = QPointF(x0 + u1 * dx, y0 + u1 * dy);
    b = QPointF(x0 + u2 * dx, y0 + u2 * dy);
    return true;
}

void setVertex(QSGGeometry::ColoredPoint2D& target, const RenderVertex& source)
{
    const int alpha = source.color.alpha();
    const auto premultiply = [alpha](int c) -> uchar {
        return static_cast<uchar>((c * alpha + 127) / 255);
    };
    target.set(source.x, source.y,
               premultiply(source.color.red()),
               premultiply(source.color.green()),
               premultiply(source.color.blue()),
               static_cast<uchar>(alpha));
}

} // namespace

BreakpointEditor::BreakpointEditor(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton);
}

void BreakpointEditor::setDocument(HydraDocument* document)
{
    if (document_ == document)
        return;

    if (document_)
        disconnect(document_, nullptr, this, nullptr);

    document_ = document;
    if (document_) {
        connect(document_, &HydraDocument::documentReset, this, [this] {
            activePoint_ = {};
            fitView();
        });
        connect(document_, &HydraDocument::dataChanged, this, [this] {
            updateAfterDocumentChange();
        });
        connect(document_, &HydraDocument::selectionChanged, this, [this] {
            update();
            updateOverlay();
        });
    }

    activePoint_ = {};
    fitView();
    emit documentChanged();
}

void BreakpointEditor::setMode(int mode)
{
    const TrackKind next = mode == 1 ? TrackKind::Frequency : TrackKind::Amplitude;
    if (mode_ == next)
        return;

    mode_ = next;
    activePoint_ = {};
    fitView();
    emit modeChanged();
}

void BreakpointEditor::setLogicSnap(bool enabled)
{
    if (logicSnap_ == enabled)
        return;
    logicSnap_ = enabled;
    emit logicSnapChanged();
}

void BreakpointEditor::setFocusOnSelected(bool enabled)
{
    if (focusOnSelected_ == enabled)
        return;
    focusOnSelected_ = enabled;
    emit focusOnSelectedChanged();
    update();
}

QRectF BreakpointEditor::plotRect() const
{
    return QRectF(kLeft, kTop,
                  std::max<qreal>(1.0, width() - kLeft - kRight),
                  std::max<qreal>(1.0, height() - kTop - kBottom));
}

double BreakpointEditor::dataMaximumY() const
{
    if (!document_ || mode_ == TrackKind::Amplitude)
        return 32767.0;

    int maximum = 1000;
    for (int partial = 0; partial < document_->partialCount(); ++partial) {
        for (const auto& point : document_->track(partial, TrackKind::Frequency))
            maximum = std::max(maximum, point.value);
    }
    return std::min(32767, std::max(1000, ((maximum + 999) / 1000) * 1000));
}

bool BreakpointEditor::focusActive() const
{
    return focusOnSelected_ && document_ && !document_->selectedPartials().isEmpty();
}

QPointF BreakpointEditor::toScreen(int partial, const Breakpoint& point) const
{
    const QRectF r = plotRect();
    const double tSpan = std::max(1.0e-9, viewTimeMax_ - viewTimeMin_);
    const double vSpan = std::max(1.0e-9, viewValueMax_ - viewValueMin_);
    const double value = document_
        ? document_->displayValue(partial, mode_, point)
        : static_cast<double>(point.value);

    const double x = r.left() + (point.timeMs - viewTimeMin_) / tSpan * r.width();
    const double y = r.bottom() - (value - viewValueMin_) / vSpan * r.height();
    return {x, y};
}

double BreakpointEditor::screenToTime(qreal x) const
{
    const QRectF r = plotRect();
    const double f = (x - r.left()) / std::max<qreal>(1.0, r.width());
    return viewTimeMin_ + f * (viewTimeMax_ - viewTimeMin_);
}

double BreakpointEditor::screenToValue(qreal y) const
{
    const QRectF r = plotRect();
    const double f = (r.bottom() - y) / std::max<qreal>(1.0, r.height());
    return viewValueMin_ + f * (viewValueMax_ - viewValueMin_);
}

void BreakpointEditor::fitView()
{
    const double duration = std::max(1, document_ ? document_->durationMs() : 1);
    viewTimeMin_ = 0.0;
    viewTimeMax_ = duration;
    viewValueMin_ = 0.0;
    viewValueMax_ = dataMaximumY();
    viewFitted_ = true;
    emit viewChanged();
    update();
    updateOverlay();
}

void BreakpointEditor::clampView()
{
    const double fullTime = std::max(1.0, document_ ? static_cast<double>(document_->durationMs()) : 1.0);
    const double fullValue = std::max(1.0, dataMaximumY());

    double tSpan = std::clamp(viewTimeMax_ - viewTimeMin_, 1.0, fullTime);
    double vSpan = std::clamp(viewValueMax_ - viewValueMin_, 1.0, fullValue);

    viewTimeMin_ = std::clamp(viewTimeMin_, 0.0, std::max(0.0, fullTime - tSpan));
    viewTimeMax_ = viewTimeMin_ + tSpan;
    viewValueMin_ = std::clamp(viewValueMin_, 0.0, std::max(0.0, fullValue - vSpan));
    viewValueMax_ = viewValueMin_ + vSpan;
}

void BreakpointEditor::updateAfterDocumentChange()
{
    if (viewFitted_) {
        const double duration = std::max(1, document_ ? document_->durationMs() : 1);
        viewTimeMax_ = duration;
        viewValueMax_ = dataMaximumY();
        emit viewChanged();
    } else {
        clampView();
        emit viewChanged();
    }
    update();
    updateOverlay();
}

QString BreakpointEditor::activeLabel() const
{
    if (!document_ || !validRef(activePoint_))
        return {};

    const Breakpoint* point = document_->point(activePoint_);
    if (!point)
        return {};

    const int display = static_cast<int>(std::lround(
        document_->displayValue(activePoint_.partial, activePoint_.kind, *point)));
    return mode_ == TrackKind::Amplitude
        ? QStringLiteral("P%1   %2 ms   amp %3")
              .arg(activePoint_.partial + 1).arg(point->timeMs).arg(display)
        : QStringLiteral("P%1   %2 ms   %3 Hz")
              .arg(activePoint_.partial + 1).arg(point->timeMs).arg(display);
}

qreal BreakpointEditor::activeLabelX() const
{
    if (!document_ || !validRef(activePoint_))
        return 0.0;
    if (const Breakpoint* point = document_->point(activePoint_))
        return toScreen(activePoint_.partial, *point).x();
    return 0.0;
}

qreal BreakpointEditor::activeLabelY() const
{
    if (!document_ || !validRef(activePoint_))
        return 0.0;
    if (const Breakpoint* point = document_->point(activePoint_))
        return toScreen(activePoint_.partial, *point).y();
    return 0.0;
}

void BreakpointEditor::updateOverlay()
{
    emit overlayChanged();
}

QSGNode* BreakpointEditor::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    QSGGeometry* geometry = nullptr;

    if (!node) {
        node = new QSGGeometryNode;
        geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::StreamPattern);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);

        auto* material = new QSGVertexColorMaterial;
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    } else {
        geometry = node->geometry();
    }

    QVector<RenderVertex> vertices;
    const QRectF bounds = boundingRect();
    const QRectF r = plotRect();

    addRect(vertices, bounds, QColor(QStringLiteral("#181a1d")));
    addRect(vertices, r, QColor(QStringLiteral("#202328")));

    const QColor gridColor(255, 255, 255, 28);
    for (int i = 0; i <= 10; ++i) {
        const qreal x = r.left() + r.width() * i / 10.0;
        addLine(vertices, {x, r.top()}, {x, r.bottom()}, 1.0, gridColor);
    }
    for (int i = 0; i <= 8; ++i) {
        const qreal y = r.top() + r.height() * i / 8.0;
        addLine(vertices, {r.left(), y}, {r.right(), y}, 1.0, gridColor);
    }

    if (document_ && document_->loaded()) {
        qsizetype pointCount = 0;
        for (int p = 0; p < document_->partialCount(); ++p)
            pointCount += document_->track(p, mode_).size();
        vertices.reserve(vertices.size() + static_cast<int>(pointCount * 12));

        const bool focused = focusActive();
        const auto& selectedPartials = document_->selectedPartials();

        // Unselected first, selected last, so selected curves stay visually on top.
        QVector<int> order;
        order.reserve(document_->partialCount());
        for (int p = 0; p < document_->partialCount(); ++p)
            if (!selectedPartials.contains(p))
                order.push_back(p);
        for (int p = 0; p < document_->partialCount(); ++p)
            if (selectedPartials.contains(p))
                order.push_back(p);

        for (int partial : std::as_const(order)) {
            const bool selectedPartial = selectedPartials.contains(partial);
            const auto& points = document_->track(partial, mode_);
            if (points.isEmpty())
                continue;

            const QColor lineColor = selectedPartial
                ? QColor(78, 181, 255, 245)
                : (focused ? QColor(190, 198, 208, 24)
                           : QColor(190, 198, 208, 72));
            const qreal lineWidth = selectedPartial ? 2.0 : 1.0;

            QPointF previous = toScreen(partial, points.first());
            for (qsizetype i = 1; i < points.size(); ++i) {
                QPointF current = toScreen(partial, points.at(i));
                QPointF a = previous;
                QPointF b = current;
                if (clipLineToRect(a, b, r))
                    addLine(vertices, a, b, lineWidth, lineColor);
                previous = current;
            }

            const bool showHandles = !focused || selectedPartial;
            if (!showHandles)
                continue;

            for (const auto& point : points) {
                const QPointF pos = toScreen(partial, point);
                if (!r.contains(pos))
                    continue;

                const PointRef ref{partial, mode_, point.id};
                const bool selectedPoint = document_->isPointSelected(ref);
                const qreal half = selectedPoint ? 5.0 : (selectedPartial ? 3.6 : 2.4);
                const QColor fill = selectedPoint
                    ? QColor(255, 214, 64)
                    : (selectedPartial ? QColor(78, 181, 255, 220)
                                       : QColor(150, 158,170,140));
                addRect(vertices, QRectF(pos.x() - half, pos.y() - half,
                                         half * 2.0, half * 2.0), fill);
            }
        }
    }

    if (selectingRect_) {
        const QRectF box = selectionRect_.normalized().intersected(r);
        if (!box.isEmpty()) {
            addRect(vertices, box, QColor(90, 180, 255, 35));
            const QColor border(120, 200, 255, 230);
            addLine(vertices, box.topLeft(), box.topRight(), 1.0, border);
            addLine(vertices, box.topRight(), box.bottomRight(), 1.0, border);
            addLine(vertices, box.bottomRight(), box.bottomLeft(), 1.0, border);
            addLine(vertices, box.bottomLeft(), box.topLeft(), 1.0, border);
        }
    }

    geometry->allocate(static_cast<int>(vertices.size()));
    auto* target = geometry->vertexDataAsColoredPoint2D();
    for (qsizetype i = 0; i < vertices.size(); ++i)
        setVertex(target[i], vertices.at(i));

    geometry->markVertexDataDirty();
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}

PointRef BreakpointEditor::hitTestPoint(const QPointF& pos) const
{
    if (!document_)
        return {};

    const QRectF r = plotRect();
    const double time = screenToTime(pos.x());
    const double timeRadius = kHitRadius * (viewTimeMax_ - viewTimeMin_)
                            / std::max<qreal>(1.0, r.width());
    const bool focused = focusActive();

    PointRef best;
    double bestDistance = kHitRadius * kHitRadius;
    bool bestSelectedPartial = false;

    for (int partial = 0; partial < document_->partialCount(); ++partial) {
        const bool selectedPartial = document_->selectedPartials().contains(partial);
        if (focused && !selectedPartial)
            continue;

        const auto& points = document_->track(partial, mode_);
        if (points.isEmpty())
            continue;

        auto it = std::lower_bound(points.cbegin(), points.cend(), time - timeRadius,
                                   [](const Breakpoint& p, double t) {
                                       return p.timeMs < t;
                                   });
        for (; it != points.cend() && it->timeMs <= time + timeRadius; ++it) {
            const QPointF screen = toScreen(partial, *it);
            const QPointF d = screen - pos;
            const double d2 = d.x() * d.x() + d.y() * d.y();
            if (d2 > bestDistance)
                continue;

            if (d2 < bestDistance - 0.01 || (selectedPartial && !bestSelectedPartial)) {
                bestDistance = d2;
                bestSelectedPartial = selectedPartial;
                best = {partial, mode_, it->id};
            }
        }
    }
    return best;
}

int BreakpointEditor::hitTestStroke(const QPointF& pos) const
{
    if (!document_)
        return -1;

    const double time = screenToTime(pos.x());
    int bestPartial = -1;
    double bestDistance = kStrokeHitRadius * kStrokeHitRadius;
    bool bestSelected = false;

    for (int partial = 0; partial < document_->partialCount(); ++partial) {
        const auto& points = document_->track(partial, mode_);
        if (points.size() < 2)
            continue;

        auto it = std::lower_bound(points.cbegin(), points.cend(), time,
                                   [](const Breakpoint& p, double t) {
                                       return p.timeMs < t;
                                   });
        const qsizetype idx = it - points.cbegin();

        const qsizetype from = std::max<qsizetype>(0, idx - 2);
        const qsizetype to = std::min<qsizetype>(points.size() - 1, idx + 1);
        for (qsizetype i = from; i < to; ++i) {
            const QPointF a = toScreen(partial, points.at(i));
            const QPointF b = toScreen(partial, points.at(i + 1));
            const double d2 = pointToSegmentDistanceSquared(pos, a, b);
            const bool selected = document_->selectedPartials().contains(partial);
            if (d2 <= bestDistance &&
                (d2 < bestDistance - 0.01 || (selected && !bestSelected))) {
                bestDistance = d2;
                bestPartial = partial;
                bestSelected = selected;
            }
        }
    }
    return bestPartial;
}

QVector<PointRef> BreakpointEditor::refsInside(const QRectF& rect) const
{
    QVector<PointRef> refs;
    if (!document_)
        return refs;

    const QRectF box = rect.normalized();
    const bool focused = focusActive();

    for (int partial = 0; partial < document_->partialCount(); ++partial) {
        if (focused && !document_->selectedPartials().contains(partial))
            continue;
        for (const auto& point : document_->track(partial, mode_)) {
            if (box.contains(toScreen(partial, point)))
                refs.push_back({partial, mode_, point.id});
        }
    }
    return refs;
}

void BreakpointEditor::panByPixels(const QPointF& delta)
{
    const QRectF r = plotRect();
    const double tSpan = viewTimeMax_ - viewTimeMin_;
    const double vSpan = viewValueMax_ - viewValueMin_;

    const double dt = -delta.x() * tSpan / std::max<qreal>(1.0, r.width());
    const double dv =  delta.y() * vSpan / std::max<qreal>(1.0, r.height());

    viewTimeMin_ += dt;
    viewTimeMax_ += dt;
    viewValueMin_ += dv;
    viewValueMax_ += dv;
    viewFitted_ = false;
    clampView();
    emit viewChanged();
    update();
    updateOverlay();
}

void BreakpointEditor::mousePressEvent(QMouseEvent* event)
{
    if (!document_ || !document_->loaded() || !plotRect().contains(event->position())) {
        event->ignore();
        return;
    }

    lastMousePos_ = event->position();

    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton &&
         event->modifiers().testFlag(Qt::AltModifier))) {
        panning_ = true;
        draggingPoints_ = false;
        selectingRect_ = false;
        event->accept();
        return;
    }

    const bool additive = event->modifiers().testFlag(Qt::ShiftModifier)
                       || event->modifiers().testFlag(Qt::ControlModifier)
                       || event->modifiers().testFlag(Qt::MetaModifier);
    const bool toggle = event->modifiers().testFlag(Qt::ControlModifier)
                     || event->modifiers().testFlag(Qt::MetaModifier);

    const PointRef hit = hitTestPoint(event->position());
    if (validRef(hit)) {
        activePoint_ = hit;
        if (!document_->isPointSelected(hit) || toggle)
            document_->selectPoint(hit, additive, toggle);
        draggingPoints_ = document_->isPointSelected(hit);
        selectingRect_ = false;
    } else {
        const int partial = hitTestStroke(event->position());
        if (partial >= 0) {
            document_->selectWholePartial(partial, additive, toggle);
            activePoint_ = {};
            draggingPoints_ = document_->selectedPartials().contains(partial);
            selectingRect_ = false;
        } else {
            if (!additive)
                document_->clearSelection();
            activePoint_ = {};
            draggingPoints_ = false;
            selectingRect_ = true;
            selectionStart_ = event->position();
            selectionRect_ = QRectF(selectionStart_, selectionStart_);
        }
    }

    update();
    updateOverlay();
    event->accept();
}

void BreakpointEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (!document_)
        return;

    if (panning_) {
        const QPointF delta = event->position() - lastMousePos_;
        panByPixels(delta);
        lastMousePos_ = event->position();
    } else if (draggingPoints_) {
        const QPointF delta = event->position() - lastMousePos_;
        const QRectF r = plotRect();
        const int dt = static_cast<int>(std::lround(
            delta.x() * (viewTimeMax_ - viewTimeMin_) / std::max<qreal>(1.0, r.width())));
        const int dv = static_cast<int>(std::lround(
            -delta.y() * (viewValueMax_ - viewValueMin_) / std::max<qreal>(1.0, r.height())));
        if (dt != 0 || dv != 0) {
            document_->moveSelected(dt, dv, mode_, logicSnap_);
            lastMousePos_ = event->position();
            updateOverlay();
        }
    } else if (selectingRect_) {
        selectionRect_ = QRectF(selectionStart_, event->position());
        update();
    }

    event->accept();
}

void BreakpointEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (selectingRect_ && document_) {
        const bool additive = event->modifiers().testFlag(Qt::ShiftModifier)
                           || event->modifiers().testFlag(Qt::ControlModifier)
                           || event->modifiers().testFlag(Qt::MetaModifier);
        document_->setPointSelection(refsInside(selectionRect_), additive);
    }

    draggingPoints_ = false;
    selectingRect_ = false;
    panning_ = false;
    update();
    updateOverlay();
    event->accept();
}

void BreakpointEditor::wheelEvent(QWheelEvent* event)
{
    if (!plotRect().contains(event->position())) {
        event->ignore();
        return;
    }

    double delta = event->angleDelta().y();
    if (qFuzzyIsNull(delta))
        delta = event->pixelDelta().y() * 2.0;
    if (qFuzzyIsNull(delta)) {
        event->ignore();
        return;
    }

    const double steps = delta / 120.0;
    const double factor = std::pow(0.82, steps);

    const double fullTime = std::max(1.0, document_ ? static_cast<double>(document_->durationMs()) : 1.0);
    const double fullValue = std::max(1.0, dataMaximumY());
    const double oldTSpan = viewTimeMax_ - viewTimeMin_;
    const double oldVSpan = viewValueMax_ - viewValueMin_;
    const double newTSpan = std::clamp(oldTSpan * factor, std::max(1.0, fullTime * 0.0005), fullTime);
    const double newVSpan = std::clamp(oldVSpan * factor, std::max(1.0, fullValue * 0.0005), fullValue);

    const double cursorTime = screenToTime(event->position().x());
    const double cursorValue = screenToValue(event->position().y());
    const QRectF r = plotRect();
    const double fx = std::clamp((event->position().x() - r.left()) / r.width(), 0.0, 1.0);
    const double fy = std::clamp((r.bottom() - event->position().y()) / r.height(), 0.0, 1.0);

    viewTimeMin_ = cursorTime - fx * newTSpan;
    viewTimeMax_ = viewTimeMin_ + newTSpan;
    viewValueMin_ = cursorValue - fy * newVSpan;
    viewValueMax_ = viewValueMin_ + newVSpan;

    viewFitted_ = qFuzzyCompare(newTSpan + 1.0, fullTime + 1.0)
               && qFuzzyCompare(newVSpan + 1.0, fullValue + 1.0);
    clampView();
    emit viewChanged();
    update();
    updateOverlay();
    event->accept();
}

void BreakpointEditor::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        update();
        updateOverlay();
    }
}

} // namespace hydra2
