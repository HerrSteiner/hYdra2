#include "HetFile.hpp"

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

QTEST_MAIN(HetFileTest)
#include "tst_hetfile.moc"
