#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>
#include "backend.h"
#include "cli.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    const bool cli = argc > 1 && QByteArray(argv[1]) != "open";
    if (cli) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        // Desktop platform themes may initialize GTK and require a display.
        qputenv("QT_QPA_PLATFORMTHEME", "generic");
    }
    QGuiApplication app(argc, argv);
    app.setApplicationName("jot");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("jot");
    app.setWindowIcon(QIcon::fromTheme("jot", QIcon(":/jot.svg")));
    QQuickStyle::setStyle("Material");
    Translator translator;
    app.installTranslator(&translator);
    if (cli) return runCli(app.arguments());

    Theme theme;
    Backend backend;
    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &backend, &Backend::flush);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
