// The spec's hardware control model (docs/spec.md), row by row.
#include <QSignalSpy>
#include <QtTest>

#include "app/Navigator.h"

using rigpa::FocusRing;
using rigpa::Navigator;

class TestNavigator : public QObject
{
    Q_OBJECT

private slots:
    void startsInRigEditor()
    {
        Navigator nav;
        QCOMPARE(nav.mainView(), Navigator::RigView);
        QCOMPARE(nav.rigFocus(), Navigator::RigEditorFocus);
        QCOMPARE(nav.activeRing(), nav.rigEditor());
    }

    // Rig editor: rotary moves the selected node, apply opens its editor, back goes to the list.
    void rigEditorRow()
    {
        Navigator nav;
        nav.rigEditor()->setCount(5);
        QSignalSpy open(&nav, &Navigator::nodeEditorRequested);
        nav.rotate(2);
        QCOMPARE(nav.rigEditor()->highlight(), 2);
        nav.rotate(10);
        QCOMPARE(nav.rigEditor()->highlight(), 4); // stops at the end: the rotary doesn't wrap
        nav.apply();
        QCOMPARE(open.count(), 1);
        QCOMPARE(open.at(0).at(0).toInt(), 4);
        nav.back();
        QCOMPARE(nav.rigFocus(), Navigator::RigListFocus);
    }

    void rigEditorApplyWithNoNodesDoesNothing()
    {
        Navigator nav;
        QSignalSpy open(&nav, &Navigator::nodeEditorRequested);
        nav.apply();
        QCOMPARE(open.count(), 0);
    }

    // Rig list: rotary moves through rigs, apply loads and returns to the editor, back does nothing.
    void rigListRow()
    {
        Navigator nav;
        nav.rigList()->setCount(3);
        nav.focusRigList();
        QSignalSpy load(&nav, &Navigator::rigLoadRequested);
        nav.rotate(1);
        nav.back();
        QCOMPARE(nav.rigFocus(), Navigator::RigListFocus);
        QCOMPARE(nav.mainView(), Navigator::RigView);
        nav.apply();
        QCOMPARE(load.count(), 1);
        QCOMPARE(load.at(0).at(0).toInt(), 1);
        QCOMPARE(nav.rigFocus(), Navigator::RigEditorFocus);
    }

    // A ring of controls: rotary moves the highlight; apply selects a value control, toggles a
    // boolean, presses an action; while selected the rotary steps the value and apply or back
    // deselects.
    void controlsRow()
    {
        Navigator nav;
        FocusRing *ring = nav.settings();
        ring->setKinds({"value", "toggle", "action"});
        ring->setCount(3);
        nav.showMainView(Navigator::SettingsView);
        QSignalSpy stepped(ring, &FocusRing::stepped);
        QSignalSpy toggled(ring, &FocusRing::toggled);
        QSignalSpy activated(ring, &FocusRing::activated);

        nav.apply();
        QVERIFY(ring->editing());
        nav.rotate(3);
        QCOMPARE(stepped.count(), 1);
        QCOMPARE(stepped.at(0).at(1).toInt(), 3);
        QCOMPARE(ring->highlight(), 0); // the rotary edits, it doesn't move
        nav.apply();
        QVERIFY(!ring->editing());

        nav.apply();
        nav.back();
        QVERIFY(!ring->editing());
        QCOMPARE(nav.mainView(), Navigator::SettingsView); // back deselected, nothing more

        nav.rotate(1);
        nav.apply();
        QCOMPARE(toggled.count(), 1);
        QVERIFY(!ring->editing()); // booleans are never "selected"
        nav.rotate(1);
        nav.apply();
        QCOMPARE(activated.count(), 1);
    }

    void backFromAnotherMainViewGoesToRigView()
    {
        for (auto view : {Navigator::BrowseView, Navigator::SettingsView}) {
            Navigator nav;
            nav.focusRigList();
            nav.showMainView(view);
            nav.back();
            QCOMPARE(nav.mainView(), Navigator::RigView);
            QCOMPARE(nav.rigFocus(), Navigator::RigEditorFocus); // rig view lands in its editor
        }
    }

    void browseTogglesItsSwitch()
    {
        Navigator nav;
        nav.browse()->setKinds({"toggle", "item", "item"});
        nav.browse()->setCount(3);
        nav.showMainView(Navigator::BrowseView);
        QSignalSpy toggled(nav.browse(), &FocusRing::toggled);
        QSignalSpy activated(nav.browse(), &FocusRing::activated);
        nav.apply();
        QCOMPARE(toggled.count(), 1);
        nav.rotate(1);
        nav.apply();
        QCOMPARE(activated.count(), 1);
    }

    // Overlays (node editor, picker): their ring takes the controls; back closes the deepest.
    void overlaysStack()
    {
        Navigator nav;
        FocusRing editor, picker;
        editor.setKinds({"value", "action"});
        editor.setCount(2);
        picker.setCount(10);
        nav.openOverlay(Navigator::NodeEditor, &editor);
        QCOMPARE(nav.activeRing(), &editor);
        nav.apply();
        QVERIFY(editor.editing());
        nav.openOverlay(Navigator::Picker, &picker);
        QVERIFY(!editor.editing()); // whatever was being edited below stops
        QCOMPARE(nav.overlay(), Navigator::Picker);
        nav.rotate(4);
        QCOMPARE(picker.highlight(), 4);
        nav.back();
        QCOMPARE(nav.overlay(), Navigator::NodeEditor);
        nav.back();
        QCOMPARE(nav.overlay(), Navigator::NoOverlay);
        QCOMPARE(nav.rigFocus(), Navigator::RigEditorFocus);
    }

    void closingAnOverlayClosesWhatIsAboveIt()
    {
        Navigator nav;
        FocusRing editor, picker;
        editor.setCount(1);
        picker.setCount(1);
        nav.openOverlay(Navigator::NodeEditor, &editor);
        nav.openOverlay(Navigator::Picker, &picker);
        QVERIFY(nav.hasOverlay(Navigator::NodeEditor));
        QVERIFY(!nav.hasOverlay(Navigator::Palette));
        nav.closeOverlaysFrom(Navigator::NodeEditor);
        QCOMPARE(nav.overlay(), Navigator::NoOverlay);
        nav.closeOverlaysFrom(Navigator::Picker); // nothing open: nothing happens
        QCOMPARE(nav.activeRing(), nav.rigEditor());
    }

    void menuClosesOverlays()
    {
        Navigator nav;
        FocusRing editor;
        editor.setCount(1);
        nav.openOverlay(Navigator::NodeEditor, &editor);
        nav.showMainView(Navigator::SettingsView);
        QCOMPARE(nav.overlay(), Navigator::NoOverlay);
        QCOMPARE(nav.activeRing(), nav.settings());
    }

    void hints()
    {
        Navigator nav;
        nav.rigEditor()->setCount(1);
        QCOMPARE(nav.applyHint(), QStringLiteral("edit node"));
        QCOMPARE(nav.backHint(), QStringLiteral("rig list"));
        nav.settings()->setKinds({"value"});
        nav.settings()->setCount(1);
        nav.showMainView(Navigator::SettingsView);
        QCOMPARE(nav.applyHint(), QStringLiteral("adjust"));
        QCOMPARE(nav.backHint(), QStringLiteral("rig view"));
        nav.apply();
        QCOMPARE(nav.applyHint(), QStringLiteral("done"));
        QCOMPARE(nav.backHint(), QStringLiteral("done"));
    }
};

QTEST_GUILESS_MAIN(TestNavigator)
#include "tst_navigator.moc"
