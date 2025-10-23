/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "common/presets.h"

#include <QAction>
#include <QObject>
#include <QTimer>

namespace KScreen
{
class ConfigMonitor;
class ConfigOperation;
}

class PresetsService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdisplaypresets")

public:
    explicit PresetsService(QObject *parent = nullptr, const QString &customPresetsFile = QString());
    ~PresetsService() override;

    bool init();

public Q_SLOTS:
    Q_SCRIPTABLE void applyPreset(const QString &presetId);
    Q_SCRIPTABLE QVariantList getPresets();

Q_SIGNALS:
    Q_SCRIPTABLE void presetsChanged(const QVariantList &changedPresets);
    void presetApplied(const QString &presetId);
    void errorOccurred(const QString &error);

private Q_SLOTS:
    void configChanged();
    void configReady(KScreen::ConfigOperation *op);
    void initShortcuts();
    void onPresetsModelChanged();

private:
    void updatePresetScreenConfiguration();
    void emitPresetsChanged(const QStringList &changedPresetIds = {});
    QVariantMap getPresetInfo(const QString &presetId) const;
    QHash<QString, QVariantMap> buildPresetOutputsMap(const QVariantList &presetOutputsList) const;
    void applyPresetToOutput(const KScreen::OutputPtr &output, const QVariantMap &presetOutputMap, KScreen::ConfigPtr config) const;
    void registerShortcut(const QString &presetId, const QKeySequence &shortcut);
    void unregisterShortcut(const QString &presetId);
    QStringList detectChangedPresets() const;

    Presets *m_presets = nullptr;
    KScreen::ConfigMonitor *m_configMonitor = nullptr;
    QTimer *m_configUpdateTimer = nullptr;
    QHash<QString, QAction *> m_shortcutActions;
    QVariantList m_previousPresets; // Cache of previous presets for change detection
};
