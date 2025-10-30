/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "preset_manager.h"
#include "common/utils.h"

#include <kscreen/edid.h>
#include <kscreen/mode.h>

#include <KLocalizedString>

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

PresetManager::PresetManager(QObject *parent)
    : QObject(parent)
{
    m_presets = new Presets(this);

    // Forward signals from Presets
    connect(m_presets, &Presets::presetsChanged, this, &PresetManager::presetsChanged);
    connect(m_presets, &Presets::screenConfigurationChanged, this, &PresetManager::screenConfigurationChanged);
    connect(m_presets, &Presets::loadingFailed, this, &PresetManager::loadingFailed);
    connect(m_presets, &Presets::savingFailed, this, &PresetManager::savingFailed);
}

Presets *PresetManager::presetsModel() const
{
    return m_presets;
}

// Presets interface forwarding
bool PresetManager::hasPresets() const
{
    return m_presets->hasPresets();
}

bool PresetManager::isPresetAvailable(const QString &presetId) const
{
    return m_presets->isPresetAvailable(presetId);
}

bool PresetManager::isPresetCurrent(const QString &presetId) const
{
    return m_presets->isPresetCurrent(presetId);
}

DisplayPreset PresetManager::getPreset(const QString &presetId) const
{
    return m_presets->getPreset(presetId);
}

bool PresetManager::presetExists(const QString &name) const
{
    return m_presets->presetExists(name);
}

void PresetManager::updateLastUsed(const QString &presetId)
{
    m_presets->updateLastUsed(presetId);
}

void PresetManager::refreshPresetStatus()
{
    m_presets->refreshPresetStatus();
}

KScreen::ConfigPtr PresetManager::screenConfiguration() const
{
    return m_presets->screenConfiguration();
}

void PresetManager::setScreenConfiguration(KScreen::ConfigPtr config)
{
    m_presets->setScreenConfiguration(config);
}

void PresetManager::savePreset(const QString &name, const QString &description, KScreen::ConfigPtr config)
{
    if (!config) {
        Q_EMIT errorOccurred(i18n("Invalid configuration"));
        return;
    }

    // Generate a unique ID for the preset
    const QString presetId = generatePresetId();

    // Create the preset
    DisplayPreset preset;
    preset.id = presetId;
    preset.name = name;
    preset.description = description;
    preset.created = QDateTime::currentDateTime();
    preset.lastUsed = QDateTime::currentDateTime();
    preset.configuration = configToVariantMap(config);

    // Extract output IDs
    for (const auto &output : config->outputs()) {
        if (output->isEnabled()) {
            preset.outputIds.append(output->hashMd5());
        }
    }

    // Check if preset with same name exists and replace it
    DisplayPreset *existingPreset = m_presets->findPresetByName(name);

    if (existingPreset) {
        // Replace existing preset
        preset.created = existingPreset->created; // Keep original creation date
        m_presets->updatePreset(existingPreset->id, preset);
    } else {
        // Add new preset
        m_presets->addPreset(preset);
    }

    m_presets->saveToDisk();
    Q_EMIT presetSaved(presetId);
}

void PresetManager::deletePreset(const QString &presetId)
{
    m_presets->removePreset(presetId);
    m_presets->saveToDisk();
    Q_EMIT presetDeleted(presetId);
}

void PresetManager::renamePreset(const QString &presetId, const QString &newName)
{
    DisplayPreset *preset = m_presets->findPreset(presetId);
    if (preset) {
        preset->name = newName;
        m_presets->saveToDisk();
    }
}

void PresetManager::updatePresetDescription(const QString &presetId, const QString &newDescription)
{
    DisplayPreset *preset = m_presets->findPreset(presetId);
    if (preset) {
        preset->description = newDescription;
        m_presets->saveToDisk();
    }
}

void PresetManager::updatePresetShortcut(const QString &presetId, const QKeySequence &shortcut)
{
    DisplayPreset *preset = m_presets->findPreset(presetId);
    if (preset) {
        preset->shortcut = shortcut;
        m_presets->saveToDisk();
    }
}

QString PresetManager::generatePresetId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QVariantMap PresetManager::configToVariantMap(KScreen::ConfigPtr config) const
{
    QVariantMap configMap;

    // Store supported features
    configMap[QStringLiteral("features")] = static_cast<int>(config->supportedFeatures());
    configMap[QStringLiteral("tabletModeEngaged")] = config->tabletModeEngaged();

    // Store outputs (only enabled ones)
    QVariantList outputsList;
    for (const auto &output : config->outputs()) {
        // Skip disabled outputs - we only store enabled displays in presets
        if (!output->isEnabled()) {
            continue;
        }

        QVariantMap outputMap;
        outputMap[QStringLiteral("id")] = output->hashMd5();
        outputMap[QStringLiteral("name")] = output->name();
        outputMap[QStringLiteral("model")] = output->model();
        outputMap[QStringLiteral("vendor")] = output->vendor();
        outputMap[QStringLiteral("type")] = static_cast<int>(output->type());
        outputMap[QStringLiteral("displayName")] = Utils::outputName(output.get());
        outputMap[QStringLiteral("connected")] = output->isConnected();
        outputMap[QStringLiteral("enabled")] = output->isEnabled();
        outputMap[QStringLiteral("primary")] = output->isPrimary();
        outputMap[QStringLiteral("priority")] = output->priority();

        // Position and size
        outputMap[QStringLiteral("pos")] = QVariantMap{{QStringLiteral("x"), output->pos().x()}, {QStringLiteral("y"), output->pos().y()}};
        outputMap[QStringLiteral("scale")] = output->scale();
        outputMap[QStringLiteral("rotation")] = static_cast<int>(output->rotation());

        // Logical size (for plasmoid)
        const QSizeF logicalSize = output->explicitLogicalSize();
        if (!logicalSize.isEmpty()) {
            outputMap[QStringLiteral("explicitLogicalSize")] = true;
            outputMap[QStringLiteral("logicalSize")] =
                QVariantMap{{QStringLiteral("width"), logicalSize.width()}, {QStringLiteral("height"), logicalSize.height()}};
        } else {
            outputMap[QStringLiteral("explicitLogicalSize")] = false;
        }

        // Mode
        if (output->currentMode()) {
            QVariantMap modeMap;
            modeMap[QStringLiteral("id")] = output->currentModeId();
            modeMap[QStringLiteral("width")] = output->currentMode()->size().width();
            modeMap[QStringLiteral("height")] = output->currentMode()->size().height();
            modeMap[QStringLiteral("refreshRate")] = output->currentMode()->refreshRate();
            outputMap[QStringLiteral("mode")] = modeMap;
            outputMap[QStringLiteral("currentModeId")] = output->currentModeId();
        }

        // Additional settings
        outputMap[QStringLiteral("overscan")] = output->overscan();
        outputMap[QStringLiteral("vrrPolicy")] = static_cast<int>(output->vrrPolicy());
        outputMap[QStringLiteral("rgbRange")] = static_cast<int>(output->rgbRange());
        outputMap[QStringLiteral("hdr")] = output->isHdrEnabled();
        outputMap[QStringLiteral("sdr_brightness")] = output->sdrBrightness();
        outputMap[QStringLiteral("wide_color_gamut")] = output->isWcgEnabled();
        outputMap[QStringLiteral("icc_profile_path")] = output->iccProfilePath();
        outputMap[QStringLiteral("brightness")] = output->brightness();
        outputMap[QStringLiteral("auto_rotate_policy")] = static_cast<int>(output->autoRotatePolicy());
        outputMap[QStringLiteral("capabilities")] = static_cast<int>(output->capabilities());
        outputMap[QStringLiteral("edr_policy")] = static_cast<int>(output->edrPolicy());

        outputsList.append(outputMap);
    }
    configMap[QStringLiteral("outputs")] = outputsList;

    return configMap;
}
