#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("rigpa"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("hemsley.cc"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("rigpa, the pedal's UI"));
    parser.addHelpOption();
    const QCommandLineOption screenshot(
        QStringLiteral("screenshot"),
        QStringLiteral("Render, save the window to <file> (PNG) once it has settled, and quit."),
        QStringLiteral("file"));
    parser.addOption(screenshot);
    parser.process(app);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Rigpa", "Main");

    if (parser.isSet(screenshot)) {
        const QString path = parser.value(screenshot);
        // Long enough for the first frames and any opening animation to finish.
        QTimer::singleShot(1500, &app, [&engine, path] {
            const auto roots = engine.rootObjects();
            auto *window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
            const bool saved = window && window->grabWindow().save(path);
            QCoreApplication::exit(saved ? 0 : 1);
        });
    }
    return app.exec();
}
