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

void HydraDocument::setPartialGain(int partial, double gain)
{
    if (partial < 0 || partial >= data_.partials.size())
        return;
    gain = std::clamp(
        gain,
        0.0,
        partialMaximumGain(partial)
        );
    if (qFuzzyCompare(data_.partials.at(partial).gain + 1.0, gain + 1.0))
        return;

    data_.partials[partial].gain = gain;
    setDirty(true);
    emit dataChanged();
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

void HydraDocument::moveSelected(int deltaTimeMs, int deltaDisplayValue, TrackKind kind)
{
    if (deltaTimeMs == 0 && deltaDisplayValue == 0)
        return;

    QSet<int> touchedPartials;
    bool changed = false;
    for (const auto& ref :  std::as_const(selectedPoints_)) {
        if (ref.kind != kind)
            continue;
        Breakpoint* p = mutablePoint(ref);
        if (!p)
            continue;

        const int oldTime = p->timeMs;
        const int oldValue = p->value;
        p->timeMs = std::clamp(p->timeMs + deltaTimeMs, 0, 32766);

        if (kind == TrackKind::Amplitude) {
            const double gain = partialGain(ref.partial);
            if (gain > 0.000001) {
                const int rawDelta = static_cast<int>(std::lround(deltaDisplayValue / gain));
                p->value = std::clamp(p->value + rawDelta, 0, 32767);
            }
        } else {
            p->value = std::clamp(p->value + deltaDisplayValue, 0, 32767);
        }

        changed |= (oldTime != p->timeMs || oldValue != p->value);
        touchedPartials.insert(ref.partial);
    }

    if (!changed)
        return;

    for (int partial : touchedPartials) {
        auto& points = mutableTrack(partial, kind);
        std::stable_sort(points.begin(), points.end(),
                         [](const Breakpoint& a, const Breakpoint& b) {
                             return a.timeMs < b.timeMs;
                         });
    }

    setDirty(true);
    emit dataChanged();
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
