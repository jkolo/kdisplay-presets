/*
    SPDX-FileCopyrightText: 2025 Jerzy Kołosowski <jerzy@kolosowscy.pl>
    Based on ScreenView.qml by Roman Gilg

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15 as QQC2
import org.kde.kirigami 2.20 as Kirigami

Item {
    id: presetView

    property var outputs: []
    property bool presetAvailable: true
    property var kcm: null

    // Calculate own width based on aspect ratio when height is set
    width: {
        if (layoutWidth <= 0 || layoutHeight <= 0 || height <= 0) {
            return height * (16/9); // Fallback aspect ratio
        }
        return height * (layoutWidth / layoutHeight);
    }

    // Calculate bounds for real positioning
    readonly property var bounds: {
        if (!outputs || outputs.length === 0) {
            return { minX: 0, minY: 0, maxX: 1920, maxY: 1080 };
        }

        var minX = 999999, minY = 999999, maxX = -999999, maxY = -999999;

        for (var i = 0; i < outputs.length; i++) {
            var output = outputs[i];
            if (!output.enabled) continue;

            var pos = output.pos || { x: 0, y: 0 };
            var mode = output.mode || { width: 1920, height: 1080 };
            var scale = output.scale || 1;

            var w = mode.width / scale;
            var h = mode.height / scale;
            var x = pos.x || 0;
            var y = pos.y || 0;

            minX = Math.min(minX, x);
            minY = Math.min(minY, y);
            maxX = Math.max(maxX, x + w);
            maxY = Math.max(maxY, y + h);
        }

        if (minX === 999999) {
            return { minX: 0, minY: 0, maxX: 1920, maxY: 1080 };
        }

        return { minX: minX, minY: minY, maxX: maxX, maxY: maxY };
    }

    readonly property real layoutWidth: bounds.maxX - bounds.minX
    readonly property real layoutHeight: bounds.maxY - bounds.minY

    // Scale to fit container - similar to ScreenView's relativeFactor
    readonly property real scaleFactor: {
        if (layoutWidth <= 0 || layoutHeight <= 0 || width <= 0 || height <= 0) {
            return 0.1;
        }
        var margin = 15; // Increased margin for better spacing
        var scaleX = (width - margin * 2) / layoutWidth;
        var scaleY = (height - margin * 2) / layoutHeight;
        var finalScale = Math.min(scaleX, scaleY);

        return finalScale;
    }

    readonly property real xOffset: (width - layoutWidth * scaleFactor) / 2 // Center horizontally
    readonly property real yOffset: (height - layoutHeight * scaleFactor) / 2

    // Real positioned monitors using Repeater with PresetOutput delegate
    Repeater {
        model: outputs

        delegate: PresetOutput {
            outputData: modelData
            kcm: presetView.kcm
            scaleFactor: presetView.scaleFactor
            xOffset: presetView.xOffset
            yOffset: presetView.yOffset
            bounds: presetView.bounds
        }
    }

    // Placeholder when no outputs - similar to ScreenView pattern
    Kirigami.Icon {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.5, Kirigami.Units.iconSizes.medium)
        height: width
        source: "computer"
        opacity: 0.3
        visible: !outputs || outputs.length === 0
    }
}