/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "common/presets.h"

#include <KScreen/Config>

class PresetManager : public QObject
{
    Q_OBJECT

public:
    explicit PresetManager(QObject *parent = nullptr);
    ~PresetManager() override = default;

    Presets *presetsModel() const;

    bool isPresetAvailable(const QString &presetId) const;
    bool isPresetCurrent(const QString &presetId) const;
    void refreshPresetStatus();

    void setScreenConfiguration(KScreen::ConfigPtr config);

    void savePreset(const QString &name, const QString &description, KScreen::ConfigPtr config);
    void deletePreset(const QString &presetId);
    void renamePreset(const QString &presetId, const QString &newName);
    void updatePresetDescription(const QString &presetId, const QString &newDescription);
    void updatePresetShortcut(const QString &presetId, const QKeySequence &shortcut);

private:
    QString generatePresetId() const;
    QVariantMap configToVariantMap(KScreen::ConfigPtr config) const;

    Presets *m_presets = nullptr;
};
