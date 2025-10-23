/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "presetsservice.h"
#include "kdisplaypresets_daemon_debug.h"

#include <KScreen/Config>
#include <KScreen/ConfigMonitor>
#include <KScreen/GetConfigOperation>
#include <KScreen/Mode>
#include <KScreen/Output>
#include <KScreen/SetConfigOperation>

#include <KGlobalAccel>

#include <QDBusConnection>
#include <QDBusMetaType>
#include <QTimer>
#include <stdexcept>

PresetsService::PresetsService(QObject *parent, const QString &customPresetsFile)
    : QObject(parent)
{
    m_presets = new Presets(this, customPresetsFile);

    m_configMonitor = KScreen::ConfigMonitor::instance();
    connect(m_configMonitor, &KScreen::ConfigMonitor::configurationChanged, this, &PresetsService::configChanged);

    // Timer to debounce rapid config changes
    m_configUpdateTimer = new QTimer(this);
    m_configUpdateTimer->setSingleShot(true);
    m_configUpdateTimer->setInterval(500); // 500ms delay
    connect(m_configUpdateTimer, &QTimer::timeout, this, &PresetsService::updatePresetScreenConfiguration);

    // Initialize shortcuts and emit D-Bus signal when presets change
    connect(m_presets, &Presets::presetsChanged, this, &PresetsService::initShortcuts);
    connect(m_presets, &Presets::presetsChanged, this, &PresetsService::onPresetsModelChanged);
}

PresetsService::~PresetsService() = default;

bool PresetsService::init()
{
    qCDebug(KDISPLAYPRESETS_DAEMON) << "Initializing PresetsService";

    // Register D-Bus meta types for complex data types
    qDBusRegisterMetaType<QVariantList>();
    qDBusRegisterMetaType<QVariantMap>();

    if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/"),
                                                      this,
                                                      QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals)) {
        qCCritical(KDISPLAYPRESETS_DAEMON) << "Failed to register D-Bus object";
        return false;
    }

    if (!QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.kdisplaypresets"))) {
        qCCritical(KDISPLAYPRESETS_DAEMON) << "Failed to register D-Bus service";
        return false;
    }

    // Initialize cache with current presets
    m_previousPresets = getPresets();

    // Get initial screen configuration
    updatePresetScreenConfiguration();

    qCDebug(KDISPLAYPRESETS_DAEMON) << "PresetsService initialized successfully";
    return true;
}

void PresetsService::configChanged()
{
    // Restart timer on each config change to debounce rapid changes
    m_configUpdateTimer->start();
}

void PresetsService::updatePresetScreenConfiguration()
{
    qCDebug(KDISPLAYPRESETS_DAEMON) << "Updating preset screen configuration";

    // Use KCM-style GetConfigOperation approach
    connect(new KScreen::GetConfigOperation(), &KScreen::GetConfigOperation::finished, this, &PresetsService::configReady);
}

void PresetsService::configReady(KScreen::ConfigOperation *op)
{
    qCDebug(KDISPLAYPRESETS_DAEMON) << "Config operation finished";

    if (op->hasError()) {
        qCWarning(KDISPLAYPRESETS_DAEMON) << "GetConfigOperation failed:" << op->errorString();
        op->deleteLater();
        return;
    }

    auto config = qobject_cast<KScreen::GetConfigOperation *>(op)->config();
    qCDebug(KDISPLAYPRESETS_DAEMON) << "GetConfigOperation successful, outputs count:" << (config ? config->outputs().count() : 0);

    if (config) {
        // Add config to monitor like KDED and KCM do
        KScreen::ConfigMonitor::instance()->addConfig(config);

        m_presets->setScreenConfiguration(config);
        emitPresetsChanged();
        qCDebug(KDISPLAYPRESETS_DAEMON) << "Screen configuration updated successfully";
    } else {
        qCWarning(KDISPLAYPRESETS_DAEMON) << "Failed to update screen configuration - missing config";
    }

    op->deleteLater();
}

void PresetsService::applyPreset(const QString &presetId)
{
    qCDebug(KDISPLAYPRESETS_DAEMON) << "Applying preset:" << presetId;

    if (!m_presets->isPresetAvailable(presetId)) {
        const QString error = QStringLiteral("Preset not available: %1").arg(presetId);
        qCWarning(KDISPLAYPRESETS_DAEMON) << error;
        Q_EMIT errorOccurred(error);
        return;
    }

    // Find preset data
    QVariantMap presetData;
    for (int i = 0; i < m_presets->rowCount(); ++i) {
        const QModelIndex idx = m_presets->index(i, 0);
        if (m_presets->data(idx, Presets::IdRole).toString() == presetId) {
            presetData = m_presets->data(idx, Presets::ConfigurationRole).toMap();
            break;
        }
    }

    if (presetData.isEmpty()) {
        const QString error = QStringLiteral("Preset data not found: %1").arg(presetId);
        qCWarning(KDISPLAYPRESETS_DAEMON) << error;
        Q_EMIT errorOccurred(error);
        return;
    }

    // Get current config and apply preset
    auto *getConfigOp = new KScreen::GetConfigOperation();
    connect(getConfigOp, &KScreen::GetConfigOperation::finished, this, [this, presetId, presetData](KScreen::ConfigOperation *op) {
        if (op->hasError()) {
            const QString error = QStringLiteral("Failed to get current config: %1").arg(op->errorString());
            qCWarning(KDISPLAYPRESETS_DAEMON) << error;
            Q_EMIT errorOccurred(error);
            op->deleteLater();
            return;
        }

        auto config = qobject_cast<KScreen::GetConfigOperation *>(op)->config();
        if (!config) {
            const QString error = QStringLiteral("Invalid config received");
            qCWarning(KDISPLAYPRESETS_DAEMON) << error;
            Q_EMIT errorOccurred(error);
            op->deleteLater();
            return;
        }

        // Apply preset configuration
        const QVariantList outputsList = presetData.value(QStringLiteral("outputs")).toList();
        const QHash<QString, QVariantMap> presetOutputsMap = buildPresetOutputsMap(outputsList);

        const auto outputs = config->outputs();
        for (const auto &output : outputs) {
            if (presetOutputsMap.contains(output->name())) {
                applyPresetToOutput(output, presetOutputsMap[output->name()], config);
            }
        }

        // Apply the configuration
        auto *setConfigOp = new KScreen::SetConfigOperation(config);
        connect(setConfigOp, &KScreen::SetConfigOperation::finished, this, [this, presetId](KScreen::ConfigOperation *setOp) {
            if (setOp->hasError()) {
                const QString error = QStringLiteral("Failed to apply preset: %1").arg(setOp->errorString());
                qCWarning(KDISPLAYPRESETS_DAEMON) << error;
                Q_EMIT errorOccurred(error);
            } else {
                // Update last used timestamp
                m_presets->updateLastUsed(presetId);
                qCDebug(KDISPLAYPRESETS_DAEMON) << "Preset applied successfully:" << presetId;
                Q_EMIT presetApplied(presetId);
            }
            setOp->deleteLater();
        });

        op->deleteLater();
    });
}

QVariantList PresetsService::getPresets()
{
    QVariantList presets;

    for (int i = 0; i < m_presets->rowCount(); ++i) {
        const QModelIndex index = m_presets->index(i, 0);
        QVariantMap preset;

        const QString presetId = m_presets->data(index, Presets::IdRole).toString();
        preset[QStringLiteral("presetId")] = presetId;
        preset[QStringLiteral("name")] = m_presets->data(index, Presets::NameRole).toString();
        preset[QStringLiteral("description")] = m_presets->data(index, Presets::DescriptionRole).toString();

        // Convert QDateTime to string for D-Bus serialization
        QDateTime lastUsed = m_presets->data(index, Presets::LastUsedRole).toDateTime();
        preset[QStringLiteral("lastUsed")] = lastUsed.toString(Qt::ISODate);

        preset[QStringLiteral("outputCount")] = m_presets->data(index, Presets::OutputCountRole).toInt();

        // Add configuration data
        preset[QStringLiteral("configuration")] = m_presets->data(index, Presets::ConfigurationRole);

        // Convert QKeySequence to string for D-Bus serialization
        QKeySequence shortcut = m_presets->data(index, Presets::ShortcutRole).value<QKeySequence>();
        preset[QStringLiteral("shortcut")] = shortcut.toString();

        // Check if preset is available and current
        preset[QStringLiteral("isAvailable")] = m_presets->isPresetAvailable(presetId);
        preset[QStringLiteral("isCurrent")] = m_presets->isPresetCurrent(presetId);

        presets.append(preset);
    }

    return presets;
}

void PresetsService::emitPresetsChanged(const QStringList &changedPresetIds)
{
    QVariantList changedPresets;

    if (changedPresetIds.isEmpty()) {
        // If no specific presets specified, emit all presets with current status
        for (int i = 0; i < m_presets->rowCount(); ++i) {
            const QModelIndex index = m_presets->index(i, 0);
            const QString presetId = m_presets->data(index, Presets::IdRole).toString();
            changedPresets.append(getPresetInfo(presetId));
        }
    } else {
        // Emit only specified presets
        for (const QString &presetId : changedPresetIds) {
            changedPresets.append(getPresetInfo(presetId));
        }
    }

    Q_EMIT presetsChanged(changedPresets);
}

QVariantMap PresetsService::getPresetInfo(const QString &presetId) const
{
    QVariantMap preset;

    // Find preset in model
    for (int i = 0; i < m_presets->rowCount(); ++i) {
        const QModelIndex index = m_presets->index(i, 0);
        if (m_presets->data(index, Presets::IdRole).toString() == presetId) {
            preset[QStringLiteral("presetId")] = presetId;
            preset[QStringLiteral("name")] = m_presets->data(index, Presets::NameRole).toString();
            preset[QStringLiteral("description")] = m_presets->data(index, Presets::DescriptionRole).toString();

            // Convert QDateTime to string for D-Bus serialization
            QDateTime lastUsed = m_presets->data(index, Presets::LastUsedRole).toDateTime();
            preset[QStringLiteral("lastUsed")] = lastUsed.toString(Qt::ISODate);

            preset[QStringLiteral("outputCount")] = m_presets->data(index, Presets::OutputCountRole).toInt();

            // Add configuration data
            preset[QStringLiteral("configuration")] = m_presets->data(index, Presets::ConfigurationRole);

            // Convert QKeySequence to string for D-Bus serialization
            QKeySequence shortcut = m_presets->data(index, Presets::ShortcutRole).value<QKeySequence>();
            preset[QStringLiteral("shortcut")] = shortcut.toString();

            // Check current status
            preset[QStringLiteral("isAvailable")] = m_presets->isPresetAvailable(presetId);
            preset[QStringLiteral("isCurrent")] = m_presets->isPresetCurrent(presetId);
            preset[QStringLiteral("deleted")] = false;
            return preset;
        }
    }

    // Preset not found - mark as deleted
    preset[QStringLiteral("presetId")] = presetId;
    preset[QStringLiteral("deleted")] = true;
    return preset;
}

void PresetsService::initShortcuts()
{
    // Clear existing shortcuts
    for (auto action : m_shortcutActions) {
        KGlobalAccel::self()->removeAllShortcuts(action);
        action->deleteLater();
    }
    m_shortcutActions.clear();

    // Register shortcuts for all presets
    for (int i = 0; i < m_presets->rowCount(); ++i) {
        const QModelIndex idx = m_presets->index(i, 0);
        const QString presetId = m_presets->data(idx, Presets::IdRole).toString();
        const QKeySequence shortcut = m_presets->data(idx, Presets::ShortcutRole).value<QKeySequence>();

        if (!shortcut.isEmpty()) {
            registerShortcut(presetId, shortcut);
        }
    }
}

QHash<QString, QVariantMap> PresetsService::buildPresetOutputsMap(const QVariantList &presetOutputsList) const
{
    QHash<QString, QVariantMap> outputsMap;
    for (const auto &outputVariant : presetOutputsList) {
        const QVariantMap outputMap = outputVariant.toMap();
        const QString outputName = outputMap.value(QStringLiteral("name")).toString();
        if (!outputName.isEmpty()) {
            outputsMap[outputName] = outputMap;
        }
    }
    return outputsMap;
}

void PresetsService::applyPresetToOutput(const KScreen::OutputPtr &output, const QVariantMap &presetOutputMap, KScreen::ConfigPtr config) const
{
    Q_UNUSED(config)

    // Apply basic output settings
    const bool enabled = presetOutputMap.value(QStringLiteral("enabled"), false).toBool();
    output->setEnabled(enabled);

    if (!enabled) {
        return;
    }

    // Apply position
    const QVariantMap posMap = presetOutputMap.value(QStringLiteral("pos")).toMap();
    const QPoint position(posMap.value(QStringLiteral("x"), 0).toInt(), posMap.value(QStringLiteral("y"), 0).toInt());
    output->setPos(position);

    // Apply mode
    const QString modeId = presetOutputMap.value(QStringLiteral("currentModeId")).toString();
    if (!modeId.isEmpty() && output->modes().contains(modeId)) {
        output->setCurrentModeId(modeId);
    }

    // Apply rotation
    const int rotation = presetOutputMap.value(QStringLiteral("rotation"), 1).toInt();
    output->setRotation(static_cast<KScreen::Output::Rotation>(rotation));

    // Apply scale
    const qreal scale = presetOutputMap.value(QStringLiteral("scale"), 1.0).toReal();
    output->setScale(scale);

    // Apply primary status
    const bool primary = presetOutputMap.value(QStringLiteral("primary"), false).toBool();
    output->setPrimary(primary);
}

void PresetsService::registerShortcut(const QString &presetId, const QKeySequence &shortcut)
{
    if (shortcut.isEmpty()) {
        return;
    }

    auto action = new QAction(this);
    action->setObjectName(QStringLiteral("preset_%1").arg(presetId));
    action->setText(QStringLiteral("Apply Display Preset"));

    connect(action, &QAction::triggered, this, [this, presetId]() {
        applyPreset(presetId);
    });

    KGlobalAccel::self()->setShortcut(action, {shortcut});
    m_shortcutActions[presetId] = action;
}

void PresetsService::unregisterShortcut(const QString &presetId)
{
    auto it = m_shortcutActions.find(presetId);
    if (it != m_shortcutActions.end()) {
        KGlobalAccel::self()->removeAllShortcuts(it.value());
        it.value()->deleteLater();
        m_shortcutActions.erase(it);
    }
}

void PresetsService::onPresetsModelChanged()
{
    const QStringList changedPresetIds = detectChangedPresets();
    if (!changedPresetIds.isEmpty()) {
        emitPresetsChanged(changedPresetIds);
    }

    // Update cached presets for next comparison
    m_previousPresets = getPresets();
}

QStringList PresetsService::detectChangedPresets() const
{
    QStringList changedIds;

    // We need to cast away const to call getPresets() - this is safe for read-only access
    const QVariantList currentPresets = const_cast<PresetsService *>(this)->getPresets();

    // Convert lists to maps for easier comparison
    QHash<QString, QVariantMap> previousMap;
    for (const auto &preset : m_previousPresets) {
        const QVariantMap presetMap = preset.toMap();
        previousMap[presetMap.value(QStringLiteral("presetId")).toString()] = presetMap;
    }

    QHash<QString, QVariantMap> currentMap;
    for (const auto &preset : currentPresets) {
        const QVariantMap presetMap = preset.toMap();
        currentMap[presetMap.value(QStringLiteral("presetId")).toString()] = presetMap;
    }

    // Check for new or modified presets
    for (auto it = currentMap.constBegin(); it != currentMap.constEnd(); ++it) {
        const QString presetId = it.key();
        const QVariantMap currentPreset = it.value();

        if (!previousMap.contains(presetId)) {
            // New preset
            changedIds.append(presetId);
        } else {
            // Check if preset was modified
            const QVariantMap previousPreset = previousMap.value(presetId);

            // Compare key fields (excluding isAvailable/isCurrent which change based on screen config)
            const QStringList fieldsToCompare = {QStringLiteral("name"), QStringLiteral("description"), QStringLiteral("shortcut"), QStringLiteral("lastUsed")};

            bool isModified = false;
            for (const QString &field : fieldsToCompare) {
                if (currentPreset.value(field) != previousPreset.value(field)) {
                    isModified = true;
                    break;
                }
            }

            if (isModified) {
                changedIds.append(presetId);
            }
        }
    }

    // Check for deleted presets
    for (auto it = previousMap.constBegin(); it != previousMap.constEnd(); ++it) {
        const QString presetId = it.key();
        if (!currentMap.contains(presetId)) {
            // Preset was deleted - emit it with deleted flag
            changedIds.append(presetId);
        }
    }

    return changedIds;
}

#include "moc_presetsservice.cpp"
