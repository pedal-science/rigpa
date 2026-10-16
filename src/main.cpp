#include "app/App.h"
#include "app/Config.h"
#include "app/DemoAnimator.h"
#include "app/DevInput.h"
#include "app/Navigator.h"
#include "app/QuitOnSignal.h"
#include "app/Settings.h"
#include "engine/EngineSession.h"
#include "library/Library.h"
#include "library/PickerModel.h"
#include "pedald/PedaldClient.h"
#include "rig/NodeEditorModel.h"
#include "rig/PaletteModel.h"
#include "rig/RigManager.h"
#include "rig/RigEditorModel.h"
#include "rig/RigList.h"
#include "rig/RigSession.h"

#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMetaEnum>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>

#include <cstdio>

Q_IMPORT_QML_PLUGIN(Rigpa_CorePlugin)

int main(int argc, char *argv[])
{
    // Names are typed on Qt Virtual Keyboard (the pedal has no keyboard; on the Mac it shows too).
    if (qEnvironmentVariableIsEmpty("QT_IM_MODULE"))
        qputenv("QT_IM_MODULE", "qtvirtualkeyboard");
    QGuiApplication app(argc, argv);
    // systemd's stop is SIGTERM: quit cleanly (exit 0), not eglfs's _exit(1). After `app`, so it
    // replaces the handler the platform plugin installed.
    rigpa::QuitOnSignal quitOnSignal;
    QGuiApplication::setApplicationName(QStringLiteral("rigpa"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("hemsley.cc"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("rigpa, the pedal's UI"));
    parser.addHelpOption();
    const QCommandLineOption configOption(QStringLiteral("config"),
        QStringLiteral("rigpa.json to read (default: $RIGPA_CONFIG, then the platform's)."),
        QStringLiteral("file"));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
        QStringLiteral("Render, save the window to <file> (PNG) once it has settled, and quit."),
        QStringLiteral("file"));
    const QCommandLineOption viewOption(QStringLiteral("view"),
        QStringLiteral("Start in this main view: rig, browse or settings."),
        QStringLiteral("view"));
    const QCommandLineOption inputOption(QStringLiteral("input"),
        QStringLiteral("Replay hardware controls once loaded (for demos and checks): '>' and '<' turn "
                       "the rotary, '.' is apply, ',' is back, '+' is the header's + (the palette), 'N', 'R', 'C' "
                       "and 'D' are the rig list's New, Rename, Copy and Delete."),
        QStringLiteral("keys"));
    const QCommandLineOption demoDragOption(QStringLiteral("demo-drag"),
        QStringLiteral("For demos: lift the rig editor's node <from> and hold it over the left edge of "
                       "node <to> (both counted in signal order), without dropping."),
        QStringLiteral("from:to"));
    const QCommandLineOption demoAnimateOption(QStringLiteral("demo-animate"),
        QStringLiteral("For the D14 soak: keep the screen busy for as long as rigpa runs, lifting each "
                       "node in turn and sweeping it through the editor, never dropping it."));
    parser.addOptions({configOption, screenshotOption, viewOption, inputOption, demoDragOption, demoAnimateOption});
    parser.process(app);

    rigpa::Config config;
    QString error;
    const QString configPath = parser.isSet(configOption) ? parser.value(configOption)
                                                           : rigpa::Config::defaultPath();
    if (!config.load(configPath, &error)) {
        std::fprintf(stderr, "rigpa: %s\n", qPrintable(error));
        return 2;
    }

    const QString settingsPath = config.settingsFile();
    QDir().mkpath(QFileInfo(settingsPath).absolutePath());
    rigpa::Settings settings(settingsPath);
    rigpa::Navigator nav;
    rigpa::EngineSession engineSession;
    rigpa::Library library;
    library.setRoots(config.modelsRoot(), config.irsRoot());
    library.rescan();
    rigpa::PedaldClient pedald;
    rigpa::RigList rigs(&pedald);
    rigpa::RigSession rigSession(&engineSession, &rigs);
    rigpa::RigEditorModel editor(&rigSession, &library);
    rigpa::NodeEditorModel nodeEditor(&rigSession, &engineSession, &editor, &library);
    rigpa::PickerModel picker(&library);
    rigpa::PaletteModel palette(&engineSession, &library);
    rigpa::RigManager manager(&engineSession, &rigSession, &rigs, &pedald);
    rigpa::App appObject(&config, &settings, &nav, &engineSession, &library, &rigs, &rigSession, &editor,
                         &nodeEditor, &picker, &palette, &manager);

    // The rotary and apply in rig view: load a rig from the list.
    QObject::connect(&nav, &rigpa::Navigator::rigLoadRequested, &rigSession, &rigpa::RigSession::loadRig);

    engineSession.setMetersEnabled(settings.liveMeters());
    QObject::connect(&settings, &rigpa::Settings::liveMetersChanged, &engineSession,
                     [&] { engineSession.setMetersEnabled(settings.liveMeters()); });
    QString tokenProblem;
    const QString token = config.engineToken(&tokenProblem);
    engineSession.start(config.engineHost(), config.enginePort(), token, tokenProblem);
    pedald.setEndpoint(config.pedaldHost(), config.pedaldPort(), token);
    rigs.start();

    rigpa::DevInput devInput;
    devInput.setInverted(config.rotaryInverted());
    QObject::connect(&devInput, &rigpa::DevInput::rotated, &nav, &rigpa::Navigator::rotate);
    QObject::connect(&devInput, &rigpa::DevInput::applied, &nav, &rigpa::Navigator::apply);
    QObject::connect(&devInput, &rigpa::DevInput::backed, &nav, &rigpa::Navigator::back);
    QObject::connect(&devInput, &rigpa::DevInput::undoRequested, &rigSession, &rigpa::RigSession::undo);
    QObject::connect(&devInput, &rigpa::DevInput::redoRequested, &rigSession, &rigpa::RigSession::redo);
    app.installEventFilter(&devInput);

    if (parser.isSet(viewOption)) {
        const QString view = parser.value(viewOption);
        if (view == QLatin1String("browse"))
            nav.showMainView(rigpa::Navigator::BrowseView);
        else if (view == QLatin1String("settings"))
            nav.showMainView(rigpa::Navigator::SettingsView);
    }

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Rigpa", "Main");

    if (parser.isSet(inputOption)) {
        const QString keys = parser.value(inputOption);
        const auto replay = [&nav, &appObject, keys] {
            for (const QChar c : keys) {
                if (c == u'+') appObject.openPalette();
                else if (c == u'N') appObject.manager()->begin(QStringLiteral("new"));
                else if (c == u'R') appObject.manager()->begin(QStringLiteral("rename"));
                else if (c == u'C') appObject.manager()->begin(QStringLiteral("duplicate"));
                else if (c == u'D') appObject.manager()->begin(QStringLiteral("delete"));
                else if (c == u'>') nav.rotate(1);
                else if (c == u'<') nav.rotate(-1);
                else if (c == u'.') nav.apply();
                else if (c == u',') nav.back();
            }
        };
        // Once the library is read and the rig is on screen (or after 5 s regardless), so a
        // replay means the same thing however long the start took.
        auto *wait = new QTimer(&app);
        wait->setInterval(100);
        QObject::connect(wait, &QTimer::timeout, &app, [wait, replay, &rigSession, &library, started = QElapsedTimer()]() mutable {
            if (!started.isValid())
                started.start();
            const bool ready = rigSession.hasDocument() && !library.scanning() && started.elapsed() >= 1500;
            if (!ready && started.elapsed() < 5000)
                return;
            wait->deleteLater();
            wait->stop();
            replay();
        });
        wait->start();
    }

    if (parser.isSet(demoDragOption)) {
        const QStringList ends = parser.value(demoDragOption).split(u':');
        QTimer::singleShot(1500, &app, [&editor, ends] {
            const int from = ends.value(0).toInt(), to = ends.value(1).toInt();
            const QRectF target = editor.rectOf(to);
            if (editor.lift(editor.elementAt(from)))
                editor.dragTo(QPointF(target.left() + 12, target.center().y()));
        });
    }

    rigpa::DemoAnimator animator(&editor, &nav);
    if (parser.isSet(demoAnimateOption))
        QTimer::singleShot(1500, &app, [&animator] { animator.start(); });

    if (parser.isSet(screenshotOption)) {
        const QString path = parser.value(screenshotOption);
        // Long enough for the first frames and any opening animation to finish.
        QTimer::singleShot(3000, &app, [&engine, path] {
            const auto roots = engine.rootObjects();
            auto *window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
            const bool saved = window && window->grabWindow().save(path);
            QCoreApplication::exit(saved ? 0 : 1);
        });
    }
    return app.exec();
}
