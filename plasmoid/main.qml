/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

// NOTE: This pragma is file-scoped. If you add it to PresetList.qml (or any
// file whose delegates read model.* context properties), you MUST first convert
// each `model.xyz` access to an explicit `required property` declaration, or the
// delegates will silently stop receiving model data.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.config as KConfig
import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid

PlasmoidItem {
    id: root

    readonly property string kcmName: "kcm_displaypresets"
    readonly property bool kcmAllowed: KConfig.KAuthorized.authorizeControlModule("kcm_displaypresets")

    // Auto-hide logic: hide when ≤1 available preset and it is the active one.
    // Bound to the model's NOTIFYable availableCount/hasActiveCurrent properties
    // so it re-evaluates on every refresh. A rowCount()/data() loop here would
    // create no binding dependency and freeze at the startup state.
    Plasmoid.status: {
        if (!Plasmoid.presetModel) {
            return PlasmaCore.Types.PassiveStatus;
        }
        return (Plasmoid.presetModel.availableCount <= 1 && Plasmoid.presetModel.hasActiveCurrent)
            ? PlasmaCore.Types.PassiveStatus
            : PlasmaCore.Types.ActiveStatus;
    }

    toolTipSubText: i18n("Switch display presets")

    PlasmaCore.Action {
        id: configureAction
        text: i18n("Configure Display Presets…")
        icon.name: "preferences-desktop-display"
        visible: root.kcmAllowed
        onTriggered: KCMUtils.KCMLauncher.openSystemSettings(root.kcmName)
    }

    Component.onCompleted: {
        Plasmoid.setInternalAction("configure", configureAction);
    }

    fullRepresentation: ColumnLayout {
        id: fullRep
        spacing: 0
        Layout.preferredWidth: Kirigami.Units.gridUnit * 15

        PresetList {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing * 2
            Layout.leftMargin: Kirigami.Units.smallSpacing
            model: Plasmoid.presetModel
            loadPresetFunc: Plasmoid.loadPreset
            plasmoidHeight: fullRep.height
            plasmoidRoot: fullRep
        }

        // compact the layout
        Item {
            Layout.fillHeight: true
        }
    }
}
