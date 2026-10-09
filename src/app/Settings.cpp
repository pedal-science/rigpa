#include "Settings.h"

#include <algorithm>

namespace rigpa {

namespace {
const QString kRigListWidthKey = QStringLiteral("rig_list_width");
const QString kLiveMetersKey = QStringLiteral("live_meters");
}

Settings::Settings(const QString &path, QObject *parent)
    : QObject(parent), m_store(path, QSettings::IniFormat)
{
    m_rigListWidth = clampRigListWidth(m_store.value(kRigListWidthKey, kRigListWidthDefault).toInt());
    m_liveMeters = m_store.value(kLiveMetersKey, true).toBool();
}

int Settings::clampRigListWidth(int width)
{
    width = std::clamp(width, kRigListWidthMin, kRigListWidthMax);
    // Snap to the step, counted from the minimum.
    const int steps = (width - kRigListWidthMin + kRigListWidthStep / 2) / kRigListWidthStep;
    return std::min(kRigListWidthMin + steps * kRigListWidthStep, kRigListWidthMax);
}

void Settings::setRigListWidth(int width)
{
    width = clampRigListWidth(width);
    if (width == m_rigListWidth)
        return;
    m_rigListWidth = width;
    m_store.setValue(kRigListWidthKey, width);
    m_store.sync();
    emit rigListWidthChanged();
}

void Settings::stepRigListWidth(int steps)
{
    setRigListWidth(m_rigListWidth + steps * kRigListWidthStep);
}

void Settings::setLiveMeters(bool on)
{
    if (on == m_liveMeters)
        return;
    m_liveMeters = on;
    m_store.setValue(kLiveMetersKey, on);
    m_store.sync();
    emit liveMetersChanged();
}

} // namespace rigpa
