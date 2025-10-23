/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QGuiApplication>

#include "kdisplaypresets_daemon_debug.h"
#include "presetsservice.h"

QString parseCommandLineArguments(QGuiApplication &app)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("KDE Display Presets Service");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption presetsFileOption(QStringList() << "p" << "presets-file", "Use custom presets file path instead of default location", "file");
    parser.addOption(presetsFileOption);

    parser.process(app);

    const QString customFilePath = parser.isSet(presetsFileOption) ? parser.value(presetsFileOption) : QString();

    if (!customFilePath.isEmpty()) {
        qCDebug(KDISPLAYPRESETS_DAEMON) << "Custom presets file specified:" << customFilePath;
    }

    return customFilePath;
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("kdisplaypresets_daemon");
    app.setApplicationVersion("1.0");

    const QString customFilePath = parseCommandLineArguments(app);

    if (!QDBusConnection::sessionBus().isConnected()) {
        qCCritical(KDISPLAYPRESETS_DAEMON) << "Cannot connect to the D-Bus session bus."
                                    << "To start it, run: eval `dbus-launch --auto-syntax`";
        return 1;
    }

    PresetsService service(nullptr, customFilePath);

    if (!service.init()) {
        qCCritical(KDISPLAYPRESETS_DAEMON) << "Failed to initialize PresetsService";
        return 1;
    }

    return app.exec();
}
