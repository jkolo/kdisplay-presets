/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <Plasma/Applet>

#include <QAbstractListModel>

class QDBusInterface;
class QDBusArgument;

class PresetModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum PresetRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        LastUsedRole,
        OutputCountRole,
        ShortcutRole,
        ConfigurationRole,
        IsCurrentRole,
        IsAvailableRole
    };

    explicit PresetModel(QDBusInterface *presetsInterface, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refreshPresets();

private Q_SLOTS:
    void onPresetAvailabilityChanged();

private:
    QVariantMap deserializePresetData(const QDBusArgument &arg) const;
    QVariantList deserializeOutputsList(const QDBusArgument &outputsArg) const;
    QVariantMap deserializeOutputData(const QDBusArgument &outputArg) const;

    QDBusInterface *m_presetsInterface;
    QVariantList m_presets;
};

class KDisplayPresetsApplet : public Plasma::Applet
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel *presetModel READ presetModel CONSTANT FINAL)

public:
    explicit KDisplayPresetsApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
    ~KDisplayPresetsApplet() override;

    void init() override;

    QAbstractItemModel *presetModel() const;

    Q_INVOKABLE void loadPreset(const QString &presetId);

private:
    QDBusInterface *m_presetsInterface = nullptr;
    PresetModel *m_presetModel = nullptr;
};
