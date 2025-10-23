/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

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

    // Auto-hide logic: Hide when ≤1 available preset and it's current
    Plasmoid.status: {
        if (!Plasmoid.presetModel) {
            return PlasmaCore.Types.PassiveStatus;
        }

        var availableCount = 0;
        var hasActiveCurrent = false;

        for (var i = 0; i < Plasmoid.presetModel.rowCount(); i++) {
            var idx = Plasmoid.presetModel.index(i, 0);
            var isAvailable = Plasmoid.presetModel.data(idx, 260); // IsAvailableRole = 260
            var isCurrent = Plasmoid.presetModel.data(idx, 259); // IsCurrentRole = 259

            if (isAvailable) {
                availableCount++;
                if (isCurrent) {
                    hasActiveCurrent = true;
                }
            }
        }

        // Hide if ≤1 available preset and it's current
        return (availableCount <= 1 && hasActiveCurrent)
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
        spacing: 0
        Layout.preferredWidth: Kirigami.Units.gridUnit * 15

        PresetList {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing * 2
            Layout.leftMargin: Kirigami.Units.smallSpacing
            model: Plasmoid.presetModel
            loadPresetFunc: Plasmoid.loadPreset
        }

        // compact the layout
        Item {
            Layout.fillHeight: true
        }
    }
}
