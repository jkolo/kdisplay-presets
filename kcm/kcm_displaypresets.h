/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KQuickConfigModule>
#include <kscreen/types.h>
#include <QAbstractItemModel>

class PresetManager;

namespace KScreen
{
class ConfigMonitor;
class ConfigOperation;
}

class KCMDisplayPresets : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(PresetManager *presetManager READ presetManager CONSTANT)
    Q_PROPERTY(QAbstractItemModel *presetModel READ presetModel CONSTANT)

public:
    explicit KCMDisplayPresets(QObject *parent, const KPluginMetaData &data);
    ~KCMDisplayPresets() override;

    PresetManager *presetManager() const;
    QAbstractItemModel *presetModel() const;

    Q_INVOKABLE void savePreset(const QString &name, const QString &description);
    Q_INVOKABLE void deletePreset(const QString &presetId);
    Q_INVOKABLE void renamePreset(const QString &presetId, const QString &newName);
    Q_INVOKABLE void updatePresetDescription(const QString &presetId, const QString &newDescription);
    Q_INVOKABLE void updatePresetShortcut(const QString &presetId, const QKeySequence &shortcut);
    Q_INVOKABLE void loadPreset(const QString &presetId);
    Q_INVOKABLE bool isPresetAvailable(const QString &presetId) const;
    Q_INVOKABLE bool isPresetCurrent(const QString &presetId) const;

Q_SIGNALS:
    void outputConnect();

private Q_SLOTS:
    void configReady(KScreen::ConfigOperation *op);
    void updateScreenConfiguration();

private:
    PresetManager *m_presetManager = nullptr;
    KScreen::ConfigPtr m_config;
    KScreen::ConfigMonitor *m_configMonitor = nullptr;
};
