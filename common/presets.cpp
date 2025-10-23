/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "presets.h"
#include "kdisplaypresets_common_debug.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

Presets::Presets(QObject *parent, const QString &customFilePath)
    : QAbstractListModel(parent)
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_customPresetsFilePath(customFilePath)
{
    loadPresetsFromDisk();

    const QString filePath = presetsFilePath();
    if (QFile::exists(filePath)) {
        m_fileWatcher->addPath(filePath);
    }
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &Presets::onPresetFileChanged);
}

int Presets::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_presets.count();
}

QVariant Presets::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_presets.count()) {
        return QVariant();
    }

    const DisplayPreset &preset = m_presets.at(index.row());

    switch (role) {
    case IdRole:
        return preset.id;
    case NameRole:
        return preset.name;
    case DescriptionRole:
        return preset.description;
    case CreatedRole:
        return preset.created;
    case LastUsedRole:
        return preset.lastUsed;
    case OutputCountRole:
        return preset.outputIds.count();
    case ConfigurationRole:
        return preset.configuration;
    case ShortcutRole:
        return preset.shortcut;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> Presets::roleNames() const
{
    return {
        {IdRole, "presetId"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {CreatedRole, "created"},
        {LastUsedRole, "lastUsed"},
        {OutputCountRole, "outputCount"},
        {ConfigurationRole, "configuration"},
        {ShortcutRole, "shortcut"},
    };
}

bool Presets::hasPresets() const
{
    return !m_presets.isEmpty();
}

KScreen::ConfigPtr Presets::screenConfiguration() const
{
    return m_screenConfiguration;
}

void Presets::setScreenConfiguration(KScreen::ConfigPtr config)
{
    m_screenConfiguration = config;
    Q_EMIT screenConfigurationChanged();
}

bool Presets::isPresetAvailable(const QString &presetId) const
{
    if (!m_screenConfiguration) {
        qCDebug(KDISPLAYPRESETS_COMMON) << "No screen configuration available for preset" << presetId;
        return false;
    }

    const auto preset = getPreset(presetId);
    if (preset.id.isEmpty()) {
        qCDebug(KDISPLAYPRESETS_COMMON) << "Preset not found:" << presetId;
        return false;
    }

    // Check if all required outputs are currently connected
    const auto presetOutputsList = preset.configuration[QStringLiteral("outputs")].toList();
    const auto currentOutputs = m_screenConfiguration->outputs();

    for (const auto &presetOutputVariant : presetOutputsList) {
        const auto presetOutputMap = presetOutputVariant.toMap();
        const QString presetOutputName = presetOutputMap[QStringLiteral("name")].toString();

        // Only check outputs that are supposed to be enabled in the preset
        if (!presetOutputMap[QStringLiteral("enabled")].toBool()) {
            continue;
        }

        bool found = false;
        for (const auto &currentOutput : currentOutputs) {
            if (currentOutput->name() == presetOutputName && currentOutput->isConnected()) {
                found = true;
                break;
            }
        }

        if (!found) {
            qCDebug(KDISPLAYPRESETS_COMMON) << "Output not found or not connected:" << presetOutputName << "for preset" << presetId;
            return false;
        }
    }

    qCDebug(KDISPLAYPRESETS_COMMON) << "Preset available:" << presetId;
    return true;
}

bool Presets::isPresetCurrent(const QString &presetId) const
{
    if (!m_screenConfiguration) {
        qCDebug(KDISPLAYPRESETS_COMMON) << "isPresetCurrent: No screen configuration available";
        return false;
    }

    const auto preset = getPreset(presetId);
    if (preset.id.isEmpty()) {
        qCDebug(KDISPLAYPRESETS_COMMON) << "isPresetCurrent: Preset not found:" << presetId;
        return false;
    }

    qCDebug(KDISPLAYPRESETS_COMMON) << "isPresetCurrent: Checking preset" << presetId << "against current config";

    const auto presetOutputsList = preset.configuration[QStringLiteral("outputs")].toList();
    const auto currentOutputs = m_screenConfiguration->outputs();

    // Check each currently connected output
    for (const auto &currentOutput : currentOutputs) {
        if (!currentOutput->isConnected()) {
            continue;
        }

        bool found = false;
        for (const auto &presetOutputVariant : presetOutputsList) {
            const auto presetOutputMap = presetOutputVariant.toMap();
            if (presetOutputMap[QStringLiteral("name")].toString() == currentOutput->name()) {
                found = true;

                // Check if enabled state matches
                if (currentOutput->isEnabled() != presetOutputMap[QStringLiteral("enabled")].toBool()) {
                    qCDebug(KDISPLAYPRESETS_COMMON) << "Enabled state mismatch for" << currentOutput->name()
                                                    << "current:" << currentOutput->isEnabled()
                                                    << "preset:" << presetOutputMap[QStringLiteral("enabled")].toBool();
                    return false;
                }

                // If output is enabled, check other properties
                if (currentOutput->isEnabled()) {
                    // Check priority
                    if (static_cast<uint32_t>(presetOutputMap[QStringLiteral("priority")].toInt()) != currentOutput->priority()) {
                        qCDebug(KDISPLAYPRESETS_COMMON) << "Priority mismatch for" << currentOutput->name()
                                                        << "current:" << currentOutput->priority()
                                                        << "preset:" << presetOutputMap[QStringLiteral("priority")].toInt();
                        return false;
                    }

                    // Check position
                    const auto presetPos = presetOutputMap[QStringLiteral("pos")].toMap();
                    const QPoint presetPosition(presetPos[QStringLiteral("x")].toInt(), presetPos[QStringLiteral("y")].toInt());
                    if (currentOutput->pos() != presetPosition) {
                        qCDebug(KDISPLAYPRESETS_COMMON) << "Position mismatch for" << currentOutput->name()
                                                        << "current:" << currentOutput->pos()
                                                        << "preset:" << presetPosition;
                        return false;
                    }

                    // Check mode (resolution and refresh rate)
                    const auto presetMode = presetOutputMap[QStringLiteral("mode")].toMap();
                    const QSize presetSize(presetMode[QStringLiteral("width")].toInt(), presetMode[QStringLiteral("height")].toInt());
                    const float presetRefresh = presetMode[QStringLiteral("refreshRate")].toFloat();

                    if (currentOutput->currentMode()) {
                        const QSize currentSize = currentOutput->currentMode()->size();
                        const float currentRefresh = currentOutput->currentMode()->refreshRate();

                        if (currentSize != presetSize || qAbs(currentRefresh - presetRefresh) > 0.1) {
                            qCDebug(KDISPLAYPRESETS_COMMON) << "Mode mismatch for" << currentOutput->name() << "current:" << currentSize << "@" << currentRefresh
                                                    << "preset:" << presetSize << "@" << presetRefresh;
                            return false;
                        }
                    } else {
                        qCDebug(KDISPLAYPRESETS_COMMON) << "No current mode for" << currentOutput->name();
                        return false;
                    }

                    // Check scale
                    if (qAbs(currentOutput->scale() - presetOutputMap[QStringLiteral("scale")].toReal()) > 0.01) {
                        qCDebug(KDISPLAYPRESETS_COMMON) << "Scale mismatch for" << currentOutput->name()
                                                        << "current:" << currentOutput->scale()
                                                        << "preset:" << presetOutputMap[QStringLiteral("scale")].toReal();
                        return false;
                    }

                    // Check rotation
                    if (static_cast<int>(currentOutput->rotation()) != presetOutputMap[QStringLiteral("rotation")].toInt()) {
                        qCDebug(KDISPLAYPRESETS_COMMON) << "Rotation mismatch for" << currentOutput->name()
                                                        << "current:" << static_cast<int>(currentOutput->rotation())
                                                        << "preset:" << presetOutputMap[QStringLiteral("rotation")].toInt();
                        return false;
                    }
                }
                break;
            }
        }

        // If current output is enabled but not found in preset, it's not current
        if (!found && currentOutput->isEnabled()) {
            qCDebug(KDISPLAYPRESETS_COMMON) << "Current enabled output" << currentOutput->name() << "not found in preset";
            return false;
        }
    }

    // Also check that preset doesn't expect outputs that aren't currently available
    for (const auto &presetOutputVariant : presetOutputsList) {
        const auto presetOutputMap = presetOutputVariant.toMap();
        if (presetOutputMap[QStringLiteral("enabled")].toBool()) {
            const QString presetOutputName = presetOutputMap[QStringLiteral("name")].toString();

            bool found = false;
            for (const auto &currentOutput : currentOutputs) {
                if (currentOutput->name() == presetOutputName && currentOutput->isConnected() && currentOutput->isEnabled()) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                qCDebug(KDISPLAYPRESETS_COMMON) << "Preset enabled output" << presetOutputName << "not found in current configuration";
                return false;
            }
        }
    }

    qCDebug(KDISPLAYPRESETS_COMMON) << "Preset" << presetId << "matches current configuration";
    return true;
}

void Presets::refreshPresetStatus()
{
    Q_EMIT presetsChanged();
    Q_EMIT dataChanged(index(0), index(rowCount() - 1));
}

DisplayPreset Presets::getPreset(const QString &presetId) const
{
    auto it = std::ranges::find_if(m_presets, [&presetId](const DisplayPreset &preset) {
        return preset.id == presetId;
    });

    if (it != m_presets.end()) {
        return *it;
    }

    return DisplayPreset{};
}

bool Presets::presetExists(const QString &name) const
{
    return std::any_of(m_presets.begin(), m_presets.end(), [&name](const DisplayPreset &preset) {
        return preset.name == name;
    });
}

void Presets::updateLastUsed(const QString &presetId)
{
    auto it = std::find_if(m_presets.begin(), m_presets.end(), [&presetId](const DisplayPreset &preset) {
        return preset.id == presetId;
    });

    if (it != m_presets.end()) {
        it->lastUsed = QDateTime::currentDateTime();
        savePresetsToDisk();
    }
}

void Presets::loadPresetsFromDisk()
{
    const QString filePath = presetsFilePath();
    QFile file(filePath);

    if (!file.exists()) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        const QString error = QStringLiteral("Could not open presets file for reading: %1").arg(filePath);
        qCWarning(KDISPLAYPRESETS_COMMON) << error;
        Q_EMIT loadingFailed(error);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        const QString error = QStringLiteral("Error parsing presets file: %1").arg(parseError.errorString());
        qCWarning(KDISPLAYPRESETS_COMMON) << error;
        Q_EMIT loadingFailed(error);
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonArray presetsArray = root[QStringLiteral("presets")].toArray();

    beginResetModel();
    m_presets.clear();

    for (const QJsonValue &value : presetsArray) {
        const QJsonObject presetObj = value.toObject();
        DisplayPreset preset;
        preset.id = presetObj[QStringLiteral("id")].toString();
        preset.name = presetObj[QStringLiteral("name")].toString();
        preset.description = presetObj[QStringLiteral("description")].toString();
        preset.created = QDateTime::fromString(presetObj[QStringLiteral("created")].toString(), Qt::ISODate);
        preset.lastUsed = QDateTime::fromString(presetObj[QStringLiteral("lastUsed")].toString(), Qt::ISODate);
        preset.configuration = presetObj[QStringLiteral("configuration")].toObject().toVariantMap();
        preset.shortcut = QKeySequence(presetObj[QStringLiteral("shortcut")].toString());

        // Extract output IDs
        const QJsonArray outputIds = presetObj[QStringLiteral("outputIds")].toArray();
        for (const QJsonValue &outputId : outputIds) {
            preset.outputIds.append(outputId.toString());
        }

        m_presets.append(preset);
    }

    endResetModel();
    Q_EMIT presetsChanged();
}

void Presets::savePresetsToDisk()
{
    const QString filePath = presetsFilePath();
    QFile file(filePath);

    // Create directory if it doesn't exist
    QDir dir = QFileInfo(filePath).absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    if (!file.open(QIODevice::WriteOnly)) {
        const QString error = QStringLiteral("Could not open presets file for writing: %1").arg(filePath);
        qCWarning(KDISPLAYPRESETS_COMMON) << error;
        Q_EMIT savingFailed(error);
        return;
    }

    QJsonArray presetsArray;
    for (const DisplayPreset &preset : m_presets) {
        QJsonObject presetObj;
        presetObj[QStringLiteral("id")] = preset.id;
        presetObj[QStringLiteral("name")] = preset.name;
        presetObj[QStringLiteral("description")] = preset.description;
        presetObj[QStringLiteral("created")] = preset.created.toString(Qt::ISODate);
        presetObj[QStringLiteral("lastUsed")] = preset.lastUsed.toString(Qt::ISODate);
        presetObj[QStringLiteral("configuration")] = QJsonObject::fromVariantMap(preset.configuration);
        presetObj[QStringLiteral("shortcut")] = preset.shortcut.toString();

        QJsonArray outputIds;
        for (const QString &outputId : preset.outputIds) {
            outputIds.append(outputId);
        }
        presetObj[QStringLiteral("outputIds")] = outputIds;

        presetsArray.append(presetObj);
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("presets")] = presetsArray;

    QJsonDocument doc(root);
    file.write(doc.toJson());

    // Ensure the file is watched after creation/modification
    if (!m_fileWatcher->files().contains(filePath)) {
        m_fileWatcher->addPath(filePath);
    }
}

QString Presets::presetsFilePath() const
{
    if (!m_customPresetsFilePath.isEmpty()) {
        return m_customPresetsFilePath;
    }

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return dataDir + QStringLiteral("/kdisplaypresets/presets.json");
}

void Presets::setCustomPresetsFilePath(const QString &filePath)
{
    m_customPresetsFilePath = filePath;
}

void Presets::reloadPresets()
{
    // Remove old file watcher if exists
    if (!m_fileWatcher->files().isEmpty()) {
        m_fileWatcher->removePaths(m_fileWatcher->files());
    }

    // Clear existing presets
    beginResetModel();
    m_presets.clear();
    endResetModel();

    // Load presets from the new path
    loadPresetsFromDisk();

    // Add new file to watcher
    const QString filePath = presetsFilePath();
    if (QFile::exists(filePath)) {
        m_fileWatcher->addPath(filePath);
    }

    Q_EMIT presetsChanged();
}

void Presets::onPresetFileChanged()
{
    const QString filePath = presetsFilePath();
    if (!QFile::exists(filePath)) {
        // File was deleted, clear presets
        beginResetModel();
        m_presets.clear();
        endResetModel();
        Q_EMIT presetsChanged();
        return;
    }

    // Re-add the file to watcher since Qt removes it after modification
    if (!m_fileWatcher->files().contains(filePath)) {
        m_fileWatcher->addPath(filePath);
    }

    // Reload presets from disk
    loadPresetsFromDisk();
    Q_EMIT presetsChanged();
}

void Presets::addPreset(const DisplayPreset &preset)
{
    beginInsertRows(QModelIndex(), m_presets.count(), m_presets.count());
    m_presets.append(preset);
    endInsertRows();
    Q_EMIT presetsChanged();
}

void Presets::updatePreset(const QString &presetId, const DisplayPreset &preset)
{
    auto it = std::find_if(m_presets.begin(), m_presets.end(), [&presetId](const DisplayPreset &p) {
        return p.id == presetId;
    });

    if (it != m_presets.end()) {
        const int row = std::distance(m_presets.begin(), it);
        m_presets[row] = preset;
        Q_EMIT dataChanged(index(row), index(row));
        Q_EMIT presetsChanged();
    }
}

void Presets::removePreset(const QString &presetId)
{
    auto it = std::find_if(m_presets.begin(), m_presets.end(), [&presetId](const DisplayPreset &preset) {
        return preset.id == presetId;
    });

    if (it != m_presets.end()) {
        const int row = std::distance(m_presets.begin(), it);
        beginRemoveRows(QModelIndex(), row, row);
        m_presets.erase(it);
        endRemoveRows();
        Q_EMIT presetsChanged();
    }
}

DisplayPreset *Presets::findPreset(const QString &presetId)
{
    auto it = std::find_if(m_presets.begin(), m_presets.end(), [&presetId](const DisplayPreset &preset) {
        return preset.id == presetId;
    });

    return it != m_presets.end() ? &(*it) : nullptr;
}

DisplayPreset *Presets::findPresetByName(const QString &name)
{
    auto it = std::find_if(m_presets.begin(), m_presets.end(), [&name](const DisplayPreset &preset) {
        return preset.name == name;
    });

    return it != m_presets.end() ? &(*it) : nullptr;
}

void Presets::saveToDisk()
{
    savePresetsToDisk();
}
