#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

#include "library/Library.h"
#include "library/MakeHeuristics.h"
#include "library/Sniff.h"

using namespace rigpa;

namespace {

QByteArray le32(quint32 v) { QByteArray b(4, 0); qToLittleEndian(v, b.data()); return b; }
QByteArray le16(quint16 v) { QByteArray b(2, 0); qToLittleEndian(v, b.data()); return b; }

QByteArray wavBytes(int channels, int rate, int bits, int frames, bool junkFirst = false)
{
    QByteArray fmt = le16(1) + le16(quint16(channels)) + le32(quint32(rate))
                     + le32(quint32(rate * channels * bits / 8)) + le16(quint16(channels * bits / 8))
                     + le16(quint16(bits));
    QByteArray body = "WAVE";
    if (junkFirst)
        body += "JUNK" + le32(5) + QByteArray(5, 0) + QByteArray(1, 0); // odd size: a pad byte
    body += "fmt " + le32(quint32(fmt.size())) + fmt;
    const int dataBytes = frames * channels * bits / 8;
    body += "data" + le32(quint32(dataBytes)) + QByteArray(dataBytes, 0);
    return "RIFF" + le32(quint32(body.size())) + body;
}

QByteArray weights(int n)
{
    QByteArray w = "[";
    for (int i = 0; i < n; ++i)
        w += (i ? ",0.123456" : "0.123456");
    return w + "]";
}

void write(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bytes);
}

} // namespace

class TestLibrary : public QObject
{
    Q_OBJECT

private slots:
    void metadataAtTheFront()
    {
        const QByteArray file = R"({"version":"0.5.4","metadata":{"name":"Deluxe {clean}","gear_make":"Fender",
            "gear_model":"Deluxe Reverb","gear_type":"amp","tone_type":"clean"},"architecture":"WaveNet",
            "config":{},"weights":)" + weights(20000) + R"(,"sample_rate":48000})";
        QVERIFY(file.size() > sniff::kNamHead + sniff::kNamTail);
        const NamInfo info = sniff::nam(file.left(sniff::kNamHead), file.right(sniff::kNamTail));
        QCOMPARE(info.name, QStringLiteral("Deluxe {clean}")); // braces in a string don't confuse it
        QCOMPARE(info.make, QStringLiteral("Fender"));
        QCOMPARE(info.architecture, QStringLiteral("WaveNet"));
        QCOMPARE(info.sampleRate, 48000.0);
        QCOMPARE(info.toneType, QStringLiteral("clean"));
    }

    void metadataAfterTheWeights()
    {
        // 0.7 files: metadata at the end, where ljoss's 16 KiB window never looked.
        const QByteArray file = R"({"version":"0.7.0","architecture":"WaveNet","config":{},"weights":)"
                                + weights(20000)
                                + R"(,"sample_rate":48000,"metadata":{"name":"A2","gear_make":"Orange"}})";
        const NamInfo info = sniff::nam(file.left(sniff::kNamHead), file.right(sniff::kNamTail));
        QCOMPARE(info.name, QStringLiteral("A2"));
        QCOMPARE(info.make, QStringLiteral("Orange"));
        QCOMPARE(info.architecture, QStringLiteral("WaveNet"));
    }

    void cutOffMetadataIsNotGuessed()
    {
        QVERIFY(sniff::objectAfterKey("{\"metadata\":{\"name\":\"half", "metadata").isEmpty());
        QCOMPARE(sniff::objectAfterKey("{\"metadata\" : {\"a\":\"}\"}}", "metadata"), QByteArray("{\"a\":\"}\"}"));
    }

    void wavHeaders()
    {
        auto info = sniff::wav(wavBytes(1, 48000, 24, 8192));
        QVERIFY(info);
        QCOMPARE(info->channels, 1);
        QCOMPARE(info->sampleRate, 48000.0);
        QCOMPARE(info->frames, 8192);
        info = sniff::wav(wavBytes(2, 44100, 16, 100, true)); // a chunk before fmt, padded
        QVERIFY(info);
        QCOMPARE(info->channels, 2);
        QCOMPARE(info->frames, 100);
        QVERIFY(!sniff::wav("not a wav at all"));
    }

    void brands()
    {
        QCOMPARE(make::guess("Orange Rockerverb mk3", "x", {}), QStringLiteral("Orange"));
        QCOMPARE(make::guess("Some Boutique Co", "x", {}), QStringLiteral("Some Boutique Co"));
        QCOMPARE(make::guess({}, "ORNG-V30-e609", {}), QStringLiteral("Orange"));
        const QStringList fenderFolders{"Fender", "Deluxe"};
        QCOMPARE(make::guess({}, "x", fenderFolders), QStringLiteral("Fender"));
        QCOMPARE(make::canonical("mesa boogie mark v"), QStringLiteral("Mesa/Boogie"));
        QCOMPARE(make::canonical("orangewood"), QString()); // whole words only
        // Brackets list compatible gear, not the maker.
        const QStringList yorkFolders{"Ampeg 4x10", "York Audio AMPG 410 HS v1.01",
                                      "48k (Fractal, Line 6, Suhr, etc.)"};
        QCOMPARE(make::guess({}, "YA AMPG 410 Mix 01", yorkFolders), QStringLiteral("Ampeg"));
        QCOMPARE(make::guess({}, "track", {}), make::kUnknown);
    }

    void scanFindsIdsGroupsAndSkipsDotNames()
    {
        QTemporaryDir dir;
        const QString models = dir.filePath("models"), irs = dir.filePath("irs");
        write(models + "/Fender/Deluxe.nam", R"({"architecture":"WaveNet","metadata":{"name":"Deluxe"},"sample_rate":48000})");
        write(models + "/top.nam", R"({"architecture":"LSTM"})");
        write(models + "/.hidden/x.nam", "{}");
        write(models + "/.dot.nam", "{}");
        write(models + "/readme.txt", "hi");
        write(irs + "/V30/57.wav", wavBytes(1, 48000, 24, 4096));
        write(irs + "/cab.nam", R"({"architecture":"Linear"})");

        const auto result = Library::scan(models, irs, {});
        QCOMPARE(result.models.size(), 2);
        QCOMPARE(result.models.at(0).id, QStringLiteral("top"));         // top level sorts first
        QCOMPARE(result.models.at(1).id, QStringLiteral("Fender/Deluxe"));
        QCOMPARE(result.models.at(1).group, QStringLiteral("Fender"));
        QCOMPARE(result.models.at(1).make, QStringLiteral("Fender"));
        QCOMPARE(result.irs.size(), 2);
        const LibraryEntry *wav = nullptr;
        for (const auto &e : result.irs)
            if (e.id == QLatin1String("V30/57"))
                wav = &e;
        QVERIFY(wav);
        QCOMPARE(wav->frames, 4096);
        QCOMPARE(wav->make, QStringLiteral("Celestion"));

        // A rescan reuses what hasn't changed: a cached entry with a planted name survives.
        Library::Cache cache;
        for (auto e : result.models) {
            e.name = QStringLiteral("cached");
            cache.insert(e.path, e);
        }
        const auto again = Library::scan(models, irs, cache);
        QCOMPARE(again.models.at(0).name, QStringLiteral("cached"));
    }

    void modelSwitchesKindAndScansInTheBackground()
    {
        QTemporaryDir dir;
        write(dir.filePath("m/a.nam"), "{}");
        write(dir.filePath("i/b.wav"), wavBytes(1, 48000, 16, 10));
        write(dir.filePath("i/c.wav"), wavBytes(1, 48000, 16, 10));
        Library library;
        library.setRoots(dir.filePath("m"), dir.filePath("i"));
        QSignalSpy scanned(&library, &Library::scanned);
        library.rescan();
        QVERIFY(scanned.wait(5000));
        QCOMPARE(library.rowCount(), 1);
        library.setKind(1);
        QCOMPARE(library.rowCount(), 2);
        QVERIFY(library.find(LibraryEntry::Ir, "b"));
        QVERIFY(!library.find(LibraryEntry::Model, "b"));
    }

    void missingRootIsSaid()
    {
        const auto result = Library::scan("/no/such/models", {}, {});
        QVERIFY(result.problem.contains("/no/such/models"));
    }
};

QTEST_GUILESS_MAIN(TestLibrary)
#include "tst_library.moc"
