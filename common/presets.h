/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KScreen/Config>
#include <KScreen/Mode>
#include <KScreen/Output>

#include <QAbstractListModel>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QKeySequence>
#include <QString>
#include <QStringList>
#include <QVariantMap>

struct DisplayPreset {
    QString id;
    QString name;
    QString description;
    QDateTime created;
    QDateTime lastUsed;
    QStringList outputIds;
    QVariantMap configuration;
    QKeySequence shortcut;

    bool operator==(const DisplayPreset &other) const
    {
        return id == other.id;
    }
};

class Presets : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY presetsChanged)
    Q_PROPERTY(bool hasPresets READ hasPresets NOTIFY presetsChanged)
    Q_PROPERTY(KScreen::ConfigPtr screenConfiguration READ screenConfiguration WRITE setScreenConfiguration)

public:
    enum PresetRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        CreatedRole,
        LastUsedRole,
        OutputCountRole,
        ConfigurationRole,
        ShortcutRole,
    };
    Q_ENUM(PresetRoles)

    explicit Presets(QObject *parent = nullptr, const QString &customFilePath = QString());
    ~Presets() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool hasPresets() const;

    Q_INVOKABLE bool isPresetAvailable(const QString &presetId) const;
    Q_INVOKABLE bool isPresetCurrent(const QString &presetId) const;
    KScreen::ConfigPtr screenConfiguration() const;
    void setScreenConfiguration(KScreen::ConfigPtr config);

    Q_INVOKABLE DisplayPreset getPreset(const QString &presetId) const;
    Q_INVOKABLE bool presetExists(const QString &name) const;
    Q_INVOKABLE void updateLastUsed(const QString &presetId);
    Q_INVOKABLE void refreshPresetStatus();

    void setCustomPresetsFilePath(const QString &filePath);
    void reloadPresets();

    // Methods for preset manipulation
    void addPreset(const DisplayPreset &preset);
    void updatePreset(const QString &presetId, const DisplayPreset &preset);
    void removePreset(const QString &presetId);
    DisplayPreset *findPreset(const QString &presetId);
    DisplayPreset *findPresetByName(const QString &name);
    void saveToDisk();

Q_SIGNALS:
    void presetsChanged();
    void screenConfigurationChanged();
    void loadingFailed(const QString &error);
    void savingFailed(const QString &error);

protected:
    void loadPresetsFromDisk();
    void savePresetsToDisk();
    QString presetsFilePath() const;

private Q_SLOTS:
    void onPresetFileChanged();

protected:
    QList<DisplayPreset> m_presets;
    KScreen::ConfigPtr m_screenConfiguration;

private:
    QFileSystemWatcher *m_fileWatcher;
    QString m_customPresetsFilePath;
};
