#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

class EngineClient;

// The tuner's reading, as namchain publishes it (`rig.state`, ../namchain/docs/tuner.md): the note,
// its octave, how many cents off it is, and how loud the input is. The engine runs the tuner; this
// only draws what it says.
//
// The node it reads is one rigpa mounted: while the tuner main view is showing, RigSession publishes
// the document with one `nc.tuner` after each chain's input, and that node is silent, so the rig is
// muted for as long as the view is up (namchain docs/tuner.md, "The ephemeral node"). This model
// therefore also answers "is the rig actually muted": `running` is whether the engine's state holds a
// tuner node, which is the truth rather than what rigpa asked for.
//
// **Polling is bounded by visibility.** A tuner has to show a live reading for as long as it is on
// screen, so there is no quiet timer as there is for flowEQ's panel: instead nothing is asked at all
// until the view calls `setWatching(true)`, one read is in flight at a time, and the timer stops when
// the view is left. That is what keeps the pedal's screen and core 0 idle when the tuner is not up
// (docs/spec.md, and D14's soak).
class TunerModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns this")
    /// The engine has answered at least once on this watch: before this, "waiting" is honest.
    Q_PROPERTY(bool available READ available NOTIFY changed)
    /// The document the engine is running holds a tuner node, so the rig really is silent.
    Q_PROPERTY(bool running READ running NOTIFY changed)
    /// The engine has a note it trusts (the tuner withholds one it does not).
    Q_PROPERTY(bool valid READ valid NOTIFY changed)
    Q_PROPERTY(QString noteText READ noteText NOTIFY changed)   // "A2", empty when there is no note
    Q_PROPERTY(QString centsText READ centsText NOTIFY changed) // "+2.4", "-1.7", "0.0"
    Q_PROPERTY(QString hzText READ hzText NOTIFY changed)       // "440.0 Hz"
    Q_PROPERTY(QString levelText READ levelText NOTIFY changed) // "-17 dB"
    Q_PROPERTY(bool inTune READ inTune NOTIFY changed)
    Q_PROPERTY(int needleX READ needleX NOTIFY changed)         // px, 0 … kBarWidth
    Q_PROPERTY(int barWidth READ barWidth CONSTANT)
    Q_PROPERTY(int maxCents READ maxCents CONSTANT)             // half the bar either side of the note
    Q_PROPERTY(int inTuneCents READ inTuneCents CONSTANT)       // what counts as in tune
    Q_PROPERTY(qreal levelFraction READ levelFraction NOTIFY changed) // 0…1 over -60…0 dBFS
    Q_PROPERTY(QString status READ status NOTIFY changed)       // one line, when there is no note
    Q_PROPERTY(QString a4Text READ a4Text NOTIFY changed)       // "A4 = 440 Hz"
    /// The node's switch, from the engine's state: true means the output is silence. The view's
    /// control is the player's intent (`RigSession::tunerMuted`); this is what the engine reports,
    /// so a refused patch leaves the two visibly disagreeing rather than silently lying.
    Q_PROPERTY(bool muted READ muted NOTIFY changed)
    /// The strobe's three rows, as fractions of a band period, 0…1: partials 1, 2 and 4 of the note
    /// (namchain docs/tuner.md, "The strobe"). The view multiplies by its own band width.
    Q_PROPERTY(qreal rowPhase1 READ rowPhase1 NOTIFY strobeChanged)
    Q_PROPERTY(qreal rowPhase2 READ rowPhase2 NOTIFY strobeChanged)
    Q_PROPERTY(qreal rowPhase4 READ rowPhase4 NOTIFY strobeChanged)
    /// How present the bands are, 0…1: they settle and fade with no note, as a mechanical strobe's
    /// do, so "the strings have stopped" is visible rather than just a number going blank.
    Q_PROPERTY(qreal strobeAlpha READ strobeAlpha NOTIFY strobeChanged)

public:
    // The cents bar's coordinate space. The pedal's screen is a fixed 1920x480 and the whole UI
    // scales rather than the panels, so one bar width is enough; the QML places it at its own x.
    static constexpr int kBarWidth = 720;
    // How far either side of the note the needle can go. `cents` is always inside this: it is the
    // offset from the *nearest* note, so its range is half a semitone.
    static constexpr int kMaxCents = 50;
    // What counts as in tune, in cents. A UI choice, not the engine's: the tuner reports a number.
    static constexpr int kInTuneCents = 5;
    // One read per screen frame. A needle slower than about 12 Hz reads as broken, and 30 Hz is what
    // the panel draws at.
    static constexpr int kPollMs = 33;
    // The strobe, ported from squatch-tuner's reference implementation (hosts/web/strobe.js): three
    // rows for partials 1, 2 and 4, row k drifting k times as fast as the fundamental, at 0.5 band
    // periods per second per cent -- and no faster than 12, which is what would alias at 60 fps.
    static constexpr int kRowCount = 3;
    static constexpr double kDriftPerCent = 0.5;
    static constexpr double kMaxDrift = 12.0;

    explicit TunerModel(QObject *parent = nullptr);

    void setClient(EngineClient *client) { m_client = client; }
    // Start reporting while the tuner view is up, and stop entirely when it is left.
    void setWatching(bool watching);
    bool watching() const { return m_watching; }

    bool available() const { return m_available; }
    bool running() const { return m_running; }
    bool valid() const { return m_valid; }
    QString noteText() const { return m_noteText; }
    QString centsText() const { return m_centsText; }
    QString hzText() const { return m_hzText; }
    QString levelText() const { return m_levelText; }
    bool inTune() const { return m_inTune; }
    int needleX() const { return m_needleX; }
    int barWidth() const { return m_barWidth; }
    // The scale's width in pixels: the layout's (720 on the bar, 560 on the 4.3" panel). Set once,
    // before the view is made.
    void setBarWidth(int width);
    int maxCents() const { return kMaxCents; }
    int inTuneCents() const { return kInTuneCents; }
    // The input's level as a bar's fill, over the range a meter shows. Layout maths in C++, as
    // everywhere else: the QML places it, it does not compute it.
    qreal levelFraction() const { return m_levelFraction; }
    QString status() const { return m_status; }
    QString a4Text() const { return m_a4Text; }
    bool muted() const { return m_muted; }
    qreal rowPhase1() const { return qreal(m_phase[0]); }
    qreal rowPhase2() const { return qreal(m_phase[1]); }
    qreal rowPhase4() const { return qreal(m_phase[2]); }
    qreal strobeAlpha() const { return qreal(m_activity); }

    /// Advance the strobe by `seconds` and wrap each row. The view calls this on its frame timer,
    /// which is what keeps the drift a function of wall time rather than of the polling rate: a
    /// dropped frame must move the bands further, not slower.
    Q_INVOKABLE void advance(qreal seconds);

    // For tests: digest one `rig.state` reply body, as `EngineClient` would deliver it.
    void applyReply(const QJsonObject &body);

signals:
    void changed();
    void strobeChanged();

private:
    void request();
    void clear();

    EngineClient *m_client = nullptr;
    QTimer m_timer;
    QElapsedTimer m_sinceRequest;
    bool m_watching = false;
    bool m_inFlight = false;

    bool m_available = false;
    bool m_running = false;
    bool m_valid = false;
    QString m_noteText;
    QString m_centsText;
    QString m_hzText;
    QString m_levelText;
    QString m_a4Text;
    QString m_status;
    bool m_inTune = false;
    int m_barWidth = kBarWidth;
    int m_needleX = kBarWidth / 2;
    qreal m_levelFraction = 0.0;
    bool m_muted = true;
    /// The cents the rows drift by, before clamping: kept so `advance` needs only the clock.
    double m_cents = 0.0;
    double m_phase[kRowCount] = {0.0, 0.0, 0.0};
    double m_activity = 0.22;
};

} // namespace rigpa
