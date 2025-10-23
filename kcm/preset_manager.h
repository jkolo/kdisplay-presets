/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "common/presets.h"
#include <QAbstractListModel>
#include <kscreen/config.h>

class PresetManager : public QObject
{
    Q_OBJECT

public:
    explicit PresetManager(QObject *parent = nullptr);
    ~PresetManager() override = default;

    // Get the presets model for QML
    Q_INVOKABLE Presets *presetsModel() const;

    // Presets interface forwarding
    Q_INVOKABLE bool hasPresets() const;
    Q_INVOKABLE bool isPresetAvailable(const QString &presetId) const;
    Q_INVOKABLE bool isPresetCurrent(const QString &presetId) const;
    Q_INVOKABLE DisplayPreset getPreset(const QString &presetId) const;
    Q_INVOKABLE bool presetExists(const QString &name) const;
    Q_INVOKABLE void updateLastUsed(const QString &presetId);
    Q_INVOKABLE void refreshPresetStatus();

    KScreen::ConfigPtr screenConfiguration() const;
    void setScreenConfiguration(KScreen::ConfigPtr config);

    Q_INVOKABLE void savePreset(const QString &name, const QString &description, KScreen::ConfigPtr config);
    Q_INVOKABLE void deletePreset(const QString &presetId);
    Q_INVOKABLE void renamePreset(const QString &presetId, const QString &newName);
    Q_INVOKABLE void updatePresetDescription(const QString &presetId, const QString &newDescription);
    Q_INVOKABLE void updatePresetShortcut(const QString &presetId, const QKeySequence &shortcut);

Q_SIGNALS:
    void presetSaved(const QString &presetId);
    void presetDeleted(const QString &presetId);
    void errorOccurred(const QString &error);

    // Forward signals from Presets
    void presetsChanged();
    void screenConfigurationChanged();
    void loadingFailed(const QString &error);
    void savingFailed(const QString &error);

private:
    QString generatePresetId() const;
    QVariantMap configToVariantMap(KScreen::ConfigPtr config) const;

    Presets *m_presets = nullptr;
};
