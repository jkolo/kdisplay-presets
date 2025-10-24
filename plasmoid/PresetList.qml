/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore

ColumnLayout {
    id: presetList

    property alias model: listView.model
    property var loadPresetFunc
    property real plasmoidHeight: 0
    property var plasmoidRoot: null

    spacing: Kirigami.Units.smallSpacing

    ListView {
        id: listView
        Layout.fillWidth: true
        Layout.preferredHeight: Math.max(contentHeight, Kirigami.Units.gridUnit * 3)

        clip: true
        boundsBehavior: Flickable.StopAtBounds


        delegate: PlasmaComponents.ItemDelegate {
            id: presetItem
            width: ListView.view.width

            property bool available: model.isAvailable || false
            property bool isCurrent: model.isCurrent || false

            // Always show presets, but disable unavailable ones
            enabled: available
            opacity: available ? 1.0 : 0.6

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                // Preview icon button
                PlasmaComponents.ToolButton {
                    id: previewButton
                    icon.name: "monitor"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium

                    PlasmaCore.PopupPlasmaWindow {
                        id: previewPopup
                        visualParent: presetList.plasmoidRoot || previewButton
                        visible: previewButton.hovered
                        popupDirection: Qt.LeftEdge

                        width: mainItem.implicitWidth
                        height: mainItem.implicitHeight

                        mainItem: Item {
                            id: mainItem
                            implicitHeight: presetList.plasmoidHeight > 0 ? presetList.plasmoidHeight : Kirigami.Units.gridUnit * 15
                            implicitWidth: presetView.width + Kirigami.Units.largeSpacing * 2

                            PresetView {
                                id: presetView
                                height: parent.implicitHeight - Kirigami.Units.largeSpacing * 2
                                x: Kirigami.Units.largeSpacing
                                y: Kirigami.Units.largeSpacing
                                outputs: model.configuration ? model.configuration.outputs : []
                                presetAvailable: presetItem.available
                                kcm: null
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: model.name || i18nc("@label", "Unnamed Preset")
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: model.description || i18nc("@info", "%1 display(s)", model.outputCount || 0)
                        opacity: 0.6
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        elide: Text.ElideRight
                    }

                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: model.shortcut || ""
                        opacity: 0.6
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        elide: Text.ElideRight
                        visible: model.shortcut && model.shortcut.length > 0
                    }
                }

                PlasmaComponents.Button {
                    id: currentButton
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                    text: i18nc("@info:status Current preset is active", "Current")
                    icon.name: "checkmark"
                    enabled: false
                    visible: presetItem.isCurrent

                    PlasmaComponents.ToolTip {
                        text: i18nc("@info:tooltip", "This display preset is currently active")
                    }
                }

                PlasmaComponents.Button {
                    id: applyButton
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                    text: i18nc("@action:button Apply display preset", "Apply")
                    icon.name: "dialog-ok-apply"
                    visible: !presetItem.isCurrent
                    enabled: presetItem.available
                    onClicked: {
                        if (loadPresetFunc) {
                            loadPresetFunc(model.presetId)
                        }
                    }

                    PlasmaComponents.ToolTip {
                        text: presetItem.available
                            ? i18nc("@info:tooltip", "Apply this display preset")
                            : i18nc("@info:tooltip", "Some monitors required by this preset are not connected")
                    }
                }
            }
        }

        PlasmaComponents.Label {
            anchors.centerIn: parent
            visible: listView.count === 0
            text: i18nc("@info", "No presets available for current displays")
            opacity: 0.6
        }
    }
}