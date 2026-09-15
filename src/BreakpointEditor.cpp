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

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

#include <algorithm>
#include <cmath>

namespace hydra2 {
namespace {

constexpr qreal kLeft = 58.0;
constexpr qreal kTop = 18.0;
constexpr qreal kRight = 16.0;
constexpr qreal kBottom = 38.0;
constexpr qreal kHitRadius = 8.0;
constexpr qreal kStrokeHitRadius = 5.0;

double pointToSegmentDistanceSquared(const QPointF& point,
                                     const QPointF& a,
                                     const QPointF& b)
{
    const QPointF ab = b - a;
    const QPointF ap = point - a;
    const double lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();

    if (lengthSquared <= 0.0) {
        const double dx = point.x() - a.x();
        const double dy = point.y() - a.y();
        return dx * dx + dy * dy;
    }

    const double projection = std::clamp(
        (ap.x() * ab.x() + ap.y() * ab.y()) / lengthSquared,
        0.0,
        1.0
    );
    const QPointF nearest = a + ab * projection;
    const double dx = point.x() - nearest.x();
    const double dy = point.y() - nearest.y();
    return dx * dx + dy * dy;
}

bool validRef(const PointRef& ref)
{
    return ref.partial >= 0 && ref.id != 0;
}

} // namespace

BreakpointEditor::BreakpointEditor(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAntialiasing(true);
}

void BreakpointEditor::setDocument(HydraDocument* document)
{
    if (document_ == document)
        return;
    if (document_)
        disconnect(document_, nullptr, this, nullptr);
    document_ = document;
    if (document_) {
        connect(document_, &HydraDocument::documentReset, this, [this] { update(); });
        connect(document_, &HydraDocument::dataChanged, this, [this] { update(); });
        connect(document_, &HydraDocument::selectionChanged, this, [this] { update(); });
    }
    emit documentChanged();
    update();
}

void BreakpointEditor::setMode(int mode)
{
    const TrackKind newMode = mode == 1 ? TrackKind::Frequency : TrackKind::Amplitude;
    if (mode_ == newMode)
        return;
    mode_ = newMode;
    activePoint_ = {};
    emit modeChanged();
    update();
}

void BreakpointEditor::setLogicSnap(bool enabled)
{
    if (logicSnap_ == enabled)
        return;
    logicSnap_ = enabled;
    emit logicSnapChanged();
}

QRectF BreakpointEditor::plotRect() const
{
    return QRectF(kLeft, kTop,
                  std::max<qreal>(1.0, width() - kLeft - kRight),
                  std::max<qreal>(1.0, height() - kTop - kBottom));
}

double BreakpointEditor::maxY() const
{
    if (!document_ || mode_ == TrackKind::Amplitude)
        return 32767.0;

    int maximum = 1000;
    for (int partial = 0; partial < document_->partialCount(); ++partial) {
        for (const auto& point : document_->track(partial, TrackKind::Frequency))
            maximum = std::max(maximum, point.value);
    }
    const int rounded = ((maximum + 999) / 1000) * 1000;
    return std::min(32767, std::max(1000, rounded));
}

QPointF BreakpointEditor::toScreen(int partial, const Breakpoint& point,
                                        double durationMs, double yMaximum) const
{
    const QRectF r = plotRect();
    const double x = r.left() + (point.timeMs / durationMs) * r.width();
    const double value = document_ ? document_->displayValue(partial, mode_, point) : point.value;
    const double y = r.bottom() - (value / yMaximum) * r.height();
    return {x, y};
}

void BreakpointEditor::paint(QPainter* painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillRect(boundingRect(), QColor(QStringLiteral("#181a1d")));

    const QRectF r = plotRect();
    painter->fillRect(r, QColor(QStringLiteral("#202328")));

    QPen gridPen(QColor(255, 255, 255, 28));
    gridPen.setWidthF(1.0);
    painter->setPen(gridPen);
    for (int i = 0; i <= 10; ++i) {
        const qreal x = r.left() + r.width() * i / 10.0;
        painter->drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
    }
    for (int i = 0; i <= 8; ++i) {
        const qreal y = r.top() + r.height() * i / 8.0;
        painter->drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
    }

    const double duration = std::max(1, document_ ? document_->durationMs() : 1);
    const double yMaximum = maxY();

    painter->setPen(QColor(210, 214, 220));
    for (int i = 0; i <= 5; ++i) {
        const qreal x = r.left() + r.width() * i / 5.0;
        const double seconds = (duration * i / 5.0) / 1000.0;
        painter->drawText(QRectF(x - 35, r.bottom() + 8, 70, 20),
                          Qt::AlignHCenter | Qt::AlignTop,
                          QString::number(seconds, 'f', duration < 10000 ? 2 : 1) + QStringLiteral(" s"));
    }

    for (int i = 0; i <= 4; ++i) {
        const qreal y = r.bottom() - r.height() * i / 4.0;
        const int value = static_cast<int>(std::lround(yMaximum * i / 4.0));
        painter->drawText(QRectF(3, y - 9, kLeft - 8, 18),
                          Qt::AlignRight | Qt::AlignVCenter,
                          mode_ == TrackKind::Amplitude
                              ? QString::number(value)
                              : QString::number(value) + QStringLiteral(" Hz"));
    }

    if (!document_ || !document_->loaded()) {
        painter->setPen(QColor(170, 175, 184));
        painter->drawText(r, Qt::AlignCenter,
                          QStringLiteral("Open a Csound HETRO / adsyn analysis file"));
        return;
    }

    for (int partial = 0; partial < document_->partialCount(); ++partial) {
        const bool selectedPartial = document_->selectedPartials().contains(partial);
        const auto& points = document_->track(partial, mode_);
        if (points.isEmpty())
            continue;

        QColor lineColor = selectedPartial
            ? QColor(78, 181, 255, 245)
            : QColor(190, 198, 208, 72);
        QPen linePen(lineColor);
        linePen.setWidthF(selectedPartial ? 1.8 : 1.0);
        painter->setPen(linePen);

        QPainterPath path;
        path.moveTo(toScreen(partial, points.first(), duration, yMaximum));
        for (qsizetype i = 1; i < points.size(); ++i){
            path.lineTo(toScreen(partial, points.at(i), duration, yMaximum));
        }
        painter->strokePath(path,linePen);

        for (const auto& point : points) {
            const PointRef ref{partial, mode_, point.id};
            const bool selectedPoint = document_->isPointSelected(ref);
            const QPointF pos = toScreen(partial, point, duration, yMaximum);
            const qreal halfSize = selectedPoint ? 5.0 : (selectedPartial ? 3.6 : 2.4);
            painter->setPen(selectedPoint ? QColor(255, 236, 120) : lineColor);
            painter->setBrush(selectedPoint ? QColor(255, 214, 64) : QColor(32, 35, 40));
            painter->drawRect(QRectF(pos.x() - halfSize,
                                     pos.y() - halfSize,
                                     halfSize * 2.0,
                                     halfSize * 2.0));
        }
    }

    if (lassoing_) {
        QPen lassoPen(QColor(120, 200, 255, 230));
        lassoPen.setStyle(Qt::DashLine);
        painter->setPen(lassoPen);
        painter->setBrush(QColor(90, 180, 255, 35));
        painter->drawRect(lassoRect_.normalized());
    }

    if (validRef(activePoint_)) {
        const Breakpoint* point = document_->point(activePoint_);
        if (point) {
            const QPointF pos = toScreen(activePoint_.partial, *point, duration, yMaximum);
            const QString text = activeLabel();
            const QFontMetrics fm(painter->font());
            QRectF labelRect = fm.boundingRect(text);
            labelRect.adjust(-6, -4, 6, 4);
            labelRect.moveTopLeft(pos + QPointF(10, -labelRect.height() - 7));
            if (labelRect.right() > r.right())
                labelRect.moveRight(r.right());
            if (labelRect.top() < r.top())
                labelRect.moveTop(pos.y() + 10);
            painter->setPen(QColor(230, 233, 238));
            painter->setBrush(QColor(22, 24, 28, 235));
            painter->drawRoundedRect(labelRect, 4, 4);
            painter->drawText(labelRect, Qt::AlignCenter, text);
        }
    }
}

PointRef BreakpointEditor::hitTest(const QPointF& pos) const
{
    if (!document_)
        return {};

    const double duration = std::max(1, document_->durationMs());
    const double yMaximum = maxY();

    PointRef best;
    double bestDistance = kHitRadius * kHitRadius;

    // Selected partials are tested last so they win ties and are easier to edit.
    QVector<int> order;
    order.reserve(document_->partialCount());
    for (int p = 0; p < document_->partialCount(); ++p)
        if (!document_->selectedPartials().contains(p))
            order.push_back(p);
    for (int p = 0; p < document_->partialCount(); ++p)
        if (document_->selectedPartials().contains(p))
            order.push_back(p);

    for (int partial : order) {
        for (const auto& point : document_->track(partial, mode_)) {
            const QPointF screen = toScreen(partial, point, duration, yMaximum);
            const double dx = screen.x() - pos.x();
            const double dy = screen.y() - pos.y();
            const double d2 = dx * dx + dy * dy;
            if (d2 <= bestDistance) {
                bestDistance = d2;
                best = {partial, mode_, point.id};
            }
        }
    }
    return best;
}

int BreakpointEditor::hitTestStroke(const QPointF& pos) const
{
    if (!document_)
        return -1;

    const double duration = std::max(1, document_->durationMs());
    const double yMaximum = maxY();
    const double hitDistanceSquared = kStrokeHitRadius * kStrokeHitRadius;

    int bestPartial = -1;
    double bestDistance = hitDistanceSquared;

    // As with point hit-testing, selected partials are checked last so they
    // win close overlaps and remain easy to work with.
    QVector<int> order;
    order.reserve(document_->partialCount());
    for (int p = 0; p < document_->partialCount(); ++p)
        if (!document_->selectedPartials().contains(p))
            order.push_back(p);
    for (int p = 0; p < document_->partialCount(); ++p)
        if (document_->selectedPartials().contains(p))
            order.push_back(p);

    for (int partial : order) {
        const auto& points = document_->track(partial, mode_);
        if (points.size() < 2)
            continue;

        QPointF previous = toScreen(partial, points.first(), duration, yMaximum);
        for (qsizetype i = 1; i < points.size(); ++i) {
            const QPointF current = toScreen(partial, points.at(i), duration, yMaximum);
            const double distance = pointToSegmentDistanceSquared(pos, previous, current);
            if (distance <= bestDistance) {
                bestDistance = distance;
                bestPartial = partial;
            }
            previous = current;
        }
    }

    return bestPartial;
}

QVector<PointRef> BreakpointEditor::refsInside(const QRectF& rect) const
{
    QVector<PointRef> refs;
    if (!document_)
        return refs;
    const double duration = std::max(1, document_->durationMs());
    const double yMaximum = maxY();
    const QRectF normalized = rect.normalized();
    for (int partial = 0; partial < document_->partialCount(); ++partial) {
        for (const auto& point : document_->track(partial, mode_)) {
            if (normalized.contains(toScreen(partial, point, duration, yMaximum)))
                refs.push_back({partial, mode_, point.id});
        }
    }
    return refs;
}

QString BreakpointEditor::activeLabel() const
{
    if (!document_)
        return {};
    const Breakpoint* point = document_->point(activePoint_);
    if (!point)
        return {};

    const int display = static_cast<int>(std::lround(document_->displayValue(activePoint_.partial,
                                                                              activePoint_.kind,
                                                                              *point)));
    return mode_ == TrackKind::Amplitude
        ? QStringLiteral("P%1   %2 ms   amp %3")
              .arg(activePoint_.partial + 1)
              .arg(point->timeMs)
              .arg(display)
        : QStringLiteral("P%1   %2 ms   %3 Hz")
              .arg(activePoint_.partial + 1)
              .arg(point->timeMs)
              .arg(display);
}

void BreakpointEditor::mousePressEvent(QMouseEvent* event)
{
    if (!document_ || !document_->loaded() || !plotRect().contains(event->position())) {
        event->ignore();
        return;
    }

    const bool additive = event->modifiers().testFlag(Qt::ShiftModifier)
                       || event->modifiers().testFlag(Qt::ControlModifier)
                       || event->modifiers().testFlag(Qt::MetaModifier);
    const bool toggle = event->modifiers().testFlag(Qt::ControlModifier)
                     || event->modifiers().testFlag(Qt::MetaModifier);
    const PointRef hit = hitTest(event->position());
    lastMousePos_ = event->position();

    if (validRef(hit)) {
        activePoint_ = hit;
        if (!document_->isPointSelected(hit) || toggle)
            document_->selectPoint(hit, additive, toggle);
        draggingPoints_ = true;
        lassoing_ = false;
    } else {
        const int strokePartial = hitTestStroke(event->position());

        if (strokePartial >= 0) {
            document_->selectWholePartial(strokePartial, additive, toggle);
            activePoint_ = {};
            draggingPoints_ = document_->selectedPartials().contains(strokePartial);
            lassoing_ = false;
        } else {
            if (!additive)
                document_->clearSelection();
            draggingPoints_ = false;
            lassoing_ = true;
            lassoStart_ = event->position();
            lassoRect_ = QRectF(lassoStart_, lassoStart_);
            activePoint_ = {};
        }
    }
    update();
    event->accept();
}

void BreakpointEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (!document_)
        return;

    if (draggingPoints_) {
        const QPointF delta = event->position() - lastMousePos_;
        const QRectF r = plotRect();
        const int dt = static_cast<int>(std::lround(delta.x() * std::max(1, document_->durationMs()) / r.width()));
        const int dv = static_cast<int>(std::lround(-delta.y() * maxY() / r.height()));
        if (dt != 0 || dv != 0) {
            document_->moveSelected(dt, dv, mode_, logicSnap_);
            lastMousePos_ = event->position();
        }
    } else if (lassoing_) {
        lassoRect_ = QRectF(lassoStart_, event->position());
        update();
    }
    event->accept();
}

void BreakpointEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (lassoing_ && document_) {
        const bool additive = event->modifiers().testFlag(Qt::ShiftModifier)
                           || event->modifiers().testFlag(Qt::ControlModifier)
                           || event->modifiers().testFlag(Qt::MetaModifier);
        document_->setPointSelection(refsInside(lassoRect_), additive);
    }
    draggingPoints_ = false;
    lassoing_ = false;
    update();
    event->accept();
}

} // namespace hydra2
