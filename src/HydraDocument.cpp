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

#include "HydraDocument.hpp"

#include <QDir>
#include <QtCore/QFileInfo>

#include <algorithm>
#include <cmath>
#include <utility>

namespace hydra2 {
namespace {

const QVector<Breakpoint> emptyTrack;

bool containsRef(const QVector<PointRef>& refs, const PointRef& ref)
{
    return std::find(refs.cbegin(), refs.cend(), ref) != refs.cend();
}

} // namespace

HydraDocument::HydraDocument(QObject* parent)
    : QObject(parent)
{
}

QString HydraDocument::fileName() const
{
    return path_.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(path_).fileName();
}

QUrl HydraDocument::fileUrl() const
{
    return path_.isEmpty() ? QUrl{} : QUrl::fromLocalFile(path_);
}

int HydraDocument::durationMs() const
{
    int maximum = 0;
    for (const auto& partial : data_.partials) {
        for (const auto& point : partial.amplitude)
            maximum = std::max(maximum, point.timeMs);
        for (const auto& point : partial.frequency)
            maximum = std::max(maximum, point.timeMs);
    }
    return maximum;
}

const QVector<Breakpoint>& HydraDocument::track(int partial, TrackKind kind) const
{
    if (partial < 0 || partial >= data_.partials.size())
        return emptyTrack;
    return kind == TrackKind::Amplitude
        ? data_.partials.at(partial).amplitude
        : data_.partials.at(partial).frequency;
}

QVector<Breakpoint>& HydraDocument::mutableTrack(int partial, TrackKind kind)
{
    Q_ASSERT(partial >= 0 && partial < data_.partials.size());
    return kind == TrackKind::Amplitude
        ? data_.partials[partial].amplitude
        : data_.partials[partial].frequency;
}

double HydraDocument::displayValue(int partial, TrackKind kind, const Breakpoint& point) const
{
    if (kind == TrackKind::Amplitude && partial >= 0 && partial < data_.partials.size())
        return point.value * data_.partials.at(partial).gain;
    return point.value;
}

double HydraDocument::partialAverageAmplitude(int partial) const
{
    if (partial < 0 || partial >= data_.partials.size())
        return 0.0;

    const auto& points = data_.partials.at(partial).amplitude;
    if (points.isEmpty())
        return 0.0;
    if (points.size() == 1)
        return points.first().value;

    double area = 0.0;
    int span = 0;
    for (qsizetype i = 1; i < points.size(); ++i) {
        const int dt = std::max(0, points.at(i).timeMs - points.at(i - 1).timeMs);
        area += 0.5 * (points.at(i - 1).value + points.at(i).value) * dt;
        span += dt;
    }
    return span > 0 ? area / span : points.first().value;
}

double HydraDocument::partialGain(int partial) const
{
    if (partial < 0 || partial >= data_.partials.size())
        return 0.0;
    return data_.partials.at(partial).gain;
}

bool HydraDocument::isPointSelected(const PointRef& ref) const
{
    return containsRef(selectedPoints_, ref);
}

const Breakpoint* HydraDocument::point(const PointRef& ref) const
{
    const auto& points = track(ref.partial, ref.kind);
    const auto it = std::find_if(points.cbegin(), points.cend(),
                                 [&](const Breakpoint& p) { return p.id == ref.id; });
    return it == points.cend() ? nullptr : &*it;
}

Breakpoint* HydraDocument::mutablePoint(const PointRef& ref)
{
    if (ref.partial < 0 || ref.partial >= data_.partials.size())
        return nullptr;
    auto& points = mutableTrack(ref.partial, ref.kind);
    const auto it = std::find_if(points.begin(), points.end(),
                                 [&](const Breakpoint& p) { return p.id == ref.id; });
    return it == points.end() ? nullptr : &*it;
}

double HydraDocument::partialMaximumGain(int partial) const
{
    if (partial < 0 || partial >= data_.partials.size())
        return 1.0;

    const auto& points = data_.partials.at(partial).amplitude;

    int peak = 0;
    for (const auto& point : points)
        peak = std::max(peak, point.value);

    if (peak <= 0)
        return 1.0;

    return 32767.0 / static_cast<double>(peak);
}


bool HydraDocument::openUrl(const QUrl& url)
{
    const QString path = url.toLocalFile();
    if (path.isEmpty()) {
        setError(QStringLiteral("The selected URL is not a local file."));
        return false;
    }

    HetData loaded;
    QString error;
    if (!HetFile::load(path, loaded, error)) {
        setError(error);
        return false;
    }

    data_ = std::move(loaded);
    path_ = path;
    selectedPartials_.clear();
    selectedPoints_.clear();
    setDirty(false);
    setError({});
    emit fileChanged();
    emit documentReset();
    emit selectionChanged();
    return true;
}

bool HydraDocument::save()
{
    if (path_.isEmpty()) {
        setError(QStringLiteral("Choose a filename with Save As first."));
        return false;
    }
    return saveToPath(path_, data_.format);
}

bool HydraDocument::saveAs(const QUrl& url, int formatIndex)
{
    QString path = url.toLocalFile();

    if (formatIndex == 1) {
        // Legacy ADS binary.
        QFileInfo info(path);
        path = info.dir().filePath(info.completeBaseName() + QStringLiteral(".ads"));

        return saveToPath(
            path,
            HetFormat::LegacyBinaryLittleEndian
            );
    }

    // Current HETRO text.
    QFileInfo info(path);
    path = info.dir().filePath(info.completeBaseName() + QStringLiteral(".het"));

    return saveToPath(
        path,
        HetFormat::CurrentText
        );
}

bool HydraDocument::saveToPath(const QString& path, HetFormat format)
{
    bool withHeader = data_.binaryHasHeader;

    // When converting a text HETRO file to legacy binary, write the
    // partial-count header. For existing binary files, preserve the
    // header style that was loaded.
    if (format != HetFormat::CurrentText &&
        data_.format == HetFormat::CurrentText) {
        withHeader = true;
    }

    QString error;
    if (!HetFile::save(path, data_, format, withHeader, error)) {
        setError(error);
        return false;
    }

    path_ = path;
    data_.format = format;
    data_.binaryHasHeader = withHeader;
    setDirty(false);
    setError({});
    emit fileChanged();
    return true;
}

HetFormat HydraDocument::formatForPath(const QString& path) const
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("ads")) {
        if (data_.format == HetFormat::LegacyBinaryBigEndian)
            return HetFormat::LegacyBinaryBigEndian;
        return HetFormat::LegacyBinaryLittleEndian;
    }
    return HetFormat::CurrentText;
}

void HydraDocument::normalizeAmplitudes()
{
    int peak = 0;

    // Normalize the raw amplitude breakpoints only. Mixer gains stay
    // non-destructive, so a muted partial remains recoverable after Normalize.
    for (const auto& partial : data_.partials) {
        for (const auto& point : partial.amplitude)
            peak = std::max(peak, point.value);
    }

    if (peak <= 0 || peak == 32767)
        return;

    const double scale = 32767.0 / static_cast<double>(peak);
    bool changed = false;

    for (int partialIndex = 0; partialIndex < data_.partials.size(); ++partialIndex) {
        auto& partial = data_.partials[partialIndex];

        for (auto& point : partial.amplitude) {
            const int normalized = std::clamp(
                static_cast<int>(std::lround(point.value * scale)),
                0,
                32767
            );
            changed |= (normalized != point.value);
            point.value = normalized;
        }

        // Normalization may reduce the remaining amplification headroom.
        // Keep the existing mixer setting when possible, but never leave a
        // gain above the new legal maximum for this partial.
        const double clampedGain = std::clamp(
            partial.gain,
            0.0,
            partialMaximumGain(partialIndex)
        );
        if (!qFuzzyCompare(partial.gain + 1.0, clampedGain + 1.0)) {
            partial.gain = clampedGain;
            changed = true;
        }
    }

    if (!changed)
        return;

    setDirty(true);
    emit dataChanged();

    QVector<int> changedPartials;
    changedPartials.reserve(data_.partials.size());
    for (int partial = 0; partial < data_.partials.size(); ++partial)
        changedPartials.push_back(partial);
    emit rawPartialsChanged(changedPartials);
    emit partialsChanged(changedPartials);
}

void HydraDocument::setPartialGain(int partial, double gain)
{
    if (partial < 0 || partial >= data_.partials.size())
        return;

    gain = std::clamp(gain, 0.0, partialMaximumGain(partial));
    if (qFuzzyCompare(data_.partials.at(partial).gain + 1.0, gain + 1.0))
        return;

    data_.partials[partial].gain = gain;
    setDirty(true);
    emit dataChanged();
    emit partialsChanged(QVector<int>{partial});
}

void HydraDocument::setPartialGains(const QVector<int>& partials,
                                    const QVector<double>& gains)
{
    const qsizetype count = std::min(partials.size(), gains.size());
    bool changed = false;
    QVector<int> changedPartials;
    changedPartials.reserve(count);

    for (qsizetype i = 0; i < count; ++i) {
        const int partial = partials.at(i);
        if (partial < 0 || partial >= data_.partials.size())
            continue;

        const double gain = std::clamp(
            gains.at(i),
            0.0,
            partialMaximumGain(partial)
        );

        if (qFuzzyCompare(data_.partials.at(partial).gain + 1.0, gain + 1.0))
            continue;

        data_.partials[partial].gain = gain;
        changed = true;
        if (!changedPartials.contains(partial))
            changedPartials.push_back(partial);
    }

    if (!changed)
        return;

    setDirty(true);
    emit dataChanged();
    emit partialsChanged(changedPartials);
}

void HydraDocument::setPartialSelection(int partial, bool additive, bool toggle)
{
    if (partial < 0 || partial >= data_.partials.size())
        return;

    if (!additive) {
        selectedPartials_.clear();
        selectedPoints_.clear();
    }

    if (toggle && selectedPartials_.contains(partial)) {
        selectedPartials_.remove(partial);
        selectedPoints_.erase(std::remove_if(selectedPoints_.begin(), selectedPoints_.end(),
                                             [&](const PointRef& ref) { return ref.partial == partial; }),
                              selectedPoints_.end());
    } else {
        selectedPartials_.insert(partial);
    }
    emit selectionChanged();
}

void HydraDocument::selectWholePartial(int partial, bool additive, bool toggle)
{
    if (partial < 0 || partial >= data_.partials.size())
        return;

    const bool wasSelected = selectedPartials_.contains(partial);

    if (!additive) {
        selectedPartials_.clear();
        selectedPoints_.clear();
    }

    if (toggle && wasSelected) {
        selectedPartials_.remove(partial);
        selectedPoints_.erase(std::remove_if(selectedPoints_.begin(), selectedPoints_.end(),
                                             [&](const PointRef& ref) {
                                                 return ref.partial == partial;
                                             }),
                              selectedPoints_.end());
        emit selectionChanged();
        return;
    }

    selectedPartials_.insert(partial);

    const auto addTrack = [&](TrackKind kind) {
        for (const auto& point : track(partial, kind)) {
            const PointRef ref{partial, kind, point.id};
            if (!containsRef(selectedPoints_, ref))
                selectedPoints_.push_back(ref);
        }
    };

    addTrack(TrackKind::Amplitude);
    addTrack(TrackKind::Frequency);

    emit selectionChanged();
}

void HydraDocument::setPointSelection(const QVector<PointRef>& refs, bool additive)
{
    if (!additive) {
        selectedPoints_.clear();
        selectedPartials_.clear();
    }

    for (const auto& ref : refs) {
        if (!containsRef(selectedPoints_, ref))
            selectedPoints_.push_back(ref);
        selectedPartials_.insert(ref.partial);
    }
    emit selectionChanged();
}

void HydraDocument::selectPoint(const PointRef& ref, bool additive, bool toggle)
{
    if (!additive) {
        selectedPoints_.clear();
        selectedPartials_.clear();
    }

    const auto it = std::find(selectedPoints_.begin(), selectedPoints_.end(), ref);
    if (toggle && it != selectedPoints_.end()) {
        selectedPoints_.erase(it);
        bool stillHasPartial = false;
        for (const auto& item : std::as_const(selectedPoints_)) {
            if (item.partial == ref.partial) {
                stillHasPartial = true;
                break;
            }
        }
        if (!stillHasPartial)
            selectedPartials_.remove(ref.partial);
    } else if (it == selectedPoints_.end()) {
        selectedPoints_.push_back(ref);
        selectedPartials_.insert(ref.partial);
    }
    emit selectionChanged();
}

void HydraDocument::clearSelection()
{
    if (selectedPoints_.isEmpty() && selectedPartials_.isEmpty())
        return;
    selectedPoints_.clear();
    selectedPartials_.clear();
    emit selectionChanged();
}

bool HydraDocument::isWholePartialSelected(int partial) const
{
    if (partial < 0 || partial >= data_.partials.size())
        return false;

    bool hasPoints = false;
    for (TrackKind kind : {TrackKind::Amplitude, TrackKind::Frequency}) {
        for (const auto& point : track(partial, kind)) {
            hasPoints = true;
            if (!containsRef(selectedPoints_, PointRef{partial, kind, point.id}))
                return false;
        }
    }
    return hasPoints;
}

quint64 HydraDocument::nextBreakpointId() const
{
    quint64 maximum = 0;
    for (const auto& partial : data_.partials) {
        for (const auto& point : partial.amplitude)
            maximum = std::max(maximum, point.id);
        for (const auto& point : partial.frequency)
            maximum = std::max(maximum, point.id);
    }
    return maximum + 1;
}

void HydraDocument::rebuildWholePartialSelection(int partial)
{
    selectedPoints_.erase(std::remove_if(selectedPoints_.begin(), selectedPoints_.end(),
                                         [&](const PointRef& ref) {
                                             return ref.partial == partial;
                                         }),
                          selectedPoints_.end());

    for (TrackKind kind : {TrackKind::Amplitude, TrackKind::Frequency}) {
        for (const auto& point : track(partial, kind))
            selectedPoints_.push_back({partial, kind, point.id});
    }
    selectedPartials_.insert(partial);
}

bool HydraDocument::translateWholePartial(int partial, int deltaTimeMs)
{
    if (partial < 0 || partial >= data_.partials.size() || deltaTimeMs == 0)
        return false;

    // Work out one legal delta for both ADSYN tracks so amplitude and
    // frequency stay aligned. The first t=0 point is a permanent anchor.
    int minimumDelta = -32766;
    int maximumDelta = 32766;
    bool hasMovablePoint = false;

    for (TrackKind kind : {TrackKind::Amplitude, TrackKind::Frequency}) {
        const auto& points = track(partial, kind);
        if (points.isEmpty())
            continue;

        const int anchorValue = points.first().value;
        int firstDifferentTime = -1;

        for (qsizetype i = 1; i < points.size(); ++i) {
            hasMovablePoint = true;
            maximumDelta = std::min(maximumDelta, 32766 - points.at(i).timeMs);
            if (firstDifferentTime < 0 && points.at(i).value != anchorValue)
                firstDifferentTime = points.at(i).timeMs;
        }

        // Initial points having the same value as the t=0 anchor are just a
        // hold and may collapse back into the anchor. A point with a different
        // value must remain at least 1 ms after t=0.
        if (firstDifferentTime >= 0)
            minimumDelta = std::max(minimumDelta, 1 - firstDifferentTime);
    }

    if (!hasMovablePoint)
        return false;

    const int delta = std::clamp(deltaTimeMs, minimumDelta, maximumDelta);
    if (delta == 0)
        return false;

    bool changed = false;

    for (TrackKind kind : {TrackKind::Amplitude, TrackKind::Frequency}) {
        auto& points = mutableTrack(partial, kind);
        if (points.isEmpty())
            continue;

        const int anchorValue = points.first().value;

        // If the trajectory currently starts immediately by moving away from
        // the anchor, shifting right needs a duplicate anchor at the amount of
        // delay. This keeps the original first segment's duration unchanged.
        const bool needsHoldPoint =
            delta > 0 &&
            points.size() > 1 &&
            points.at(1).value != anchorValue;

        for (qsizetype i = 1; i < points.size(); ++i) {
            points[i].timeMs += delta;
            changed = true;
        }

        // Initial hold points can disappear into the permanent zero-time
        // anchor when a delayed partial is moved back to the left.
        while (points.size() > 1 &&
               points.at(1).timeMs <= 0 &&
               points.at(1).value == anchorValue) {
            points.removeAt(1);
            changed = true;
        }

        if (needsHoldPoint) {
            const int holdTime = delta;
            if (holdTime > 0 && holdTime < points.at(1).timeMs) {
                points.insert(1, Breakpoint{nextBreakpointId(), holdTime, anchorValue});
                changed = true;
            }
        }

        std::stable_sort(points.begin(), points.end(),
                         [](const Breakpoint& a, const Breakpoint& b) {
                             return a.timeMs < b.timeMs;
                         });
    }

    if (changed)
        rebuildWholePartialSelection(partial);

    return changed;
}

int HydraDocument::snappedDeltaTime(int requestedDeltaTimeMs,
                                    TrackKind kind,
                                    const QSet<int>& excludedPartials) const
{
    if (requestedDeltaTimeMs == 0)
        return 0;

    int minimumDelta = -32766;
    int maximumDelta = 32766;
    bool hasMovableSelection = false;

    for (int partial = 0; partial < data_.partials.size(); ++partial) {
        if (excludedPartials.contains(partial))
            continue;

        const auto& points = track(partial, kind);
        if (points.isEmpty())
            continue;

        QVector<bool> movable(points.size(), false);
        for (qsizetype i = 0; i < points.size(); ++i) {
            const PointRef ref{partial, kind, points.at(i).id};
            movable[i] = points.at(i).timeMs > 0 && containsRef(selectedPoints_, ref);
            hasMovableSelection |= movable[i];
        }

        for (qsizetype i = 0; i < points.size(); ++i) {
            if (!movable.at(i))
                continue;

            qsizetype previous = i - 1;
            while (previous >= 0 && movable.at(previous))
                --previous;

            qsizetype next = i + 1;
            while (next < points.size() && movable.at(next))
                ++next;

            const int lowerBoundary =
                previous >= 0 ? points.at(previous).timeMs + 1 : 1;
            const int upperBoundary =
                next < points.size() ? points.at(next).timeMs - 1 : 32766;

            minimumDelta = std::max(minimumDelta,
                                    lowerBoundary - points.at(i).timeMs);
            maximumDelta = std::min(maximumDelta,
                                    upperBoundary - points.at(i).timeMs);
        }
    }

    if (!hasMovableSelection)
        return 0;

    return std::clamp(requestedDeltaTimeMs, minimumDelta, maximumDelta);
}

void HydraDocument::moveSelected(int deltaTimeMs,
                                 int deltaDisplayValue,
                                 TrackKind kind,
                                 bool logicSnap)
{
    if (deltaTimeMs == 0 && deltaDisplayValue == 0)
        return;

    // A stroke click selects both tracks of a partial. Treat horizontal
    // movement of such a selection as a true whole-partial translation:
    // amplitude and frequency move together while their t=0 anchors stay put.
    QSet<int> wholePartials;
    for (int partial : std::as_const(selectedPartials_)) {
        if (isWholePartialSelected(partial))
            wholePartials.insert(partial);
    }

    bool changed = false;
    QSet<int> touchedPartials;

    if (deltaTimeMs != 0) {
        for (int partial : std::as_const(wholePartials)) {
            if (translateWholePartial(partial, deltaTimeMs)) {
                changed = true;
                touchedPartials.insert(partial);
            }
        }
    }

    int ordinaryDeltaTime = deltaTimeMs;
    if (logicSnap)
        ordinaryDeltaTime = snappedDeltaTime(deltaTimeMs, kind, wholePartials);

    for (const auto& ref : std::as_const(selectedPoints_)) {
        if (ref.kind != kind)
            continue;

        Breakpoint* p = mutablePoint(ref);
        if (!p)
            continue;

        const int oldTime = p->timeMs;
        const int oldValue = p->value;

        // Whole-partial horizontal movement has already been applied to both
        // tracks above. For normal point editing the t=0 anchor is permanent.
        if (!wholePartials.contains(ref.partial) && p->timeMs > 0) {
            p->timeMs = std::clamp(p->timeMs + ordinaryDeltaTime, 1, 32766);
        }

        if (deltaDisplayValue != 0) {
            if (kind == TrackKind::Amplitude) {
                const double gain = partialGain(ref.partial);
                if (gain > 0.000001) {
                    const int rawDelta =
                        static_cast<int>(std::lround(deltaDisplayValue / gain));
                    p->value = std::clamp(p->value + rawDelta, 0, 32767);
                }
            } else {
                p->value = std::clamp(p->value + deltaDisplayValue, 0, 32767);
            }
        }

        changed |= (oldTime != p->timeMs || oldValue != p->value);
        touchedPartials.insert(ref.partial);
    }

    if (!changed)
        return;

    // Logic Snap preserves point topology. When it is disabled, keep the file
    // valid by sorting crossed points into chronological order.
    if (!logicSnap) {
        for (int partial : std::as_const(touchedPartials)) {
            if (wholePartials.contains(partial))
                continue;
            auto& points = mutableTrack(partial, kind);
            std::stable_sort(points.begin(), points.end(),
                             [](const Breakpoint& a, const Breakpoint& b) {
                                 return a.timeMs < b.timeMs;
                             });
        }
    }

    setDirty(true);
    emit dataChanged();

    QVector<int> changedPartials = touchedPartials.values();
    std::sort(changedPartials.begin(), changedPartials.end());
    emit rawPartialsChanged(changedPartials);
    emit partialsChanged(changedPartials);
}

void HydraDocument::setDirty(bool value)
{
    if (dirty_ == value)
        return;
    dirty_ = value;
    emit dirtyChanged();
}

void HydraDocument::setError(const QString& error)
{
    if (errorString_ == error)
        return;
    errorString_ = error;
    emit errorStringChanged();
}

} // namespace hydra2
