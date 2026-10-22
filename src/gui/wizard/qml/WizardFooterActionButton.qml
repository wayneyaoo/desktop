/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import Style

// Flat footer button for an action that stays inside the wizard, such as opening a settings dialog.
Button {
    id: root

    flat: true
    font.pixelSize: Style.wizardButtonFontPixelSize
    font.weight: Font.Medium

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        enabled: root.enabled
        hoverEnabled: enabled
        cursorShape: Qt.PointingHandCursor
    }
}
