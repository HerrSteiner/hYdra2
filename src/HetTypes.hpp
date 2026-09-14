#pragma once

#include <QtCore/QVector>
#include <QtCore/QtGlobal>

namespace hydra2 {

enum class TrackKind {
    Amplitude = 0,
    Frequency = 1
};

struct Breakpoint {
    quint64 id = 0;
    int timeMs = 0;
    int value = 0;
};

struct Partial {
    QVector<Breakpoint> amplitude;
    QVector<Breakpoint> frequency;

    // Non-destructive mixer attenuation. It is multiplied into amplitude
    // values on save, but the underlying breakpoints remain intact in memory.
    double gain = 1.0;
};

struct PointRef {
    int partial = -1;
    TrackKind kind = TrackKind::Amplitude;
    quint64 id = 0;

    friend bool operator==(const PointRef&, const PointRef&) = default;
};

} // namespace hydra2
