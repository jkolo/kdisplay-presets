/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

ColumnLayout {
    id: presetList

    property alias model: listView.model
    property var loadPresetFunc

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

                // Preview icon button
                PlasmaComponents.ToolButton {
                    id: previewButton
                    icon.name: "monitor"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium

                    // Preview popup - moved outside the RowLayout to avoid positioning issues
                    QQC2.ToolTip {
                        id: previewPopup
                        parent: QQC2.Overlay.overlay  // Use overlay for unrestricted positioning
                        visible: previewButton.hovered

                       // Dynamic sizing with reasonable bounds
                        readonly property real maxPopupWidth: Kirigami.Units.gridUnit * 30
                        readonly property real maxPopupHeight: Kirigami.Units.gridUnit * 20
                        readonly property real minPopupWidth: Kirigami.Units.gridUnit * 12
                        readonly property real minPopupHeight: Kirigami.Units.gridUnit * 8

                        // Calculate size based on layout aspect ratio
                        readonly property real contentAspectRatio: {
                            if (!presetViewContent.outputs || presetViewContent.outputs.length === 0) {
                                return 16/9; // Default aspect ratio
                            }

                            var bounds = presetViewContent.bounds;
                            var layoutWidth = bounds.maxX - bounds.minX;
                            var layoutHeight = bounds.maxY - bounds.minY;

                            if (layoutWidth <= 0 || layoutHeight <= 0) {
                                return 16/9;
                            }

                            return layoutWidth / layoutHeight;
                        }

                        onAboutToShow: {
                            var pos = previewButton.mapToItem(QQC2.Overlay.overlay, 0, 0);
                            x = pos.x - width;
                            y = pos.y;
                        }

                        // Fixed width for consistent positioning, dynamic height based on aspect ratio
                        width: Kirigami.Units.gridUnit * 30

                        height: {
                            var idealHeight = width / contentAspectRatio;
                            return Math.max(minPopupHeight, Math.min(maxPopupHeight, idealHeight));
                        }

                        contentItem: PresetView {
                            id: presetViewContent
                            outputs: model.configuration ? model.configuration.outputs : []
                            presetAvailable: presetItem.available
                            kcm: null // We don't have KCM in plasmoid, but PresetView should handle null
                            clip: false  // Disable clipping on content too
                        }
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