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
#include "HydraDocument.hpp"

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

using namespace hydra2;

class HetFileTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesCurrentText();
    void roundTripsLegacyLittleEndian();
    void preservesMutedSourceUntilSerialization();
    void undoRedoDocumentEdit();
    void smoothsSelectedBreakpoints();
    void averagesSelectedBreakpointsPerPartial();
};

void HetFileTest::parsesCurrentText()
{
    const QByteArray input =
        "HETRO 1\n"
        "-1,0,0,500,12000,1000,0,32767\n"
        "-2,0,440,500,442,1000,439,32767\n\n";

    HetData data;
    QString error;
    QVERIFY2(HetFile::parseBytes(input, data, error), qPrintable(error));
    QVERIFY(data.format == HetFormat::CurrentText);
    QCOMPARE(data.partials.size(), qsizetype(1));
    QCOMPARE(data.partials.first().amplitude.size(), qsizetype(3));
    QCOMPARE(data.partials.first().frequency.at(1).value, 442);
}

void HetFileTest::roundTripsLegacyLittleEndian()
{
    HetData original;
    Partial p;
    p.amplitude = {{1, 0, 0}, {2, 250, 1234}, {3, 500, 0}};
    p.frequency = {{4, 0, 220}, {5, 250, 221}, {6, 500, 219}};
    original.partials.push_back(p);

    const QByteArray binary = HetFile::toLegacyBinary(original,
                                                       HetFormat::LegacyBinaryLittleEndian,
                                                       true);
    HetData parsed;
    QString error;
    QVERIFY2(HetFile::parseBytes(binary, parsed, error), qPrintable(error));
    QCOMPARE(parsed.partials.size(), qsizetype(1));
    QCOMPARE(parsed.partials.first().amplitude.at(1).value, 1234);
    QCOMPARE(parsed.partials.first().frequency.at(2).value, 219);
    QVERIFY(parsed.format == HetFormat::LegacyBinaryLittleEndian);
    QVERIFY(parsed.binaryHasHeader);
}

void HetFileTest::preservesMutedSourceUntilSerialization()
{
    HetData data;
    Partial p;
    p.amplitude = {{1, 0, 2000}, {2, 1000, 4000}};
    p.frequency = {{3, 0, 440}, {4, 1000, 440}};
    p.gain = 0.0;
    data.partials.push_back(p);

    QCOMPARE(data.partials.first().amplitude.first().value, 2000);
    const QByteArray text = HetFile::toCurrentText(data);
    QVERIFY(text.contains("-1,0,0,1000,0,32767"));
    QCOMPARE(data.partials.first().amplitude.first().value, 2000);
}


void HetFileTest::undoRedoDocumentEdit()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("undo-test.het"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(
        "HETRO 1\n"
        "-1,0,1000,500,2000,1000,1000,32767\n"
        "-2,0,440,500,442,1000,439,32767\n\n");
    file.close();

    HydraDocument document;
    QVERIFY(document.openUrl(QUrl::fromLocalFile(path)));
    QVERIFY(!document.dirty());
    QVERIFY(!document.canUndo());

    const auto& amplitude = document.track(0, TrackKind::Amplitude);
    QVERIFY(amplitude.size() >= 2);
    const PointRef ref{0, TrackKind::Amplitude, amplitude.at(1).id};
    document.selectPoint(ref);

    const int originalTime = document.point(ref)->timeMs;
    const int originalValue = document.point(ref)->value;

    document.beginEditTransaction(QStringLiteral("Move breakpoints"));
    document.moveSelected(75, 250, TrackKind::Amplitude, true);
    document.moveSelected(25, 250, TrackKind::Amplitude, true);
    document.endEditTransaction();

    QVERIFY(document.canUndo());
    QCOMPARE(document.undoText(), QStringLiteral("Move breakpoints"));
    QVERIFY(document.dirty());
    QCOMPARE(document.point(ref)->timeMs, originalTime + 100);
    QCOMPARE(document.point(ref)->value, originalValue + 500);

    document.undo();
    QVERIFY(!document.dirty());
    QVERIFY(document.canRedo());
    QCOMPARE(document.point(ref)->timeMs, originalTime);
    QCOMPARE(document.point(ref)->value, originalValue);

    document.redo();
    QVERIFY(document.dirty());
    QCOMPARE(document.point(ref)->timeMs, originalTime + 100);
    QCOMPARE(document.point(ref)->value, originalValue + 500);
}


void HetFileTest::smoothsSelectedBreakpoints()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("smooth-test.het"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(
        "HETRO 1\n"
        "-1,0,100,250,300,500,100,750,500,1000,100,32767\n"
        "-2,0,440,250,441,500,442,750,443,1000,444,32767\n\n");
    file.close();

    HydraDocument document;
    QVERIFY(document.openUrl(QUrl::fromLocalFile(path)));

    const auto& amplitude = document.track(0, TrackKind::Amplitude);
    QCOMPARE(amplitude.size(), qsizetype(5));

    QVector<PointRef> selection;
    for (int i = 1; i <= 3; ++i)
        selection.push_back({0, TrackKind::Amplitude, amplitude.at(i).id});
    document.setPointSelection(selection);

    document.smoothSelected(static_cast<int>(TrackKind::Amplitude));

    const auto& smoothed = document.track(0, TrackKind::Amplitude);
    QCOMPARE(smoothed.at(0).value, 100);
    QCOMPARE(smoothed.at(1).value, 200);
    QCOMPARE(smoothed.at(2).value, 250);
    QCOMPARE(smoothed.at(3).value, 300);
    QCOMPARE(smoothed.at(4).value, 100);

    // Frequency data must not be touched while the amplitude editor is active.
    QCOMPARE(document.track(0, TrackKind::Frequency).at(2).value, 442);
    QCOMPARE(document.undoText(), QStringLiteral("Smooth selected"));

    document.undo();
    QCOMPARE(document.track(0, TrackKind::Amplitude).at(1).value, 300);
    QCOMPARE(document.track(0, TrackKind::Amplitude).at(2).value, 100);
    QCOMPARE(document.track(0, TrackKind::Amplitude).at(3).value, 500);
}

void HetFileTest::averagesSelectedBreakpointsPerPartial()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("average-test.het"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(
        "HETRO 2\n"
        "-1,0,1000,500,2000,1000,1000,32767\n"
        "-2,0,400,500,500,1000,600,32767\n"
        "-1,0,3000,500,4000,1000,5000,32767\n"
        "-2,0,1000,500,1200,1000,1600,32767\n\n");
    file.close();

    HydraDocument document;
    QVERIFY(document.openUrl(QUrl::fromLocalFile(path)));
    QCOMPARE(document.partialCount(), 2);

    const auto& freq0 = document.track(0, TrackKind::Frequency);
    const auto& freq1 = document.track(1, TrackKind::Frequency);

    QVector<PointRef> selection {
        {0, TrackKind::Frequency, freq0.at(0).id},
        {0, TrackKind::Frequency, freq0.at(1).id},
        {0, TrackKind::Frequency, freq0.at(2).id},
        {1, TrackKind::Frequency, freq1.at(0).id},
        {1, TrackKind::Frequency, freq1.at(2).id},
        // An amplitude point is deliberately selected as well; frequency mode
        // must ignore it.
        {1, TrackKind::Amplitude,
         document.track(1, TrackKind::Amplitude).at(1).id}
    };
    document.setPointSelection(selection);

    document.averageSelected(static_cast<int>(TrackKind::Frequency));

    const auto& averaged0 = document.track(0, TrackKind::Frequency);
    QCOMPARE(averaged0.at(0).value, 500);
    QCOMPARE(averaged0.at(1).value, 500);
    QCOMPARE(averaged0.at(2).value, 500);

    const auto& averaged1 = document.track(1, TrackKind::Frequency);
    QCOMPARE(averaged1.at(0).value, 1300);
    QCOMPARE(averaged1.at(1).value, 1200);
    QCOMPARE(averaged1.at(2).value, 1300);

    QCOMPARE(document.track(1, TrackKind::Amplitude).at(1).value, 4000);
    QCOMPARE(document.undoText(), QStringLiteral("Average selected"));
}

QTEST_GUILESS_MAIN(HetFileTest)
#include "tst_hetfile.moc"
