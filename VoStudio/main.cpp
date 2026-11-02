#include "AudioController.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include <cstdlib>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("VoStudio"));
    QCoreApplication::setOrganizationName(QStringLiteral("Waruta"));
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QQmlApplicationEngine engine;
    auto *audioController = new AudioController(&engine);
    engine.rootContext()->setContextProperty(QStringLiteral("audioController"), audioController);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("VoStudio"), QStringLiteral("Main"));
    return application.exec();
}
