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
#include <QtGui/QPainter>

#include <algorithm>
#include <utility>

namespace hydra2 {

PartialMixer::PartialMixer(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAntialiasing(true);
}

void PartialMixer::setDocument(HydraDocument* document)
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

QRectF PartialMixer::innerRect() const
{
    return QRectF(12, 10, std::max<qreal>(1.0, width() - 24), std::max<qreal>(1.0, height() - 30));
}

void PartialMixer::paint(QPainter* painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillRect(boundingRect(), QColor(QStringLiteral("#181a1d")));
    const QRectF r = innerRect();
    painter->fillRect(r, QColor(QStringLiteral("#202328")));

    if (!document_ || document_->partialCount() == 0) {
        painter->setPen(QColor(160, 165, 175));
        painter->drawText(r, Qt::AlignCenter, QStringLiteral("Partial mixer"));
        return;
    }

    const int count = document_->partialCount();
    QVector<double> averages(count);
    double maxAverage = 1.0;
    for (int p = 0; p < count; ++p) {
        averages[p] = document_->partialAverageAmplitude(p);
        maxAverage = std::max(maxAverage, averages[p]);
    }

    const qreal columnWidth = r.width() / count;
    const qreal gap = std::clamp(columnWidth * 0.18, 1.0, 5.0);

    for (int p = 0; p < count; ++p) {
        const qreal left = r.left() + p * columnWidth + gap * 0.5;
        const qreal barWidth = std::max<qreal>(1.0, columnWidth - gap);
        const double average = averages.at(p);
        const double naturalFraction = average / maxAverage;
        const qreal naturalHeight = std::max<qreal>(2.0, naturalFraction * r.height());
        const qreal visibleHeight = std::min<qreal>(
            r.height(),
            naturalHeight * document_->partialGain(p)
        );

        QRectF slot(left, r.bottom() - naturalHeight, barWidth, naturalHeight);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 24));
        painter->drawRoundedRect(slot, 2, 2);

        QRectF bar(left, r.bottom() - visibleHeight, barWidth, visibleHeight);
        const bool selected = document_->selectedPartials().contains(p);
        painter->setBrush(selected ? QColor(78, 181, 255, 220) : QColor(178, 187, 199, 170));
        painter->drawRoundedRect(bar, 2, 2);

        if (selected) {
            QPen outline(QColor(135, 215, 255));
            outline.setWidthF(1.5);
            painter->setPen(outline);
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(QRectF(left - 1, r.top(), barWidth + 2, r.height()), 2, 2);
        }

        if (count <= 32 || (p % std::max(1, count / 16) == 0)) {
            painter->setPen(QColor(190, 195, 205));
            painter->drawText(QRectF(left - 3, r.bottom() + 4, barWidth + 6, 16),
                              Qt::AlignHCenter | Qt::AlignTop,
                              QString::number(p + 1));
        }
    }
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

void PartialMixer::editAt(const QPointF& pos, Qt::KeyboardModifiers modifiers, bool select)
{
    if (!document_)
        return;
    const int p = draggingPartial_ >= 0 ? draggingPartial_ : partialAt(pos.x());
    if (p < 0)
        return;

    if (select) {
        const bool additive = modifiers.testFlag(Qt::ShiftModifier)
                           || modifiers.testFlag(Qt::ControlModifier)
                           || modifiers.testFlag(Qt::MetaModifier);
        const bool toggle = modifiers.testFlag(Qt::ControlModifier)
                         || modifiers.testFlag(Qt::MetaModifier);
        document_->setPartialSelection(p, additive, toggle);
    }

    if (dragPartials_.isEmpty())
        return;

    const QRectF r = innerRect();
    const double deltaHeight = pressPos_.y() - pos.y();

    QVector<double> gains;
    gains.reserve(dragPartials_.size());

    for (qsizetype i = 0; i < dragPartials_.size(); ++i) {
        const int partial = dragPartials_.at(i);
        const double naturalHeight = std::max(2.0, dragNaturalHeights_.at(i));
        const double startHeight = naturalHeight * dragStartGains_.at(i);
        const double requestedHeight = std::clamp(
            startHeight + deltaHeight,
            0.0,
            static_cast<double>(r.height())
        );

        double gain = requestedHeight / naturalHeight;
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

    // Clicking one member of an existing multi-selection must not collapse
    // the selection before a linked drag starts.
    const bool alreadySelected =
        document_->selectedPartials().contains(draggingPartial_);

    if (additive || toggle) {
        document_->setPartialSelection(draggingPartial_, additive, toggle);
    } else if (!alreadySelected) {
        document_->setPartialSelection(draggingPartial_, false, false);
    }

    // A Ctrl/Cmd-click may have toggled the clicked partial off. In that case
    // the gesture is selection-only and does not start a level drag.
    if (!document_->selectedPartials().contains(draggingPartial_)) {
        draggingPartial_ = -1;
        event->accept();
        return;
    }

    // Cache the starting state for every selected slider. During the drag all
    // slider tops move by the same number of pixels, preserving their visual
    // relationship while still respecting each partial's own headroom.
    double maxAverage = 1.0;
    for (int i = 0; i < document_->partialCount(); ++i)
        maxAverage = std::max(maxAverage, document_->partialAverageAmplitude(i));

    dragPartials_ = document_->selectedPartials().values();
    std::sort(dragPartials_.begin(), dragPartials_.end());

    dragStartGains_.clear();
    dragNaturalHeights_.clear();
    dragStartGains_.reserve(dragPartials_.size());
    dragNaturalHeights_.reserve(dragPartials_.size());

    for (int partial : std::as_const(dragPartials_)) {
        const double average = document_->partialAverageAmplitude(partial);
        dragStartGains_.push_back(document_->partialGain(partial));
        dragNaturalHeights_.push_back(
            std::max(2.0, (average / maxAverage) * innerRect().height())
        );
    }

    event->accept();
}

void PartialMixer::mouseMoveEvent(QMouseEvent* event)
{
    if (draggingPartial_ < 0)
        return;

    if (!levelDragStarted_ && (event->position() - pressPos_).manhattanLength() >= 3.0)
        levelDragStarted_ = true;

    if (levelDragStarted_)
        editAt(event->position(), event->modifiers(), false);
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
