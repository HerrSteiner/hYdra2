/*
Copyright (C) 2026 Malte Steiner

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "BreakpointEditor.hpp"
#include "BreakpointGraphModel.hpp"
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
    qmlRegisterType<hydra2::BreakpointGraphModel>("Hydra2.Native", 1, 0, "BreakpointGraphModel");
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
