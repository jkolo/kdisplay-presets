/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.15
import QtQuick.Layouts
import QtQuick.Controls 2.15 as QQC2
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kcmutils as KCM
import org.kde.kquickcontrols

KCM.SimpleKCM {
    id: presetPage

    property bool canSavePreset: true

    // Force refresh when monitor connection state changes
    property bool _refreshTrigger: false

    title: i18nc("@title:window Display presets management", "Display Presets")

    // Connect to monitor connection changes to refresh preset availability
    Connections {
        target: kcm
        function onOutputConnect() {
            _refreshTrigger = !_refreshTrigger;
        }
    }

    actions: Kirigami.Action {
        text: i18nc("@action:button Save current display configuration", "Save Current…")
        icon.name: "list-add"
        enabled: canSavePreset
        onTriggered: savePresetDialog.open()
    }


    ListView {
        id: presetListView

        anchors.fill: parent
        clip: true
        model: kcm ? kcm.presetModel : null

        delegate: QQC2.ItemDelegate {
            width: ListView.view.width
            height: Math.max(Kirigami.Units.gridUnit * 5, buttonColumn.implicitHeight + Kirigami.Units.largeSpacing * 2)

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                ColumnLayout {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                    Layout.maximumWidth: Kirigami.Units.gridUnit * 12
                    spacing: 2

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: model.name || i18nc("@label Fallback name for preset without name", "Unnamed Preset")
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        font.bold: true
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: {
                            var description = model.description || "";
                            var count = i18nc("@info Number of displays in preset", "%1 display(s)", model.outputCount || 0);

                            if (description) {
                                return description + " • " + count;
                            } else {
                                return count;
                            }
                        }
                        textFormat: Text.PlainText
                        opacity: 0.6
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        elide: Text.ElideRight
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: model.lastUsed ? i18nc("@info", "Last used: %1", Qt.formatDate(model.lastUsed, Qt.DefaultLocaleShortDate)) : i18nc("@info", "Never used")
                        opacity: 0.6
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        elide: Text.ElideRight
                    }


                    QQC2.Label {
                        Layout.fillWidth: true
                        text: i18nc("@info", "Some monitors unavailable")
                        color: Kirigami.Theme.negativeTextColor
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        elide: Text.ElideRight
                        visible: {
                            _refreshTrigger; // Force binding update
                            return kcm && !kcm.isPresetAvailable(model.presetId);
                        }
                    }
                }

                // Monitor layout visualization - constrained to available space
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true  // Prevent overflow

                    PresetView {
                        anchors.fill: parent
                        // Ensure outputs is correctly passed as array
                        outputs: {
                            return model.configuration ? model.configuration.outputs : [];
                        }
                        presetAvailable: {
                            _refreshTrigger; // Force binding update
                            return !kcm || kcm.isPresetAvailable(model.presetId);
                        }
                        kcm: presetPage.kcm
                    }
                }

                ColumnLayout {
                    id: buttonColumn
                    spacing: Kirigami.Units.smallSpacing
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 9
                    Layout.maximumWidth: Kirigami.Units.gridUnit * 10

                    QQC2.Button {
                        id: applyButton
                        Layout.fillWidth: true
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                        text: {
                            _refreshTrigger; // Force binding update
                            return kcm && kcm.isPresetCurrent && kcm.isPresetCurrent(model.presetId)
                                  ? i18nc("@info:status Current preset is active", "Current")
                                  : i18nc("@action:button Apply display preset", "Apply");
                        }
                        icon.name: {
                            _refreshTrigger; // Force binding update
                            return kcm && kcm.isPresetCurrent && kcm.isPresetCurrent(model.presetId)
                                   ? "checkmark"
                                   : "dialog-ok-apply";
                        }
                        display: QQC2.AbstractButton.TextBesideIcon
                        enabled: {
                            _refreshTrigger; // Force binding update
                            return kcm && kcm.isPresetAvailable && kcm.isPresetAvailable(model.presetId) &&
                                   (!kcm.isPresetCurrent || !kcm.isPresetCurrent(model.presetId));
                        }
                        onClicked: {
                            if (kcm && typeof kcm.loadPreset === "function") {
                                kcm.loadPreset(model.presetId)
                            }
                        }

                        QQC2.ToolTip.text: {
                            _refreshTrigger; // Force binding update
                            return kcm && kcm.isPresetCurrent && kcm.isPresetCurrent(model.presetId)
                                   ? i18nc("@info:tooltip", "This display preset is currently active")
                                   : i18nc("@info:tooltip", "Apply this display preset configuration");
                        }
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: 1000
                    }

                    QQC2.Button {
                        text: i18nc("@action:button Edit preset properties", "Edit")
                        icon.name: "document-edit"
                        display: QQC2.AbstractButton.TextBesideIcon
                        Layout.fillWidth: true
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                        onClicked: {
                            editPresetDialog.presetId = model.presetId;
                            editPresetDialog.presetName = model.name || "";
                            editPresetDialog.presetDescription = model.description || "";
                            editPresetDialog.open();
                        }

                        QQC2.ToolTip.text: i18nc("@info:tooltip", "Edit the name and description of this preset")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: 1000
                    }

                    QQC2.Button {
                        text: i18nc("@action:button Delete preset", "Delete")
                        icon.name: "edit-delete"
                        display: QQC2.AbstractButton.TextBesideIcon
                        Layout.fillWidth: true
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                        onClicked: {
                            deleteConfirmDialog.presetId = model.presetId;
                            deleteConfirmDialog.presetName = model.name || i18nc("@label", "Unnamed Preset");
                            deleteConfirmDialog.open();
                        }

                        QQC2.ToolTip.text: i18nc("@info:tooltip", "Delete this display preset permanently")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: 1000
                    }

                    KeySequenceItem {
                        id: shortcutItem
                        Layout.alignment: Qt.AlignRight
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                        keySequence: model.shortcut || ""
                        patterns: ShortcutPattern.ModifierAndKey | ShortcutPattern.Modifier
                        multiKeyShortcutsAllowed: false
                        checkForConflictsAgainst: ShortcutType.Global
                        onCaptureFinished: {
                            if (kcm && typeof kcm.updatePresetShortcut === "function") {
                                kcm.updatePresetShortcut(model.presetId, keySequence)
                            }
                        }
                    }
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: presetListView.count === 0
            text: i18nc("@info", "No display presets saved")
            explanation: i18nc("@info", "Save your current display configuration to quickly restore it later")
            icon.name: "view-list-symbolic"

            helpfulAction: Kirigami.Action {
                text: i18nc("@action:button", "Save Current Configuration")
                icon.name: "document-save"
                enabled: canSavePreset
                onTriggered: savePresetDialog.open()
            }
        }
    }

    Kirigami.PromptDialog {
        id: deleteConfirmDialog

        property string presetId
        property string presetName

        title: i18nc("@title:window", "Delete Preset")
        subtitle: i18nc("%1 is a preset name",
                       "Do you want to delete preset '%1'?", presetName)

        dialogType: Kirigami.PromptDialog.Warning

        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18nc("@action:button", "Delete Preset")
                icon.name: "edit-delete"
                onTriggered: {
                    if (kcm && typeof kcm.deletePreset === "function") {
                        kcm.deletePreset(deleteConfirmDialog.presetId)
                    }
                    deleteConfirmDialog.close()
                }
            }
        ]
    }

    Kirigami.Dialog {
        id: savePresetDialog

        title: i18nc("@title:window", "Save Display Preset")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel

        preferredWidth: Kirigami.Units.gridUnit * 20

        Kirigami.FormLayout {
            QQC2.TextField {
                id: presetNameField
                Kirigami.FormData.label: i18nc("@label:textbox", "Name:")
                placeholderText: i18nc("@info:placeholder", "Enter preset name")
                Layout.fillWidth: true
            }

            QQC2.TextArea {
                id: presetDescriptionField
                Kirigami.FormData.label: i18nc("@label:textbox", "Description:")
                placeholderText: i18nc("@info:placeholder", "Optional description")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 3
            }
        }

        onAccepted: {
            if (presetNameField.text.trim() !== "") {
                if (kcm && typeof kcm.savePreset === "function") {
                    kcm.savePreset(presetNameField.text.trim(), presetDescriptionField.text.trim())
                }
                presetNameField.clear()
                presetDescriptionField.clear()
            }
        }
    }

    Kirigami.Dialog {
        id: editPresetDialog

        property string presetId
        property string presetName
        property string presetDescription

        title: i18nc("@title:window", "Edit Preset")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel

        preferredWidth: Kirigami.Units.gridUnit * 20

        Kirigami.FormLayout {
            QQC2.TextField {
                id: editNameField
                Kirigami.FormData.label: i18nc("@label:textbox", "Name:")
                placeholderText: i18nc("@info:placeholder", "Enter preset name")
                Layout.fillWidth: true
                text: editPresetDialog.presetName
            }

            QQC2.TextArea {
                id: editDescriptionField
                Kirigami.FormData.label: i18nc("@label:textbox", "Description:")
                placeholderText: i18nc("@info:placeholder", "Optional description")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 3
                text: editPresetDialog.presetDescription
            }
        }

        onAccepted: {
            if (editNameField.text.trim() !== "") {
                // Update name if changed
                if (editNameField.text.trim() !== editPresetDialog.presetName &&
                    kcm && typeof kcm.renamePreset === "function") {
                    kcm.renamePreset(editPresetDialog.presetId, editNameField.text.trim())
                }

                // Update description if changed
                if (editDescriptionField.text.trim() !== editPresetDialog.presetDescription &&
                    kcm && typeof kcm.updatePresetDescription === "function") {
                    kcm.updatePresetDescription(editPresetDialog.presetId, editDescriptionField.text.trim())
                }
            }
        }
    }
}
