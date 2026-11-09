// The pedal's block templates: read from its catalogue (here, the file pedald serves it from), each
// control's transform inverts, and the palette makes a block that says which template it came from.
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

#include "engine/EngineSession.h"
#include "rig/PaletteModel.h"
#include "rig/Templates.h"

using namespace rigpa;

class TestTemplates : public QObject
{
    Q_OBJECT
    Templates templates;

private slots:
    void initTestCase()
    {
        QFile file(QStringLiteral(RIGPA_TEMPLATES));
        // pedal.project beside this repo is where the templates live: a build without it (the image
        // build's container) has nothing to check them against.
        if (!file.exists())
            QSKIP("no templates.json (pedal.project isn't beside this repo)");
        QVERIFY2(file.open(QIODevice::ReadOnly), RIGPA_TEMPLATES);
        // The catalogue's answer is the file's blocks as `templates`.
        const QJsonObject served{{"templates", QJsonDocument::fromJson(file.readAll()).object().value("blocks")}};
        templates.apply(served);
    }

    void the27Blocks()
    {
        QCOMPARE(templates.all().size(), 27);
        const Templates::Template *cut = templates.find("eq.lowcut");
        QVERIFY(cut);
        QCOMPARE(cut->name, QStringLiteral("Low Cut"));
        QCOMPARE(cut->controls.size(), 2);
        QCOMPARE(cut->controls[1].options, QStringList({"12 dB", "24 dB"}));
    }

    void everyTransformInverts()
    {
        for (const Templates::Template &t : templates.all()) {
            for (const Templates::Control &c : t.controls) {
                if (c.kind == QLatin1String("options")) {
                    for (const QString &option : c.options)
                        QCOMPARE(Templates::fromEngine(c.transform, Templates::toEngine(c.transform, option)).toString(), option);
                    continue;
                }
                for (const double v : {c.min, c.defaultValue, c.max}) {
                    const double back = Templates::fromEngine(c.transform, Templates::toEngine(c.transform, v)).toDouble();
                    QVERIFY2(qAbs(back - v) < 1e-9, qPrintable(t.id + "." + c.id));
                }
            }
        }
        // A tone knob at its centre is flat; a volume at 10 is unity and at 0 is the floor.
        const auto *brit = templates.find("amp.brit800");
        QCOMPARE(Templates::toEngine(brit->controls[1].transform, 5.0).toDouble(), 0.0);
        const auto *volume = templates.find("util.volume");
        QCOMPARE(Templates::toEngine(volume->controls[0].transform, 10.0).toDouble(), 0.0);
        QCOMPARE(Templates::toEngine(volume->controls[0].transform, 0.0).toDouble(), -60.0);
    }

    void aBlockSaysWhereItCameFrom()
    {
        EngineSession engine;
        PaletteModel palette(&engine, nullptr);
        palette.setTemplates(&templates);
        QCOMPARE(palette.indexOfType("amp"), -1); // the templates' amps stand in for the old Amp
        const int row = palette.indexOfType("eq.lowcut");
        QVERIFY(row >= 0);
        const auto made = palette.make(row);
        QVERIFY(made);
        QCOMPARE(made->nodes.size(), 1);
        const RigNode &node = made->nodes[0];
        QCOMPARE(node.id, made->element + "-cut");
        QCOMPARE(node.json.value("meta").toObject().value("template").toString(), QStringLiteral("eq.lowcut"));
        QCOMPARE(node.json.value("meta").toObject().value("slot").toString(), QStringLiteral("cut"));
        QCOMPARE(node.json.value("low_cut").toDouble(), 80.0);   // the control's default, as the engine's
        QCOMPARE(node.json.value("low_slope").toString(), QStringLiteral("12"));
        QVERIFY(templates.forNodes({node.json}) == templates.find("eq.lowcut"));
    }
};

QTEST_MAIN(TestTemplates)
#include "tst_templates.moc"
