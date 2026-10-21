#include "TunerModel.h"

#include "engine/EngineClient.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <cmath>

namespace rigpa {

namespace {

// The vocabulary the engine publishes for nc.tuner (namchain docs/tuner.md).
const QString kType = QStringLiteral("nc.tuner");

QString cents_text(double cents)
{
    // A sign for both directions so a needle's number and its direction agree, and one decimal:
    // a tenth of a cent is finer than anyone tunes and coarser than the engine's own resolution.
    const QString number = QString::number(std::abs(cents), 'f', 1);
    if (std::abs(cents) < 0.05)
        return QStringLiteral("0.0");
    return (cents > 0 ? QStringLiteral("+") : QStringLiteral("-")) + number;
}

} // namespace

TunerModel::TunerModel(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(kPollMs);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (!m_watching)
            return;
        request();
    });
    clear();
}

void TunerModel::setWatching(bool watching)
{
    if (watching == m_watching)
        return;
    m_watching = watching;
    if (!watching) {
        m_timer.stop();
        m_inFlight = false; // a reply still on its way names a watch that has ended
        clear();
        emit changed();
        return;
    }
    m_timer.start();
    request();
}

void TunerModel::request()
{
    if (m_client == nullptr || !m_watching || m_inFlight)
        return;
    // One read in flight at a time: a slow engine then gets asked less often rather than piling up
    // requests it cannot answer. The timer is the floor otherwise -- 30 Hz is what the view draws.
    m_inFlight = true;
    m_sinceRequest.restart();
    m_client->request(QStringLiteral("rig.state"), {}, [this](const EngineReply &reply) {
        m_inFlight = false;
        if (reply.ok && m_watching)
            applyReply(reply.body);
    });
}

void TunerModel::clear()
{
    m_available = false;
    m_running = false;
    m_valid = false;
    m_noteText.clear();
    m_centsText.clear();
    m_hzText.clear();
    m_levelText.clear();
    m_a4Text.clear();
    m_inTune = false;
    m_needleX = kBarWidth / 2;
    m_levelFraction = 0.0;
    m_muted = true;
    m_cents = 0.0;
    for (double &phase : m_phase)
        phase = 0.0;
    m_status = tr("Waiting for the engine");
    emit strobeChanged();
}

void TunerModel::applyReply(const QJsonObject &body)
{
    if (!m_watching)
        return;

    // Every reporting node in the rig is in this answer: a whole-document read is cheaper to ask than
    // a per-node one, and this finds its own node by type. A rig has one per chain; the first is the
    // one drawn, and the others hear the same instrument on their own channel.
    QJsonObject tuner;
    for (const QJsonValue &entry : body.value(QStringLiteral("nodes")).toArray()) {
        const QJsonObject node = entry.toObject();
        if (node.value(QStringLiteral("type")).toString() == kType) {
            tuner = node;
            break;
        }
    }

    const bool had = m_available;
    m_available = true;

    // What the rail looks like after this answer, so an unchanged reading does not repaint the view
    // thirty times a second. `blocks` is deliberately not part of it: it changes every block, and a
    // note held steady would then redraw for ever.
    const bool running = !tuner.isEmpty();
    const QJsonObject state = tuner.value(QStringLiteral("state")).toObject();
    const bool valid = running && state.value(QStringLiteral("valid")).toBool(false);
    double cents = state.value(QStringLiteral("cents")).toDouble(0.0);
    if (!std::isfinite(cents))
        cents = 0.0;
    cents = std::max(-double(kMaxCents), std::min(double(kMaxCents), cents));
    const int needle = kBarWidth / 2 + int(std::lround(cents / double(kMaxCents) * (kBarWidth / 2)));

    QString noteText, centsText, hzText, levelText, a4Text;
    if (valid) {
        const QString note = state.value(QStringLiteral("note")).toString();
        const int octave = state.value(QStringLiteral("octave")).toInt();
        if (!note.isEmpty())
            noteText = note + QString::number(octave);
        centsText = cents_text(cents);
        hzText = QString::number(state.value(QStringLiteral("hz")).toDouble(0.0), 'f', 1) + tr(" Hz");
    }
    levelText = QString::number(state.value(QStringLiteral("level_db")).toDouble(-120.0), 'f', 0) + tr(" dB");
    a4Text = running ? tr("A4 = %1 Hz").arg(state.value(QStringLiteral("a4")).toDouble(440.0), 0, 'f', 0)
                     : QString();
    // `mute` is the node's own switch, published as a bool; a number is accepted too, because a flag
    // in a document is 0/1 and a client should not have to care which spelling a node chose.
    const QJsonValue muteValue = state.value(QStringLiteral("mute"));
    const bool muted = muteValue.isBool() ? muteValue.toBool()
                      : muteValue.isDouble() ? muteValue.toDouble() != 0.0
                                             : true;
    const bool inTune = valid && std::abs(cents) <= double(kInTuneCents);
    const double levelDb = std::max(-60.0, std::min(0.0, state.value(QStringLiteral("level_db")).toDouble(-120.0)));
    const qreal levelFraction = qreal((levelDb + 60.0) / 60.0);

    QString status;
    if (!running)
        // The node is not in the graph, so the rig is not muted -- which a player looking at a tuner
        // has to be told, because it is the opposite of what the view is for.
        status = tr("The rig is not muted — the engine is running no tuner node");
    else if (!valid)
        status = tr("No note");

    // Everything the view draws, and nothing it does not: an answer that changes only `blocks`
    // repaints nothing.
    const bool same = had && running == m_running && valid == m_valid && noteText == m_noteText
                      && centsText == m_centsText && hzText == m_hzText && levelText == m_levelText
                      && inTune == m_inTune && needle == m_needleX && a4Text == m_a4Text
                      && muted == m_muted && qAbs(levelFraction - m_levelFraction) < 0.005
                      && status == m_status;
    if (same)
        return;

    m_status = status;
    m_running = running;
    m_valid = valid;
    m_noteText = noteText;
    m_centsText = centsText;
    m_hzText = hzText;
    m_levelText = levelText;
    m_a4Text = a4Text;
    m_inTune = inTune;
    m_needleX = needle;
    m_levelFraction = levelFraction;
    m_muted = muted;
    // The strobe drifts by the *unrounded* cents: a display that stepped a tenth of a cent at a time
    // would tick, and the rows exist to show a drift rather than a number.
    m_cents = valid ? cents : 0.0;
    emit changed();
}

void TunerModel::advance(qreal seconds)
{
    if (!m_watching)
        return;
    // A timer that stalled (a repaint, a suspension) must not jump the bands a whole period: the
    // clamp is a display device, not a physics one, and the alternative is a pattern that teleports.
    double dt = double(seconds);
    if (!(dt > 0.0) || dt > 0.25)
        dt = 0.016;
    // No note, no drift: the rows settle rather than animate a reading that is not there.
    const double cents = m_valid ? m_cents : 0.0;
    bool moved = false;
    // The bands' presence follows whether there is a note, over the reference's 0.25 s constant:
    // they settle and fade rather than blink out.
    const double target = m_valid ? 1.0 : 0.22;
    const double fresh = m_activity + (target - m_activity) * (1.0 - std::exp(-dt / 0.25));
    if (std::abs(fresh - m_activity) > 0.002) {
        m_activity = fresh;
        moved = true;
    }
    for (int row = 0; row < kRowCount; ++row) {
        const double partial = row == 0 ? 1.0 : row == 1 ? 2.0 : 4.0;
        const double speed = m_valid ? std::max(-kMaxDrift, std::min(kMaxDrift, kDriftPerCent * cents * partial))
                                      : 0.0;
        double phase = m_phase[row] + speed * dt;
        phase -= std::floor(phase);
        if (phase != m_phase[row]) {
            m_phase[row] = phase;
            moved = true;
        }
    }
    if (moved)
        emit strobeChanged();
}

} // namespace rigpa
