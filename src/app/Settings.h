#pragma once

#include <QObject>
#include <QSettings>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

// The user's settings, kept in an INI file (config: settings_file). Changes are written at once:
// settings are direct manipulation like everything else.
class Settings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the settings")
    Q_PROPERTY(int rigListWidth READ rigListWidth WRITE setRigListWidth NOTIFY rigListWidthChanged)
    Q_PROPERTY(int rigListWidthMin MEMBER kRigListWidthMin CONSTANT)
    Q_PROPERTY(int rigListWidthMax MEMBER kRigListWidthMax CONSTANT)
    Q_PROPERTY(int rigListWidthStep MEMBER kRigListWidthStep CONSTANT)
    Q_PROPERTY(bool liveMeters READ liveMeters WRITE setLiveMeters NOTIFY liveMetersChanged)

public:
    static constexpr int kRigListWidthMin = 192;
    static constexpr int kRigListWidthMax = 576;
    static constexpr int kRigListWidthStep = 16;
    static constexpr int kRigListWidthDefault = 288;

    explicit Settings(const QString &path, QObject *parent = nullptr);

    QString path() const { return m_store.fileName(); }

    int rigListWidth() const { return m_rigListWidth; }
    void setRigListWidth(int width);
    // The rotary's way in: whole steps, clamped.
    Q_INVOKABLE void stepRigListWidth(int steps);

    bool liveMeters() const { return m_liveMeters; }
    void setLiveMeters(bool on);

    static int clampRigListWidth(int width);

signals:
    void rigListWidthChanged();
    void liveMetersChanged();

private:
    QSettings m_store;
    int m_rigListWidth;
    bool m_liveMeters;
};

} // namespace rigpa
