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
#include "PartialMixer.hpp"

#include <QtGui/QMouseEvent>

#include <algorithm>
#include <utility>

namespace hydra2 {

PartialMixer::PartialMixer(QQuickItem* parent)
    : QQuickItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
}

void PartialMixer::setDocument(HydraDocument* document)
{
    if (document_ == document)
        return;

    if (document_)
        disconnect(document_, nullptr, this, nullptr);

    document_ = document;

    if (document_) {
        connect(document_, &HydraDocument::documentReset, this, [this] {
            rebuildAverageCache();
            emit valuesChanged();
        });
        connect(document_, &HydraDocument::rawPartialsChanged, this,
                [this](const QVector<int>& partials) {
                    refreshAverageCache(partials);
                });
        connect(document_, &HydraDocument::partialsChanged,
                this, &PartialMixer::valuesChanged);
        connect(document_, &HydraDocument::selectionChanged,
                this, &PartialMixer::selectionValuesChanged);
    }

    rebuildAverageCache();
    emit documentChanged();
    emit valuesChanged();
    emit selectionValuesChanged();
}

QRectF PartialMixer::innerRect() const
{
    return QRectF(0.0, 0.0,
                  std::max<qreal>(1.0, width()),
                  std::max<qreal>(1.0, height()));
}

void PartialMixer::rebuildAverageCache()
{
    averageAmplitudes_.clear();
    maximumAverage_ = 1.0;

    if (!document_)
        return;

    averageAmplitudes_.resize(document_->partialCount());
    for (int p = 0; p < document_->partialCount(); ++p) {
        const double average = document_->partialAverageAmplitude(p);
        averageAmplitudes_[p] = average;
        maximumAverage_ = std::max(maximumAverage_, average);
    }
}

void PartialMixer::refreshAverageCache(const QVector<int>& partials)
{
    if (!document_)
        return;

    if (averageAmplitudes_.size() != document_->partialCount()) {
        rebuildAverageCache();
        return;
    }

    for (int partial : partials) {
        if (partial >= 0 && partial < averageAmplitudes_.size())
            averageAmplitudes_[partial] = document_->partialAverageAmplitude(partial);
    }

    maximumAverage_ = 1.0;
    for (double average : std::as_const(averageAmplitudes_))
        maximumAverage_ = std::max(maximumAverage_, average);
}

double PartialMixer::maximumAverage() const
{
    return maximumAverage_;
}

double PartialMixer::naturalHeight(int partial, double maxAverage) const
{
    if (!document_ || maxAverage <= 0.0)
        return 2.0;

    const double average = partial >= 0 && partial < averageAmplitudes_.size()
        ? averageAmplitudes_.at(partial)
        : 0.0;
    const double fraction = average / maxAverage;
    return std::max(2.0, fraction * innerRect().height());
}

QVariantList PartialMixer::values() const
{
    QVariantList result;
    if (!document_ || document_->partialCount() == 0)
        return result;

    const double maxAverage = maximumAverage();
    result.reserve(document_->partialCount());

    for (int p = 0; p < document_->partialCount(); ++p) {
        const double average = p < averageAmplitudes_.size() ? averageAmplitudes_.at(p) : 0.0;
        const double naturalFraction = average / maxAverage;
        const double visible = std::clamp(naturalFraction * document_->partialGain(p), 0.0, 1.0);
        result.push_back(visible);
    }

    return result;
}

QVariantList PartialMixer::selectedBars() const
{
    QVariantList result;
    if (!document_)
        return result;

    QVector<int> selected = document_->selectedPartials().values();
    std::sort(selected.begin(), selected.end());
    result.reserve(selected.size());
    for (int partial : std::as_const(selected))
        result.push_back(partial);
    return result;
}

int PartialMixer::partialAt(qreal x) const
{
    if (!document_ || document_->partialCount() == 0)
        return -1;

    const QRectF r = innerRect();
    if (x < r.left() || x > r.right())
        return -1;

    const int p = static_cast<int>((x - r.left()) / r.width() * document_->partialCount());
    return std::clamp(p, 0, document_->partialCount() - 1);
}

void PartialMixer::editAt(const QPointF& pos)
{
    if (!document_ || dragPartials_.isEmpty())
        return;

    const QRectF r = innerRect();
    const double deltaHeight = pressPos_.y() - pos.y();

    QVector<double> gains;
    gains.reserve(dragPartials_.size());

    for (qsizetype i = 0; i < dragPartials_.size(); ++i) {
        const int partial = dragPartials_.at(i);
        const double natural = std::max(2.0, dragNaturalHeights_.at(i));
        const double startHeight = natural * dragStartGains_.at(i);
        const double requestedHeight = std::clamp(
            startHeight + deltaHeight,
            0.0,
            static_cast<double>(r.height()));

        double gain = requestedHeight / natural;
        gain = std::min(gain, document_->partialMaximumGain(partial));
        gains.push_back(gain);
    }

    document_->setPartialGains(dragPartials_, gains);
}

void PartialMixer::mousePressEvent(QMouseEvent* event)
{
    draggingPartial_ = partialAt(event->position().x());
    if (draggingPartial_ < 0) {
        event->ignore();
        return;
    }

    pressPos_ = event->position();
    levelDragStarted_ = false;

    const bool additive = event->modifiers().testFlag(Qt::ShiftModifier)
                       || event->modifiers().testFlag(Qt::ControlModifier)
                       || event->modifiers().testFlag(Qt::MetaModifier);
    const bool toggle = event->modifiers().testFlag(Qt::ControlModifier)
                     || event->modifiers().testFlag(Qt::MetaModifier);

    const bool alreadySelected =
        document_->selectedPartials().contains(draggingPartial_);

    if (additive || toggle) {
        document_->setPartialSelection(draggingPartial_, additive, toggle);
    } else if (!alreadySelected) {
        document_->setPartialSelection(draggingPartial_, false, false);
    }

    if (!document_->selectedPartials().contains(draggingPartial_)) {
        draggingPartial_ = -1;
        event->accept();
        return;
    }

    const double maxAverage = maximumAverage();
    dragPartials_ = document_->selectedPartials().values();
    std::sort(dragPartials_.begin(), dragPartials_.end());

    dragStartGains_.clear();
    dragNaturalHeights_.clear();
    dragStartGains_.reserve(dragPartials_.size());
    dragNaturalHeights_.reserve(dragPartials_.size());

    for (int partial : std::as_const(dragPartials_)) {
        dragStartGains_.push_back(document_->partialGain(partial));
        dragNaturalHeights_.push_back(naturalHeight(partial, maxAverage));
    }

    event->accept();
}

void PartialMixer::mouseMoveEvent(QMouseEvent* event)
{
    if (draggingPartial_ < 0)
        return;

    if (!levelDragStarted_ &&
        (event->position() - pressPos_).manhattanLength() >= 3.0) {
        levelDragStarted_ = true;
    }

    if (levelDragStarted_)
        editAt(event->position());

    event->accept();
}

void PartialMixer::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    draggingPartial_ = -1;
    levelDragStarted_ = false;
    dragPartials_.clear();
    dragStartGains_.clear();
    dragNaturalHeights_.clear();
}

} // namespace hydra2
