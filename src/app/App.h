#pragma once

#include "Config.h"
#include "Navigator.h"
#include "Settings.h"

#include <QObject>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

namespace rigpa {

// What QML sees of the app: one singleton, `App`, made in main() before the engine loads.
class App : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(rigpa::Config *config READ config CONSTANT)
    Q_PROPERTY(rigpa::Settings *settings READ settings CONSTANT)
    Q_PROPERTY(rigpa::Navigator *nav READ nav CONSTANT)

public:
    App(Config *config, Settings *settings, Navigator *nav, QObject *parent = nullptr);
    ~App() override;

    static App *create(QQmlEngine *, QJSEngine *);

    Config *config() const { return m_config; }
    Settings *settings() const { return m_settings; }
    Navigator *nav() const { return m_nav; }

private:
    Config *m_config;
    Settings *m_settings;
    Navigator *m_nav;
};

} // namespace rigpa
