#pragma once

#include "HetFile.hpp"

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

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QUrl>

namespace hydra2 {

class HydraDocument final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileChanged)
    Q_PROPERTY(QUrl fileUrl READ fileUrl NOTIFY fileChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY documentReset)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(int partialCount READ partialCount NOTIFY documentReset)
    Q_PROPERTY(int durationMs READ durationMs NOTIFY dataChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    explicit HydraDocument(QObject* parent = nullptr);

    QString fileName() const;
    QUrl fileUrl() const;
    bool loaded() const { return !data_.partials.isEmpty(); }
    bool dirty() const { return dirty_; }
    int partialCount() const { return data_.partials.size(); }
    int durationMs() const;
    QString errorString() const { return errorString_; }

    const QVector<Partial>& partials() const { return data_.partials; }
    const QVector<Breakpoint>& track(int partial, TrackKind kind) const;
    double displayValue(int partial, TrackKind kind, const Breakpoint& point) const;
    double partialAverageAmplitude(int partial) const;
    double partialGain(int partial) const;
    double partialMaximumGain(int partial) const;

    const QSet<int>& selectedPartials() const { return selectedPartials_; }
    const QVector<PointRef>& selectedPoints() const { return selectedPoints_; }
    bool isPointSelected(const PointRef& ref) const;
    const Breakpoint* point(const PointRef& ref) const;

    Q_INVOKABLE bool openUrl(const QUrl& url);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool saveAs(const QUrl& url, int formatIndex);
    Q_INVOKABLE void normalizeAmplitudes();

    void setPartialGain(int partial, double gain);
    void setPartialGains(const QVector<int>& partials, const QVector<double>& gains);
    void setPartialSelection(int partial, bool additive = false, bool toggle = false);
    void selectWholePartial(int partial, bool additive = false, bool toggle = false);
    void setPointSelection(const QVector<PointRef>& refs, bool additive = false);
    void selectPoint(const PointRef& ref, bool additive = false, bool toggle = false);
    void clearSelection();
    void moveSelected(int deltaTimeMs, int deltaDisplayValue, TrackKind kind, bool logicSnap = true);

signals:
    void documentReset();
    void dataChanged();
    // Identifies partials whose breakpoint/display data changed. This lets
    // GPU-backed views refresh only the affected series instead of rebuilding
    // every partial for each mouse move.
    void partialsChanged(const QVector<int>& partials);
    // Raw breakpoint data changed. Mixer uses this to refresh its cached
    // natural levels; gain-only changes intentionally do not emit it.
    void rawPartialsChanged(const QVector<int>& partials);
    void selectionChanged();
    void fileChanged();
    void dirtyChanged();
    void errorStringChanged();

private:
    Breakpoint* mutablePoint(const PointRef& ref);
    QVector<Breakpoint>& mutableTrack(int partial, TrackKind kind);
    void setDirty(bool value);
    void setError(const QString& error);
    bool saveToPath(const QString& path, HetFormat format);
    HetFormat formatForPath(const QString& path) const;
    bool isWholePartialSelected(int partial) const;
    bool translateWholePartial(int partial, int deltaTimeMs);
    int snappedDeltaTime(int requestedDeltaTimeMs, TrackKind kind,
                         const QSet<int>& excludedPartials) const;
    quint64 nextBreakpointId() const;
    void rebuildWholePartialSelection(int partial);

    HetData data_;
    QString path_;
    QString errorString_;
    bool dirty_ = false;
    QSet<int> selectedPartials_;
    QVector<PointRef> selectedPoints_;
};

} // namespace hydra2
