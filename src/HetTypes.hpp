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
