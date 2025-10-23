/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kdisplaypresets_applet.h"
#include "kdisplaypresets_applet_debug.h"

#include <QMetaEnum>
#include <QQmlEngine> // for qmlRegisterType

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

#include <algorithm>

K_PLUGIN_CLASS_WITH_JSON(KDisplayPresetsApplet, "metadata.json")

KDisplayPresetsApplet::KDisplayPresetsApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plasma::Applet(parent, data, args)
    , m_presetsInterface(new QDBusInterface(QStringLiteral("org.kde.kdisplaypresets"),
                                            QStringLiteral("/"),
                                            QStringLiteral("org.kde.kdisplaypresets"),
                                            QDBusConnection::sessionBus(),
                                            this))
    , m_presetModel(new PresetModel(m_presetsInterface, this))
{
}

KDisplayPresetsApplet::~KDisplayPresetsApplet() = default;

void KDisplayPresetsApplet::init()
{
    // Initial load of presets
    if (m_presetModel) {
        m_presetModel->refreshPresets();
    }
}

QAbstractItemModel *KDisplayPresetsApplet::presetModel() const
{
    return m_presetModel;
}

void KDisplayPresetsApplet::loadPreset(const QString &presetId)
{
    if (!m_presetsInterface || !m_presetsInterface->isValid()) {
        return;
    }

    m_presetsInterface->asyncCall(QStringLiteral("applyPreset"), presetId);
}

// PresetModel implementation
PresetModel::PresetModel(QDBusInterface *presetsInterface, QObject *parent)
    : QAbstractListModel(parent)
    , m_presetsInterface(presetsInterface)
{
    // Connect to D-Bus signal for preset availability changes
    if (m_presetsInterface && m_presetsInterface->isValid()) {
        qCDebug(KDISPLAYPRESETS_APPLET) << "PresetModel: D-Bus interface is valid, connecting signals and loading presets";
        QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.kdisplaypresets"),
                                              QStringLiteral("/"),
                                              QStringLiteral("org.kde.kdisplaypresets"),
                                              QStringLiteral("presetsChanged"),
                                              this,
                                              SLOT(onPresetAvailabilityChanged()));

        // Load initial presets
        refreshPresets();
    } else {
        qCWarning(KDISPLAYPRESETS_APPLET) << "PresetModel: D-Bus interface is NOT valid!"
                                  << "Interface exists:" << (m_presetsInterface != nullptr)
                                  << "Is valid:" << (m_presetsInterface ? m_presetsInterface->isValid() : false)
                                  << "Service:" << (m_presetsInterface ? m_presetsInterface->service() : "null")
                                  << "Last error:" << (m_presetsInterface ? m_presetsInterface->lastError().message() : "no interface");
    }
}

int PresetModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_presets.count();
}

QVariant PresetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_presets.count()) {
        return QVariant();
    }

    const QVariant &presetVariant = m_presets.at(index.row());
    const QVariantMap preset = presetVariant.toMap();

    switch (role) {
    case IdRole:
        return preset[QStringLiteral("presetId")];
    case NameRole:
        return preset[QStringLiteral("name")];
    case DescriptionRole:
        return preset[QStringLiteral("description")];
    case LastUsedRole:
        return preset[QStringLiteral("lastUsed")];
    case OutputCountRole:
        return preset[QStringLiteral("outputCount")];
    case ShortcutRole:
        return preset[QStringLiteral("shortcut")];
    case ConfigurationRole: {
        QVariant configVar = preset[QStringLiteral("configuration")];
        if (configVar.toMap().contains(QStringLiteral("outputs"))) {
            QVariantList outputs = configVar.toMap()[QStringLiteral("outputs")].toList();
            qCDebug(KDISPLAYPRESETS_APPLET) << "PresetModel: Returning configuration with" << outputs.count() << "outputs for preset"
                                    << preset[QStringLiteral("name")].toString();
        }
        return configVar;
    }
    case IsCurrentRole:
        return preset[QStringLiteral("isCurrent")];
    case IsAvailableRole:
        return preset[QStringLiteral("isAvailable")];
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PresetModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "presetId";
    roles[NameRole] = "name";
    roles[DescriptionRole] = "description";
    roles[LastUsedRole] = "lastUsed";
    roles[OutputCountRole] = "outputCount";
    roles[ShortcutRole] = "shortcut";
    roles[ConfigurationRole] = "configuration";
    roles[IsCurrentRole] = "isCurrent";
    roles[IsAvailableRole] = "isAvailable";
    return roles;
}

void PresetModel::refreshPresets()
{
    if (!m_presetsInterface || !m_presetsInterface->isValid()) {
        qCWarning(KDISPLAYPRESETS_APPLET) << "PresetModel::refreshPresets() - interface not valid";
        return;
    }

    QDBusReply<QVariantList> reply = m_presetsInterface->call(QStringLiteral("getPresets"));
    if (reply.isValid()) {
        QVariantList presets;
        const QVariantList &rawPresets = reply.value();

        // Process each preset
        for (const QVariant &rawPreset : rawPresets) {
            QVariantMap preset;

            if (rawPreset.canConvert<QDBusArgument>()) {
                preset = deserializePresetData(rawPreset.value<QDBusArgument>());
            } else if (rawPreset.canConvert<QVariantMap>()) {
                preset = rawPreset.toMap();
            }

            presets.append(preset);
        }

        beginResetModel();
        m_presets = presets;
        endResetModel();
    } else {
        qCWarning(KDISPLAYPRESETS_APPLET) << "PresetModel::refreshPresets() - D-Bus call failed:" << reply.error().message();
    }
}

void PresetModel::onPresetAvailabilityChanged()
{
    qCDebug(KDISPLAYPRESETS_APPLET) << "PresetModel: Received presetsChanged signal, refreshing presets";
    refreshPresets();
}

QVariantMap PresetModel::deserializePresetData(const QDBusArgument &arg) const
{
    QVariantMap preset;
    arg >> preset;

    // Deserialize nested configuration if it's QDBusArgument
    if (preset.contains(QStringLiteral("configuration"))) {
        QVariant configVar = preset[QStringLiteral("configuration")];
        if (configVar.canConvert<QDBusArgument>()) {
            QDBusArgument configArg = configVar.value<QDBusArgument>();
            QVariantMap configMap;
            configArg >> configMap;

            // Deserialize outputs array if it's QDBusArgument
            if (configMap.contains(QStringLiteral("outputs"))) {
                QVariant outputsVar = configMap[QStringLiteral("outputs")];
                if (outputsVar.canConvert<QDBusArgument>()) {
                    QVariantList outputsList = deserializeOutputsList(outputsVar.value<QDBusArgument>());
                    configMap[QStringLiteral("outputs")] = outputsList;
                    qCDebug(KDISPLAYPRESETS_APPLET) << "PresetModel: Deserialized" << outputsList.count() << "outputs from QDBusArgument";
                } else if (outputsVar.canConvert<QVariantList>()) {
                    configMap[QStringLiteral("outputs")] = outputsVar.toList();
                    qCDebug(KDISPLAYPRESETS_APPLET) << "PresetModel: Got" << outputsVar.toList().count() << "outputs as QVariantList";
                }
            } else {
                qCDebug(KDISPLAYPRESETS_APPLET) << "PresetModel: Configuration does not contain outputs";
            }

            preset[QStringLiteral("configuration")] = configMap;
        }
    }

    return preset;
}

QVariantList PresetModel::deserializeOutputsList(const QDBusArgument &outputsArg) const
{
    QVariantList outputsList;
    outputsArg >> outputsList;

    QVariantList deserializedOutputs;
    for (const QVariant &outputVar : outputsList) {
        if (outputVar.canConvert<QDBusArgument>()) {
            QVariantMap outputMap = deserializeOutputData(outputVar.value<QDBusArgument>());
            deserializedOutputs.append(outputMap);
        } else {
            deserializedOutputs.append(outputVar);
        }
    }

    return deserializedOutputs;
}

QVariantMap PresetModel::deserializeOutputData(const QDBusArgument &outputArg) const
{
    QVariantMap outputMap;
    outputArg >> outputMap;

    // Deserialize nested fields that might be QDBusArgument
    QStringList nestedFields = {"pos", "mode", "logicalSize"};
    for (const QString &field : nestedFields) {
        if (outputMap.contains(field)) {
            QVariant fieldVar = outputMap[field];
            if (fieldVar.canConvert<QDBusArgument>()) {
                QDBusArgument fieldArg = fieldVar.value<QDBusArgument>();
                QVariantMap fieldMap;
                fieldArg >> fieldMap;
                outputMap[field] = fieldMap;
            }
        }
    }

    return outputMap;
}

#include "kdisplaypresets_applet.moc"

#include "moc_kdisplaypresets_applet.cpp"
