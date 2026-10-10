// The schema as rigpa reads it, and how the node editor shows and maps values.
#include <QJsonDocument>
#include <QtTest>

#include "engine/Schema.h"
#include "rig/NodeEditorModel.h"

using namespace rigpa;

namespace {

// Entries trimmed from a real engine.schema reply (namchain 2026-10).
const char *kSchema = R"({"ok":true,"nodes":[
 {"id":"nc.eq.biquad","kind":"primitive","name":"Biquad EQ","category":"eq","flags":{"bypassable":true},
  "params":[
   {"id":"mode","kind":"option","name":"Mode","default":0.0,"options":[{"label":"Peaking","value":"peaking"},
     {"label":"Low shelf","value":"lowshelf"}],"options_from":null,"patchable":true,"rampable":false,"scale":"linear"},
   {"id":"freq","kind":"number","name":"Frequency","default":1000.0,"min":10.0,"max":null,"scale":"log",
     "unit":"Hz","patchable":true,"rampable":true},
   {"id":"q","kind":"number","name":"Q","default":0.707,"min":0.0,"min_inclusive":false,"max":20.0,
     "max_inclusive":true,"scale":"log","patchable":true,"rampable":true},
   {"id":"gain_db","kind":"number","name":"Gain","default":0.0,"min":-30.0,"min_inclusive":false,"max":30.0,
     "max_inclusive":false,"unit":"dB"}]},
 {"id":"nc.nam","kind":"primitive","name":"NAM model","flags":{"bypassable":true},
  "params":[{"id":"model","kind":"option","name":"Model","default":null,"options":[{"label":"a","value":"a"}],
     "options_from":"models","patchable":true}]},
 {"id":"nc.split","kind":"primitive","name":"Split","flags":{"bypassable":false},
  "params":[{"id":"outputs","kind":"number","default":2.0,"min":1.0,"max":64.0,"snap":true,"step":1.0}]},
 {"id":"fx.amp.tonestack","kind":"macro","name":"Tone stack","bypass":"roles",
  "params":[{"id":"bass","kind":"number","name":"Bass","default":0.0,"min":-18.0,"max":18.0,"unit":"dB",
     "rampable":true,"patchable":true,"hidden":false},
    {"id":"secret","kind":"number","hidden":true}]},
 {"id":"nc.delay","kind":"primitive","name":"Delay line","flags":{"bypassable":true},
  "params":[{"id":"samples","kind":"number","min":1.0,"max":null,"one_of":"delay time","unit":"samples","step":1.0},
    {"id":"ms","kind":"number","min":0.021,"max":null,"one_of":"delay time","unit":"ms"}]}
]})";

Schema schema()
{
    return Schema::parse(QJsonDocument::fromJson(kSchema).object());
}

} // namespace

class TestSchema : public QObject
{
    Q_OBJECT

private slots:
    void parses()
    {
        const Schema s = schema();
        QCOMPARE(s.typeIds().size(), 5);
        const NodeTypeSpec *biquad = s.type("nc.eq.biquad");
        QVERIFY(biquad);
        QVERIFY(!biquad->macro);
        QVERIFY(biquad->bypassable);
        const ParamSpec *mode = biquad->param("mode");
        QCOMPARE(mode->kind, ParamSpec::Option);
        QCOMPARE(mode->options.size(), 2);
        QCOMPARE(mode->options.at(1).value, QStringLiteral("lowshelf"));
        const ParamSpec *freq = biquad->param("freq");
        QVERIFY(freq->log);
        QVERIFY(freq->rampable);
        QVERIFY(!freq->max);
        QCOMPARE(Schema::upperOf(*freq), 20000.0); // open-ended, so a stand-in

        // A library reference keeps no option list: the library is read directly.
        const ParamSpec *model = s.type("nc.nam")->param("model");
        QCOMPARE(model->optionsFrom, QStringLiteral("models"));
        QVERIFY(model->options.isEmpty());

        QVERIFY(!s.type("nc.split")->bypassable);
        QCOMPARE(s.type("nc.split")->param("outputs")->step, 1.0);
        QVERIFY(s.type("fx.amp.tonestack")->macro);
        QVERIFY(s.type("fx.amp.tonestack")->bypassable); // it declares a bypass rule
        QCOMPARE(s.type("nc.delay")->param("ms")->oneOf, QStringLiteral("delay time"));
    }

    void positionsAndValues()
    {
        const Schema s = schema();
        const ParamSpec &freq = *s.type("nc.eq.biquad")->param("freq");
        // Logarithmic: equal travel is an equal ratio.
        QCOMPARE(NodeEditorModel::positionOf(freq, 10), 0.0);
        QCOMPARE(NodeEditorModel::positionOf(freq, 20000), 1.0);
        QVERIFY(qAbs(NodeEditorModel::valueAt(freq, 0.5) - std::sqrt(10.0 * 20000)) < 1e-6);
        QVERIFY(qAbs(NodeEditorModel::valueAt(freq, NodeEditorModel::positionOf(freq, 440)) - 440) < 1e-9);

        const ParamSpec &bass = *s.type("fx.amp.tonestack")->param("bass");
        QCOMPARE(NodeEditorModel::positionOf(bass, 0), 0.5);
        QCOMPARE(NodeEditorModel::valueAt(bass, 2.0), 18.0); // clamped
        const ParamSpec &outputs = *s.type("nc.split")->param("outputs");
        QCOMPARE(NodeEditorModel::valueAt(outputs, 0.5), 33.0); // snapped to its step
    }

    void excludedEndsAreNeverSent()
    {
        const Schema s = schema();
        const ParamSpec &freq = *s.type("nc.eq.biquad")->param("freq");
        QVERIFY(freq.minInclusive); // absent means allowed
        QCOMPARE(Schema::lowerOf(freq), 10.0);

        // "More than -30" and "less than 30": the knob's ends are just inside.
        const ParamSpec &gain = *s.type("nc.eq.biquad")->param("gain_db");
        QVERIFY(!gain.minInclusive);
        QVERIFY(!gain.maxInclusive);
        QVERIFY(Schema::lowerOf(gain) > -30.0 && Schema::lowerOf(gain) < -29.99);
        QVERIFY(Schema::upperOf(gain) < 30.0 && Schema::upperOf(gain) > 29.99);
        QVERIFY(NodeEditorModel::valueAt(gain, 0) > -30.0);
        QVERIFY(NodeEditorModel::valueAt(gain, 1) < 30.0);

        // A log scale with nothing but "more than 0" below: a stand-in a thousandth of the top.
        const ParamSpec &q = *s.type("nc.eq.biquad")->param("q");
        QCOMPARE(Schema::lowerOf(q), 0.02);
        QCOMPARE(NodeEditorModel::valueAt(q, 0), 0.02);
        QVERIFY(qAbs(NodeEditorModel::valueAt(q, 0.5) - std::sqrt(0.02 * 20)) < 1e-9); // still logarithmic

        // A stepped parameter steps in.
        ParamSpec count;
        count.min = 0;
        count.minInclusive = false;
        count.max = 8;
        count.step = 1;
        QCOMPARE(Schema::lowerOf(count), 1.0);
    }

    void formats()
    {
        const Schema s = schema();
        const ParamSpec &freq = *s.type("nc.eq.biquad")->param("freq");
        QCOMPARE(NodeEditorModel::format(freq, 80), QStringLiteral("80 Hz"));
        QCOMPARE(NodeEditorModel::format(freq, 9000), QStringLiteral("9 kHz"));
        QCOMPARE(NodeEditorModel::format(freq, 12500), QStringLiteral("12.5 kHz"));
        const ParamSpec &bass = *s.type("fx.amp.tonestack")->param("bass");
        QCOMPARE(NodeEditorModel::format(bass, 1.2), QStringLiteral("+1.2 dB"));
        QCOMPARE(NodeEditorModel::format(bass, 0), QStringLiteral("0.0 dB"));
        QCOMPARE(NodeEditorModel::format(bass, -6), QStringLiteral("-6.0 dB"));
        ParamSpec mix;
        mix.unit = "%";
        mix.min = 0;
        mix.max = 1;
        QCOMPARE(NodeEditorModel::format(mix, 0.25), QStringLiteral("25 %"));
        ParamSpec ramp;
        ramp.unit = "ms";
        QCOMPARE(NodeEditorModel::format(ramp, 5), QStringLiteral("5 ms"));
        QCOMPARE(NodeEditorModel::format(*s.type("nc.split")->param("outputs"), 3), QStringLiteral("3"));
    }
};

QTEST_GUILESS_MAIN(TestSchema)
#include "tst_schema.moc"
