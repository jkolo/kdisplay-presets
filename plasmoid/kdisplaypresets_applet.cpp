/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kdisplaypresets_applet.h"
#include "kdisplaypresets_applet_debug.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>

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
    // Reconnect automatically if the daemon (re)appears on the bus: handles the
    // daemon being started after the applet (D-Bus/systemd activation) or
    // restarting after a crash, instead of being stuck disconnected forever.
    auto *serviceWatcher = new QDBusServiceWatcher(QStringLiteral("org.kde.kdisplaypresets"),
                                                   QDBusConnection::sessionBus(),
                                                   QDBusServiceWatcher::WatchForRegistration,
                                                   this);
    connect(serviceWatcher, &QDBusServiceWatcher::serviceRegistered, this, [this]() {
        qCDebug(KDISPLAYPRESETS_APPLET) << "kdisplaypresets daemon registered; (re)connecting";
        connectToDaemon();
        refreshPresets();
    });

    connectToDaemon();
    // The first fetch is kicked off from KDisplayPresetsApplet::init().
}

void PresetModel::connectToDaemon()
{
    if (m_signalConnected) {
        return;
    }
    m_signalConnected = QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.kdisplaypresets"),
                                                              QStringLiteral("/"),
                                                              QStringLiteral("org.kde.kdisplaypresets"),
                                                              QStringLiteral("presetsChanged"),
                                                              this,
                                                              SLOT(onPresetAvailabilityChanged()));
}

int PresetModel::availableCount() const
{
    int count = 0;
    for (const QVariant &presetVariant : m_presets) {
        if (presetVariant.toMap().value(QStringLiteral("isAvailable")).toBool()) {
            ++count;
        }
    }
    return count;
}

bool PresetModel::hasActiveCurrent() const
{
    for (const QVariant &presetVariant : m_presets) {
        const QVariantMap preset = presetVariant.toMap();
        if (preset.value(QStringLiteral("isAvailable")).toBool() && preset.value(QStringLiteral("isCurrent")).toBool()) {
            return true;
        }
    }
    return false;
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
    if (!m_presetsInterface) {
        return;
    }

    // Asynchronous: a blocking call here would run on the plasmashell GUI
    // thread and could freeze the whole panel (up to the 25 s D-Bus timeout)
    // if the daemon is slow or hung. Matches the asyncCall used for applyPreset.
    auto *watcher = new QDBusPendingCallWatcher(m_presetsInterface->asyncCall(QStringLiteral("getPresets")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        call->deleteLater();

        const QDBusPendingReply<QVariantList> reply = *call;
        if (reply.isError()) {
            qCWarning(KDISPLAYPRESETS_APPLET) << "PresetModel::refreshPresets() - D-Bus call failed:" << reply.error().message();
            return;
        }

        QVariantList presets;
        const QVariantList rawPresets = reply.value();
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

        // Drive the reactive auto-hide binding (availableCount/hasActiveCurrent).
        Q_EMIT countsChanged();
    });
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

    // QtDBus demarshals an a{sv} only one level deep, so any nested map (pos,
    // mode, logicalSize, ...) arrives as a QVariant wrapping a QDBusArgument.
    // Unwrap every such value generically rather than hardcoding field names —
    // that way a new nested field added to the preset never silently arrives
    // as undefined in QML.
    for (auto it = outputMap.begin(); it != outputMap.end(); ++it) {
        if (it.value().canConvert<QDBusArgument>()) {
            QDBusArgument nested = it.value().value<QDBusArgument>();
            QVariantMap nestedMap;
            nested >> nestedMap;
            it.value() = nestedMap;
        }
    }

    return outputMap;
}

#include "kdisplaypresets_applet.moc"

#include "moc_kdisplaypresets_applet.cpp"
