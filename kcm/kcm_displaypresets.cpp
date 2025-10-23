/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kcm_displaypresets.h"
#include "preset_manager.h"

#include <KScreen/Config>
#include <KScreen/ConfigMonitor>
#include <KScreen/GetConfigOperation>

#include <KLocalizedString>
#include <KPluginFactory>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

K_PLUGIN_CLASS_WITH_JSON(KCMDisplayPresets, "kcm_displaypresets.json")

KCMDisplayPresets::KCMDisplayPresets(QObject *parent, const KPluginMetaData &data)
    : KQuickConfigModule(parent, data)
{
    setButtons(NoAdditionalButton);

    m_presetManager = new PresetManager(this);

    // Monitor screen configuration changes
    m_configMonitor = KScreen::ConfigMonitor::instance();
    connect(m_configMonitor, &KScreen::ConfigMonitor::configurationChanged, this, &KCMDisplayPresets::updateScreenConfiguration);

    // Get initial configuration
    connect(new KScreen::GetConfigOperation(), &KScreen::GetConfigOperation::finished, this, &KCMDisplayPresets::configReady);
}

KCMDisplayPresets::~KCMDisplayPresets() = default;

PresetManager *KCMDisplayPresets::presetManager() const
{
    return m_presetManager;
}

QAbstractItemModel *KCMDisplayPresets::presetModel() const
{
    return m_presetManager->presetsModel();
}

void KCMDisplayPresets::savePreset(const QString &name, const QString &description)
{
    if (m_config) {
        m_presetManager->savePreset(name, description, m_config);
    }
}

void KCMDisplayPresets::deletePreset(const QString &presetId)
{
    m_presetManager->deletePreset(presetId);
}

void KCMDisplayPresets::renamePreset(const QString &presetId, const QString &newName)
{
    m_presetManager->renamePreset(presetId, newName);
}

void KCMDisplayPresets::updatePresetDescription(const QString &presetId, const QString &newDescription)
{
    m_presetManager->updatePresetDescription(presetId, newDescription);
}

void KCMDisplayPresets::updatePresetShortcut(const QString &presetId, const QKeySequence &shortcut)
{
    m_presetManager->updatePresetShortcut(presetId, shortcut);
}

void KCMDisplayPresets::loadPreset(const QString &presetId)
{
    // Call D-Bus service to apply the preset
    QDBusInterface interface(QStringLiteral("org.kde.kdisplaypresets"),
                             QStringLiteral("/"),
                             QStringLiteral("org.kde.kdisplaypresets"),
                             QDBusConnection::sessionBus());

    if (!interface.isValid()) {
        qWarning() << "Failed to connect to kdisplaypresets D-Bus service";
        return;
    }

    interface.asyncCall(QStringLiteral("applyPreset"), presetId);
}

bool KCMDisplayPresets::isPresetAvailable(const QString &presetId) const
{
    return m_presetManager->isPresetAvailable(presetId);
}

bool KCMDisplayPresets::isPresetCurrent(const QString &presetId) const
{
    return m_presetManager->isPresetCurrent(presetId);
}

void KCMDisplayPresets::configReady(KScreen::ConfigOperation *op)
{
    if (op->hasError()) {
        qWarning() << "Failed to get screen configuration:" << op->errorString();
        op->deleteLater();
        return;
    }

    m_config = qobject_cast<KScreen::GetConfigOperation *>(op)->config();

    if (m_config) {
        m_configMonitor->addConfig(m_config);
        m_presetManager->setScreenConfiguration(m_config);
        // Refresh preset status now that configuration is available
        m_presetManager->refreshPresetStatus();
        // Notify QML to update bindings (triggers _refreshTrigger)
        Q_EMIT outputConnect();
    }

    op->deleteLater();
}

void KCMDisplayPresets::updateScreenConfiguration()
{
    // Notify QML that outputs might have changed
    Q_EMIT outputConnect();

    // Update preset availability status
    m_presetManager->refreshPresetStatus();
}

#include "kcm_displaypresets.moc"
#include "moc_kcm_displaypresets.cpp"
