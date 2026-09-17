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
#include "BreakpointGraphModel.hpp"

#include <QtCore/QPointF>
#include <QtCore/QVariant>
#include <QtGui/QVector3D>

#include <algorithm>
#include <cmath>
#include <utility>

namespace hydra2 {
namespace {

constexpr double kDepthSpacing = 2.0;

Qt::KeyboardModifiers toModifiers(int value)
{
    return Qt::KeyboardModifiers::fromInt(value);
}

} // namespace

TrackPointModel::TrackPointModel(HydraDocument* document,
                                 int partial,
                                 TrackKind kind,
                                 QObject* parent)
    : QAbstractListModel(parent)
    , document_(document)
    , partial_(partial)
    , kind_(kind)
{
    rowCountCache_ = document_ ? document_->track(partial_, kind_).size() : 0;
}

int TrackPointModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !document_)
        return 0;
    return document_->track(partial_, kind_).size();
}

QVariant TrackPointModel::data(const QModelIndex& index, int role) const
{
    if (!document_ || !index.isValid())
        return {};

    const auto& points = document_->track(partial_, kind_);
    if (index.row() < 0 || index.row() >= points.size())
        return {};

    const auto& point = points.at(index.row());
    switch (role) {
    case XPosRole:
        return point.timeMs;
    case YPosRole:
        return document_->displayValue(partial_, kind_, point);
    case ZPosRole:
        return (partial_ + 1) * kDepthSpacing;
    case PointIdRole:
        return QVariant::fromValue<qulonglong>(point.id);
    default:
        return {};
    }
}

QHash<int, QByteArray> TrackPointModel::roleNames() const
{
    return {
        {XPosRole, "xPos"},
        {YPosRole, "yPos"},
        {ZPosRole, "zPos"},
        {PointIdRole, "pointId"}
    };
}

void TrackPointModel::setKind(TrackKind kind)
{
    if (kind_ == kind)
        return;
    beginResetModel();
    kind_ = kind;
    rowCountCache_ = document_ ? document_->track(partial_, kind_).size() : 0;
    endResetModel();
}

void TrackPointModel::refresh()
{
    // ItemModelScatterDataProxy does not reliably rebuild an existing
    // Spline3DSeries immediately from QAbstractItemModel::dataChanged().
    // Reset only this partial's point model so Qt Graphs re-reads all
    // coordinates and uploads the changed spline geometry at once.
    const int newCount = document_ ? document_->track(partial_, kind_).size() : 0;

    beginResetModel();
    rowCountCache_ = newCount;
    endResetModel();
}

quint64 TrackPointModel::pointIdAt(int row) const
{
    if (!document_)
        return 0;
    const auto& points = document_->track(partial_, kind_);
    if (row < 0 || row >= points.size())
        return 0;
    return points.at(row).id;
}

SelectedPointModel::SelectedPointModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int SelectedPointModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : points_.size();
}

QVariant SelectedPointModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= points_.size())
        return {};

    const QVector3D& point = points_.at(index.row());
    switch (role) {
    case XPosRole:
        return point.x();
    case YPosRole:
        return point.y();
    case ZPosRole:
        return point.z();
    default:
        return {};
    }
}

QHash<int, QByteArray> SelectedPointModel::roleNames() const
{
    return {
        {XPosRole, "xPos"},
        {YPosRole, "yPos"},
        {ZPosRole, "zPos"}
    };
}

void SelectedPointModel::rebuild(HydraDocument* document, TrackKind kind)
{
    beginResetModel();
    points_.clear();
    refs_.clear();

    if (document) {
        for (const PointRef& ref : document->selectedPoints()) {
            if (ref.kind != kind)
                continue;
            const Breakpoint* point = document->point(ref);
            if (!point)
                continue;
            points_.push_back(QVector3D(
                static_cast<float>(point->timeMs),
                static_cast<float>(document->displayValue(ref.partial, kind, *point)),
                static_cast<float>((ref.partial + 1) * kDepthSpacing)));
            refs_.push_back(ref);
        }
    }

    endResetModel();
}

PointRef SelectedPointModel::refAt(int row) const
{
    if (row < 0 || row >= refs_.size())
        return {};
    return refs_.at(row);
}

BreakpointGraphModel::BreakpointGraphModel(QObject* parent)
    : QAbstractListModel(parent)
    , selectedPoints_(this)
{
}

int BreakpointGraphModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !document_)
        return 0;
    return document_->partialCount();
}

QVariant BreakpointGraphModel::data(const QModelIndex& index, int role) const
{
    if (!document_ || !index.isValid() || index.row() < 0 || index.row() >= pointModels_.size())
        return {};

    const int partial = index.row();
    switch (role) {
    case PartialIndexRole:
        return partial;
    case PointModelRole:
        return QVariant::fromValue<QObject*>(pointModels_.at(partial));
    case PartialSelectedRole:
        return document_->selectedPartials().contains(partial);
    default:
        return {};
    }
}

QHash<int, QByteArray> BreakpointGraphModel::roleNames() const
{
    return {
        {PartialIndexRole, "partialIndex"},
        {PointModelRole, "pointModel"},
        {PartialSelectedRole, "partialSelected"}
    };
}

void BreakpointGraphModel::setDocument(HydraDocument* document)
{
    if (document_ == document)
        return;

    if (document_)
        disconnect(document_, nullptr, this, nullptr);

    document_ = document;

    if (document_) {
        connect(document_, &HydraDocument::documentReset,
                this, &BreakpointGraphModel::rebuildModels);
        connect(document_, &HydraDocument::partialsChanged,
                this, &BreakpointGraphModel::refreshPartials);
        connect(document_, &HydraDocument::selectionChanged,
                this, &BreakpointGraphModel::refreshSelection);
    }

    rebuildModels();
    emit documentChanged();
}

void BreakpointGraphModel::setMode(int mode)
{
    const TrackKind next = mode == static_cast<int>(TrackKind::Frequency)
        ? TrackKind::Frequency
        : TrackKind::Amplitude;
    if (mode_ == next)
        return;

    mode_ = next;
    for (TrackPointModel* model : pointModels_)
        model->setKind(mode_);
    selectedPoints_.rebuild(document_.data(), mode_);
    activePoint_ = {};
    updateActiveLabel();
    recalculateRanges();

    emit modeChanged();
    emit graphChanged();
}

void BreakpointGraphModel::recalculateRanges()
{
    durationMs_ = document_ ? std::max(1, document_->durationMs()) : 1.0;
    maximumZ_ = document_ && document_->partialCount() > 0
        ? document_->partialCount() * kDepthSpacing
        : kDepthSpacing;

    if (!document_ || mode_ == TrackKind::Amplitude) {
        maximumY_ = 32767.0;
        return;
    }

    int maximum = 1000;
    for (int partial = 0; partial < document_->partialCount(); ++partial) {
        for (const auto& point : document_->track(partial, TrackKind::Frequency))
            maximum = std::max(maximum, point.value);
    }
    const int rounded = ((maximum + 999) / 1000) * 1000;
    maximumY_ = std::min(32767, std::max(1000, rounded));
}

bool BreakpointGraphModel::additiveFromModifiers(Qt::KeyboardModifiers modifiers) const
{
    return modifiers.testFlag(Qt::ShiftModifier)
        || modifiers.testFlag(Qt::ControlModifier)
        || modifiers.testFlag(Qt::MetaModifier);
}

bool BreakpointGraphModel::toggleFromModifiers(Qt::KeyboardModifiers modifiers) const
{
    return modifiers.testFlag(Qt::ControlModifier)
        || modifiers.testFlag(Qt::MetaModifier);
}

bool BreakpointGraphModel::beginPointDrag(int partial,
                                          int pointIndex,
                                          int modifiers,
                                          bool logicSnap)
{
    Q_UNUSED(logicSnap);
    if (!document_ || partial < 0 || partial >= pointModels_.size())
        return false;

    const quint64 id = pointModels_.at(partial)->pointIdAt(pointIndex);
    if (id == 0)
        return false;

    const PointRef ref{partial, mode_, id};
    const auto mods = toModifiers(modifiers);
    const bool toggle = toggleFromModifiers(mods);

    activePoint_ = ref;
    if (!document_->isPointSelected(ref) || toggle)
        document_->selectPoint(ref, additiveFromModifiers(mods), toggle);

    dragging_ = document_->isPointSelected(ref);
    updateActiveLabel();
    return dragging_;
}

bool BreakpointGraphModel::beginSelectedPointDrag(int selectedIndex,
                                                  int modifiers,
                                                  bool logicSnap)
{
    Q_UNUSED(logicSnap);
    if (!document_)
        return false;

    const PointRef ref = selectedPoints_.refAt(selectedIndex);
    if (ref.id == 0)
        return false;

    const auto mods = toModifiers(modifiers);
    const bool toggle = toggleFromModifiers(mods);

    activePoint_ = ref;
    if (!document_->isPointSelected(ref) || toggle)
        document_->selectPoint(ref, additiveFromModifiers(mods), toggle);

    dragging_ = document_->isPointSelected(ref);
    updateActiveLabel();
    return dragging_;
}

bool BreakpointGraphModel::beginPartialDrag(int partial,
                                            int modifiers,
                                            bool logicSnap)
{
    Q_UNUSED(logicSnap);
    if (!document_ || partial < 0 || partial >= document_->partialCount())
        return false;

    const auto mods = toModifiers(modifiers);
    document_->selectWholePartial(partial,
                                  additiveFromModifiers(mods),
                                  toggleFromModifiers(mods));
    activePoint_ = {};
    dragging_ = document_->selectedPartials().contains(partial);
    updateActiveLabel();
    return dragging_;
}

bool BreakpointGraphModel::beginFrontPick(double graphX,
                                          double graphY,
                                          double viewWidth,
                                          double viewHeight,
                                          double cameraZoomLevel,
                                          int modifiers,
                                          bool logicSnap)
{
    if (!document_ || viewWidth <= 1.0 || viewHeight <= 1.0)
        return false;

    // graphPositionQuery returns normalized graph coordinates in [-1, 1].
    // In FrontLow orthographic view Z is purely display depth, so the nearest
    // editable item is determined from X/Y exactly like the old 2D editor.
    if (graphX < -1.05 || graphX > 1.05 || graphY < -1.05 || graphY > 1.05)
        return false;

    const double zoom = std::max(1.0, cameraZoomLevel) / 100.0;
    const double sx = 0.5 * viewWidth * zoom;
    const double sy = 0.5 * viewHeight * zoom;
    constexpr double pointRadius = 10.0;
    constexpr double lineRadius = 6.0;

    int bestPartial = -1;
    int bestPoint = -1;
    double bestPointDistance2 = pointRadius * pointRadius;

    const auto normalizedPoint = [&](int partial, const Breakpoint& point) {
        const double x = 2.0 * (static_cast<double>(point.timeMs) / durationMs_) - 1.0;
        const double yValue = document_->displayValue(partial, mode_, point);
        const double y = 2.0 * (yValue / maximumY_) - 1.0;
        return QPointF((x - graphX) * sx, (y - graphY) * sy);
    };

    // Handles have priority over strokes. Prefer an already-selected partial
    // if two front-projected points overlap almost exactly.
    for (int partial = 0; partial < document_->partialCount(); ++partial) {
        const auto& points = document_->track(partial, mode_);
        for (qsizetype i = 0; i < points.size(); ++i) {
            const QPointF p = normalizedPoint(partial, points.at(i));
            const double d2 = p.x() * p.x() + p.y() * p.y();
            if (d2 > bestPointDistance2)
                continue;

            const bool prefer = bestPartial < 0
                || d2 < bestPointDistance2 - 0.25
                || (std::abs(d2 - bestPointDistance2) <= 0.25
                    && document_->selectedPartials().contains(partial)
                    && !document_->selectedPartials().contains(bestPartial));
            if (prefer) {
                bestPointDistance2 = d2;
                bestPartial = partial;
                bestPoint = static_cast<int>(i);
            }
        }
    }

    if (bestPoint >= 0)
        return beginPointDrag(bestPartial, bestPoint, modifiers, logicSnap);

    int bestLinePartial = -1;
    double bestLineDistance2 = lineRadius * lineRadius;

    for (int partial = 0; partial < document_->partialCount(); ++partial) {
        const auto& points = document_->track(partial, mode_);
        if (points.size() < 2)
            continue;

        QPointF a = normalizedPoint(partial, points.first());
        for (qsizetype i = 1; i < points.size(); ++i) {
            const QPointF b = normalizedPoint(partial, points.at(i));
            const QPointF ab = b - a;
            const double length2 = ab.x() * ab.x() + ab.y() * ab.y();
            double t = 0.0;
            if (length2 > 1.0e-9)
                t = std::clamp(-(a.x() * ab.x() + a.y() * ab.y()) / length2, 0.0, 1.0);
            const QPointF closest = a + ab * t;
            const double d2 = closest.x() * closest.x() + closest.y() * closest.y();
            if (d2 < bestLineDistance2) {
                bestLineDistance2 = d2;
                bestLinePartial = partial;
            }
            a = b;
        }
    }

    if (bestLinePartial >= 0)
        return beginPartialDrag(bestLinePartial, modifiers, logicSnap);

    return false;
}

void BreakpointGraphModel::dragByPixels(double deltaX,
                                        double deltaY,
                                        double viewWidth,
                                        double viewHeight,
                                        double cameraZoomLevel,
                                        bool logicSnap)
{
    if (!document_ || !dragging_ || viewWidth <= 1.0 || viewHeight <= 1.0)
        return;

    // Camera zoom changes the visible range, so scale edit sensitivity with it.
    // Z remains completely untouched: partial depth is display-only.
    const double zoomScale = 100.0 / std::max(1.0, cameraZoomLevel);
    const int dt = static_cast<int>(std::lround(
        deltaX * durationMs() / viewWidth * zoomScale));
    const int dv = static_cast<int>(std::lround(
        -deltaY * maximumY() / viewHeight * zoomScale));
    if (dt == 0 && dv == 0)
        return;

    document_->moveSelected(dt, dv, mode_, logicSnap);
    updateActiveLabel();
}

void BreakpointGraphModel::endDrag()
{
    if (!dragging_)
        return;
    dragging_ = false;
    recalculateRanges();
    emit graphChanged();
}

void BreakpointGraphModel::clearSelection()
{
    if (document_)
        document_->clearSelection();
    activePoint_ = {};
    updateActiveLabel();
}

void BreakpointGraphModel::selectPartial(int partial, int modifiers)
{
    if (!document_ || partial < 0 || partial >= document_->partialCount())
        return;

    const auto mods = toModifiers(modifiers);
    document_->selectWholePartial(partial,
                                  additiveFromModifiers(mods),
                                  toggleFromModifiers(mods));
    activePoint_ = {};
    updateActiveLabel();
}

void BreakpointGraphModel::rebuildModels()
{
    beginResetModel();
    for (TrackPointModel* model : std::as_const(pointModels_))
        delete model;
    pointModels_.clear();

    if (document_) {
        pointModels_.reserve(document_->partialCount());
        for (int partial = 0; partial < document_->partialCount(); ++partial) {
            pointModels_.push_back(
                new TrackPointModel(document_, partial, mode_, this));
        }
    }
    endResetModel();

    selectedPoints_.rebuild(document_.data(), mode_);
    activePoint_ = {};
    updateActiveLabel();
    recalculateRanges();
    emit graphChanged();
}

void BreakpointGraphModel::refreshPartials(const QVector<int>& partials)
{
    for (int partial : partials) {
        if (partial >= 0 && partial < pointModels_.size())
            pointModels_.at(partial)->refresh();
    }

    selectedPoints_.rebuild(document_.data(), mode_);

    // A breakpoint move can change the analysis duration, and frequency edits
    // can change the Y range. Keep the 3D axes synchronized as well.
    recalculateRanges();
    emit graphChanged();
}

void BreakpointGraphModel::refreshSelection()
{
    if (!pointModels_.isEmpty()) {
        emit dataChanged(index(0, 0), index(pointModels_.size() - 1, 0),
                         {PartialSelectedRole});
    }
    selectedPoints_.rebuild(document_.data(), mode_);
    updateActiveLabel();
}

void BreakpointGraphModel::updateActiveLabel()
{
    QString next;
    if (document_ && activePoint_.id != 0) {
        if (const Breakpoint* point = document_->point(activePoint_)) {
            const int display = static_cast<int>(std::lround(
                document_->displayValue(activePoint_.partial, activePoint_.kind, *point)));
            next = mode_ == TrackKind::Amplitude
                ? QStringLiteral("P%1   %2 ms   amp %3")
                      .arg(activePoint_.partial + 1)
                      .arg(point->timeMs)
                      .arg(display)
                : QStringLiteral("P%1   %2 ms   %3 Hz")
                      .arg(activePoint_.partial + 1)
                      .arg(point->timeMs)
                      .arg(display);
        }
    }

    if (activeLabel_ == next)
        return;
    activeLabel_ = next;
    emit activeLabelChanged();
}

} // namespace hydra2
