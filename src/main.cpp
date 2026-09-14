#include "BreakpointEditor.hpp"
#include "HydraDocument.hpp"
#include "PartialMixer.hpp"

#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQml/qqml.h>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("hYdra2"));
    QGuiApplication::setOrganizationName(QStringLiteral("mobileart.org"));
    QGuiApplication::setApplicationVersion(QStringLiteral(HYDRA2_VERSION));

    qmlRegisterType<hydra2::BreakpointEditor>("Hydra2.Native", 1, 0, "BreakpointEditor");
    qmlRegisterType<hydra2::PartialMixer>("Hydra2.Native", 1, 0, "PartialMixer");

    hydra2::HydraDocument document;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("hydraDocument"), &document);
    engine.loadFromModule("Hydra2", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    if (argc > 1)
        document.openUrl(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));

    return app.exec();
}
