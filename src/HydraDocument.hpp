#pragma once

#include "HetFile.hpp"

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

    void setPartialGain(int partial, double gain);
    void setPartialSelection(int partial, bool additive = false, bool toggle = false);
    void setPointSelection(const QVector<PointRef>& refs, bool additive = false);
    void selectPoint(const PointRef& ref, bool additive = false, bool toggle = false);
    void clearSelection();
    void moveSelected(int deltaTimeMs, int deltaDisplayValue, TrackKind kind);

signals:
    void documentReset();
    void dataChanged();
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

    HetData data_;
    QString path_;
    QString errorString_;
    bool dirty_ = false;
    QSet<int> selectedPartials_;
    QVector<PointRef> selectedPoints_;
};

} // namespace hydra2
