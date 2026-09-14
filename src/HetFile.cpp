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

#include "HetFile.hpp"

#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>

#include <algorithm>
#include <cmath>
#include <limits>

namespace hydra2 {
namespace {

struct ParsedTrack {
    TrackKind kind = TrackKind::Amplitude;
    QVector<Breakpoint> points;
};

quint64 nextId()
{
    static quint64 id = 1;
    return id++;
}

int clampWordValue(int value)
{
    return std::clamp(value, 0, 32767);
}

int bakedAmplitude(const Partial& partial, int value)
{
    return clampWordValue(static_cast<int>(std::lround(value * partial.gain)));
}

bool pairTracks(const QVector<ParsedTrack>& tracks,
                int declaredPartials,
                QVector<Partial>& partials,
                QString& error)
{
    QVector<QVector<Breakpoint>> amplitudes;
    QVector<QVector<Breakpoint>> frequencies;

    for (const auto& track : tracks) {
        if (track.kind == TrackKind::Amplitude)
            amplitudes.push_back(track.points);
        else
            frequencies.push_back(track.points);
    }

    if (amplitudes.isEmpty() && frequencies.isEmpty()) {
        error = QStringLiteral("No HETRO tracks found.");
        return false;
    }
    if (amplitudes.size() != frequencies.size()) {
        error = QStringLiteral("The file has %1 amplitude tracks but %2 frequency tracks.")
                    .arg(amplitudes.size())
                    .arg(frequencies.size());
        return false;
    }
    if (declaredPartials > 0 && declaredPartials != amplitudes.size()) {
        error = QStringLiteral("HETRO header declares %1 partials, but %2 track pairs were found.")
                    .arg(declaredPartials)
                    .arg(amplitudes.size());
        return false;
    }

    partials.clear();
    partials.reserve(amplitudes.size());
    for (qsizetype i = 0; i < amplitudes.size(); ++i) {
        Partial p;
        p.amplitude = amplitudes.at(i);
        p.frequency = frequencies.at(i);
        p.gain = 1.0;
        partials.push_back(std::move(p));
    }
    return true;
}

bool parseTrackNumbers(const QVector<int>& values, ParsedTrack& track, QString& error)
{
    if (values.isEmpty() || (values.first() != -1 && values.first() != -2)) {
        error = QStringLiteral("Track does not start with -1 or -2.");
        return false;
    }

    track.kind = values.first() == -1 ? TrackKind::Amplitude : TrackKind::Frequency;
    int i = 1;
    int previousTime = -1;

    while (i < values.size()) {
        const int time = values.at(i++);
        if (time == 32767)
            break;
        if (time < 0 || time > 32766) {
            error = QStringLiteral("Invalid breakpoint time %1.").arg(time);
            return false;
        }
        if (i >= values.size()) {
            error = QStringLiteral("Breakpoint time %1 has no value.").arg(time);
            return false;
        }

        // 32767 is legal in the value position even though it also acts as
        // the end marker when encountered in the time position.
        const int value = values.at(i++);
        if (value < 0 || value > 32767) {
            error = QStringLiteral("Invalid breakpoint value %1.").arg(value);
            return false;
        }
        if (time < previousTime) {
            error = QStringLiteral("Breakpoint times are not monotonically increasing.");
            return false;
        }
        previousTime = time;
        track.points.push_back({nextId(), time, value});
    }

    if (track.points.isEmpty()) {
        error = QStringLiteral("Empty HETRO track.");
        return false;
    }
    return true;
}

bool parseText(const QByteArray& bytes, HetData& out, QString& error)
{
    QString text = QString::fromUtf8(bytes);
    if (!text.isEmpty() && text.front() == QChar(0xfeff))
        text.removeFirst();

    const QStringList lines = text.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    if (lines.isEmpty()) {
        error = QStringLiteral("Empty file.");
        return false;
    }

    const QRegularExpression headerRx(QStringLiteral("^\\s*HETRO(?:\\s+(\\d+))?\\s*$"),
                                      QRegularExpression::CaseInsensitiveOption);
    const auto match = headerRx.match(lines.first());
    if (!match.hasMatch()) {
        error = QStringLiteral("Text file has no HETRO header.");
        return false;
    }

    int declaredPartials = 0;
    if (!match.captured(1).isEmpty())
        declaredPartials = match.captured(1).toInt();

    QVector<ParsedTrack> tracks;
    for (qsizetype lineIndex = 1; lineIndex < lines.size(); ++lineIndex) {
        const QString line = lines.at(lineIndex).trimmed();
        if (line.isEmpty())
            continue;

        const QStringList tokens = line.split(QRegularExpression(QStringLiteral("[,\\s]+")),
                                              Qt::SkipEmptyParts);
        QVector<int> values;
        values.reserve(tokens.size());
        for (const QString& token : tokens) {
            bool ok = false;
            const int value = token.toInt(&ok);
            if (!ok) {
                error = QStringLiteral("Invalid integer '%1' on line %2.")
                            .arg(token)
                            .arg(lineIndex + 1);
                return false;
            }
            values.push_back(value);
        }

        // Current HETRO text includes 32767 at the end of each track. Files
        // intended for het_import may omit it because newline also terminates
        // a track. parseTrackNumbers accepts either form.
        ParsedTrack track;
        if (!parseTrackNumbers(values, track, error)) {
            error = QStringLiteral("Line %1: %2").arg(lineIndex + 1).arg(error);
            return false;
        }
        tracks.push_back(std::move(track));
    }

    if (!pairTracks(tracks, declaredPartials, out.partials, error))
        return false;

    out.format = HetFormat::CurrentText;
    out.binaryHasHeader = true;
    return true;
}

QVector<qint16> decodeWords(const QByteArray& bytes, bool littleEndian)
{
    QVector<qint16> words;
    words.reserve(bytes.size() / 2);
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.constData());
    for (qsizetype i = 0; i + 1 < bytes.size(); i += 2) {
        quint16 u = littleEndian
            ? static_cast<quint16>(p[i] | (static_cast<quint16>(p[i + 1]) << 8))
            : static_cast<quint16>((static_cast<quint16>(p[i]) << 8) | p[i + 1]);
        words.push_back(static_cast<qint16>(u));
    }
    return words;
}

struct BinaryCandidate {
    bool valid = false;
    int score = std::numeric_limits<int>::min();
    HetData data;
};

BinaryCandidate parseBinaryCandidate(const QVector<qint16>& words,
                                     bool littleEndian,
                                     bool withHeader)
{
    BinaryCandidate result;
    if (words.isEmpty())
        return result;

    int index = 0;
    int declaredPartials = 0;
    if (withHeader) {
        declaredPartials = words.at(index++);
        if (declaredPartials <= 0 || declaredPartials > 32766)
            return result;
    }

    QVector<ParsedTrack> tracks;
    while (index < words.size()) {
        const int marker = words.at(index++);
        if (marker != -1 && marker != -2)
            return result;

        QVector<int> values;
        values.push_back(marker);
        bool ended = false;
        int previousTime = -1;
        int points = 0;

        while (index < words.size()) {
            const int time = static_cast<int>(words.at(index++));
            values.push_back(time);
            if (time == 32767) {
                ended = true;
                break;
            }
            if (time < 0 || time > 32766 || time < previousTime)
                return result;
            previousTime = time;
            if (index >= words.size())
                return result;
            const int value = static_cast<int>(words.at(index++));
            if (value < 0)
                return result;
            values.push_back(value);
            ++points;
        }

        if (!ended || points == 0)
            return result;

        ParsedTrack track;
        QString trackError;
        if (!parseTrackNumbers(values, track, trackError))
            return result;
        tracks.push_back(std::move(track));
    }

    QString pairError;
    QVector<Partial> partials;
    if (!pairTracks(tracks, declaredPartials, partials, pairError))
        return result;

    result.valid = true;
    result.score = 100 + tracks.size() * 2 + (withHeader ? 20 : 0);
    result.data.partials = std::move(partials);
    result.data.format = littleEndian
        ? HetFormat::LegacyBinaryLittleEndian
        : HetFormat::LegacyBinaryBigEndian;
    result.data.binaryHasHeader = withHeader;
    return result;
}

bool parseBinary(const QByteArray& bytes, HetData& out, QString& error)
{
    if (bytes.size() < 4 || (bytes.size() % 2) != 0) {
        error = QStringLiteral("Legacy HETRO binary file has an invalid byte length.");
        return false;
    }

    QVector<BinaryCandidate> candidates;
    const auto little = decodeWords(bytes, true);
    const auto big = decodeWords(bytes, false);
    candidates << parseBinaryCandidate(little, true, true)
               << parseBinaryCandidate(little, true, false)
               << parseBinaryCandidate(big, false, true)
               << parseBinaryCandidate(big, false, false);

    const BinaryCandidate* best = nullptr;
    for (const auto& candidate : candidates) {
        if (candidate.valid && (!best || candidate.score > best->score))
            best = &candidate;
    }

    if (!best) {
        error = QStringLiteral("The file is neither current HETRO text nor a recognized legacy HETRO binary file.");
        return false;
    }

    out = best->data;
    return true;
}

void appendWord(QByteArray& bytes, qint16 value, bool littleEndian)
{
    const quint16 u = static_cast<quint16>(value);
    if (littleEndian) {
        bytes.append(static_cast<char>(u & 0xff));
        bytes.append(static_cast<char>((u >> 8) & 0xff));
    } else {
        bytes.append(static_cast<char>((u >> 8) & 0xff));
        bytes.append(static_cast<char>(u & 0xff));
    }
}

void appendBinaryTrack(QByteArray& bytes,
                       const Partial& partial,
                       TrackKind kind,
                       bool littleEndian)
{
    appendWord(bytes, kind == TrackKind::Amplitude ? -1 : -2, littleEndian);
    const auto& points = kind == TrackKind::Amplitude ? partial.amplitude : partial.frequency;
    for (const auto& point : points) {
        appendWord(bytes, static_cast<qint16>(std::clamp(point.timeMs, 0, 32766)), littleEndian);
        const int value = kind == TrackKind::Amplitude
            ? bakedAmplitude(partial, point.value)
            : clampWordValue(point.value);
        appendWord(bytes, static_cast<qint16>(value), littleEndian);
    }
    appendWord(bytes, 32767, littleEndian);
}

QString textTrack(const Partial& partial, TrackKind kind)
{
    QStringList fields;
    fields << (kind == TrackKind::Amplitude ? QStringLiteral("-1") : QStringLiteral("-2"));
    const auto& points = kind == TrackKind::Amplitude ? partial.amplitude : partial.frequency;
    for (const auto& point : points) {
        fields << QString::number(std::clamp(point.timeMs, 0, 32766));
        const int value = kind == TrackKind::Amplitude
            ? bakedAmplitude(partial, point.value)
            : clampWordValue(point.value);
        fields << QString::number(value);
    }
    fields << QStringLiteral("32767");
    return fields.join(QLatin1Char(','));
}

} // namespace

bool HetFile::load(const QString& path, HetData& out, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Could not open %1: %2").arg(path, file.errorString());
        return false;
    }
    return parseBytes(file.readAll(), out, error);
}

bool HetFile::save(const QString& path,
                   const HetData& data,
                   HetFormat format,
                   bool binaryHasHeader,
                   QString& error)
{
    const QByteArray bytes = format == HetFormat::CurrentText
        ? toCurrentText(data)
        : toLegacyBinary(data, format, binaryHasHeader);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = QStringLiteral("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        error = QStringLiteral("Could not write all data to %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool HetFile::parseBytes(const QByteArray& bytes, HetData& out, QString& error)
{
    QByteArray trimmed = bytes;
    while (!trimmed.isEmpty() && (trimmed.front() == '\xef' || trimmed.front() == '\xbb' || trimmed.front() == '\xbf')) {
        if (trimmed.startsWith("\xef\xbb\xbf")) {
            trimmed.remove(0, 3);
            break;
        }
        break;
    }

    if (trimmed.trimmed().startsWith("HETRO"))
        return parseText(trimmed, out, error);
    return parseBinary(bytes, out, error);
}

QByteArray HetFile::toCurrentText(const HetData& data)
{
    QString result = QStringLiteral("HETRO %1\n").arg(data.partials.size());
    for (const auto& partial : data.partials) {
        result += textTrack(partial, TrackKind::Amplitude);
        result += QLatin1Char('\n');
        result += textTrack(partial, TrackKind::Frequency);
        result += QStringLiteral("\n\n");
    }
    return result.toUtf8();
}

QByteArray HetFile::toLegacyBinary(const HetData& data,
                                   HetFormat format,
                                   bool withHeader)
{
    const bool littleEndian = format != HetFormat::LegacyBinaryBigEndian;
    QByteArray bytes;
    if (withHeader)
        appendWord(bytes, static_cast<qint16>(std::clamp<qsizetype>(data.partials.size(), 0, 32766)), littleEndian);

    for (const auto& partial : data.partials) {
        appendBinaryTrack(bytes, partial, TrackKind::Amplitude, littleEndian);
        appendBinaryTrack(bytes, partial, TrackKind::Frequency, littleEndian);
    }
    return bytes;
}

} // namespace hydra2
