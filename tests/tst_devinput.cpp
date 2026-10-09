#include <QSignalSpy>
#include <QWindow>
#include <QtTest>

#include "app/DevInput.h"

using rigpa::DevInput;

namespace {

QWheelEvent wheel(QPoint pixels, QPoint angle, Qt::KeyboardModifiers mods,
                  Qt::ScrollPhase phase = Qt::ScrollUpdate)
{
    return QWheelEvent(QPointF(10, 10), QPointF(10, 10), pixels, angle, Qt::NoButton, mods, phase,
                       false);
}

} // namespace

class TestDevInput : public QObject
{
    Q_OBJECT

private slots:
    void commandScrollIsTheRotary()
    {
        QWindow window;
        DevInput input;
        QSignalSpy rotated(&input, &DevInput::rotated);
        // Content moving down (fingers up with natural scrolling) is the next item.
        for (int i = 0; i < 3; ++i) {
            auto e = wheel({0, -20}, {0, -40}, Qt::ControlModifier);
            QVERIFY(input.eventFilter(&window, &e)); // eaten: it's the rotary, not a scroll
        }
        // 60 px, 30 px per detent.
        QCOMPARE(rotated.count(), 2);
        QCOMPARE(rotated.at(0).at(0).toInt(), 1);
        auto back = wheel({0, 90}, {0, 180}, Qt::ControlModifier);
        input.eventFilter(&window, &back);
        QCOMPARE(rotated.last().at(0).toInt(), -3);
    }

    void plainScrollIsLeftAlone()
    {
        QWindow window;
        DevInput input;
        QSignalSpy rotated(&input, &DevInput::rotated);
        auto e = wheel({0, -200}, {0, -400}, Qt::NoModifier);
        QVERIFY(!input.eventFilter(&window, &e));
        QCOMPARE(rotated.count(), 0);
    }

    void momentumDoesNotTurnTheRotary()
    {
        QWindow window;
        DevInput input;
        QSignalSpy rotated(&input, &DevInput::rotated);
        auto e = wheel({0, -300}, {0, -600}, Qt::ControlModifier, Qt::ScrollMomentum);
        QVERIFY(input.eventFilter(&window, &e));
        QCOMPARE(rotated.count(), 0);
    }

    void mouseWheelNotches()
    {
        QWindow window;
        DevInput input;
        QSignalSpy rotated(&input, &DevInput::rotated);
        auto e = wheel({}, {0, -240}, Qt::ControlModifier);
        input.eventFilter(&window, &e);
        QCOMPARE(rotated.at(0).at(0).toInt(), 2);
    }

    void inverted()
    {
        QWindow window;
        DevInput input;
        input.setInverted(true);
        QSignalSpy rotated(&input, &DevInput::rotated);
        auto e = wheel({0, -30}, {0, -60}, Qt::ControlModifier);
        input.eventFilter(&window, &e);
        QCOMPARE(rotated.at(0).at(0).toInt(), -1);
    }

    void periodAppliesCommaGoesBack()
    {
        QWindow window;
        DevInput input;
        QSignalSpy applied(&input, &DevInput::applied);
        QSignalSpy backed(&input, &DevInput::backed);
        QKeyEvent dot(QEvent::KeyPress, Qt::Key_Period, Qt::NoModifier, QStringLiteral("."));
        QKeyEvent comma(QEvent::KeyPress, Qt::Key_Comma, Qt::NoModifier, QStringLiteral(","));
        QKeyEvent repeat(QEvent::KeyPress, Qt::Key_Period, Qt::NoModifier, QStringLiteral("."), true);
        QVERIFY(input.eventFilter(&window, &dot));
        QVERIFY(input.eventFilter(&window, &comma));
        QVERIFY(!input.eventFilter(&window, &repeat));
        QCOMPARE(applied.count(), 1);
        QCOMPARE(backed.count(), 1);
    }

    void onlyTheWindowsDeliveryCounts()
    {
        QObject item; // an item inside the window sees the same event a second time
        DevInput input;
        QSignalSpy applied(&input, &DevInput::applied);
        QKeyEvent dot(QEvent::KeyPress, Qt::Key_Period, Qt::NoModifier, QStringLiteral("."));
        QVERIFY(!input.eventFilter(&item, &dot));
        QCOMPARE(applied.count(), 0);
    }
};

QTEST_MAIN(TestDevInput)
#include "tst_devinput.moc"
