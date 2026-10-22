/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style
import "../../tray"

Dialog {
    id: root

    required property var controller
    property string errorText: ""

    objectName: "customRequestHeadersDialog"
    modal: true
    width: Style.wizardDialogMaximumWidth
    // Long headers scroll inside the text area, so the buttons stay inside the window.
    height: Math.min(contentLayout.implicitHeight + topPadding + bottomPadding,
                     parent ? parent.height - 2 * Style.wizardWindowMargin : Number.MAX_VALUE)
    padding: Style.wizardWindowMargin
    header: null
    footer: null

    onOpened: {
        headersTextArea.text = root.controller.customRequestHeadersText
        root.errorText = ""
        headersTextArea.forceActiveFocus()
    }

    function submit() {
        root.errorText = root.controller.submitCustomRequestHeaders(headersTextArea.text)
        if (root.errorText === "") {
            root.close()
        }
    }

    background: Rectangle {
        radius: Style.wizardDialogRadius
        color: Style.wizardWindowBackground
    }

    contentItem: ColumnLayout {
        id: contentLayout

        spacing: Style.wizardDialogSpacing
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("Custom headers")

        EnforcedPlainTextLabel {
            text: qsTr("Custom headers")
            color: Style.wizardPrimaryText
            font.pixelSize: Style.wizardHeaderTitleFontPixelSize
            font.bold: true
            Layout.fillWidth: true
        }

        EnforcedPlainTextLabel {
            text: qsTr("These headers are sent with every request to the server, for example to pass a reverse proxy that requires a token. Enter one header per line as \"Name: value\".")
            color: Style.wizardSecondaryText
            font.pixelSize: Style.wizardBodyFontPixelSize
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        Flickable {
            id: headersFlickable

            objectName: "customRequestHeadersFlickable"
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Style.wizardTextAreaHeight
            Layout.preferredHeight: Math.max(contentHeight, Style.wizardTextAreaHeight)

            ScrollBar.vertical: ScrollBar {}

            TextArea.flickable: WizardTextArea {
                id: headersTextArea

                objectName: "customRequestHeadersTextArea"
                // The flickable scrolls by the implicit height, so it has to grow with the text.
                implicitHeight: Math.max(Style.wizardTextAreaHeight, contentHeight + topPadding + bottomPadding)
                placeholderText: qsTr("X-Example-Token: value")
                Accessible.name: qsTr("Custom headers, one per line")
            }
        }

        EnforcedPlainTextLabel {
            objectName: "customRequestHeadersErrorLabel"
            visible: root.errorText !== ""
            text: root.errorText
            color: Style.wizardErrorText
            font.pixelSize: Style.wizardBodyFontPixelSize
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Style.wizardFooterSpacing

            Item {
                Layout.fillWidth: true
            }

            WizardButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }

            WizardButton {
                primary: true
                text: qsTr("Done")
                onClicked: root.submit()
            }
        }
    }
}
