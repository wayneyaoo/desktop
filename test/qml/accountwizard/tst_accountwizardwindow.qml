/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtTest
import com.nextcloud.desktopclient
import Style

import "../../../src/gui/wizard/qml"

Item {
    id: testRoot

    width: 200
    height: 100

    Component {
        id: controllerComponent

        AccountWizardController {}
    }

    Component {
        id: windowComponent

        AccountWizardWindow {}
    }

    TestCase {
        id: testCase

        property var wizard: null

        name: "AccountWizardWindow"
        when: windowShown

        function createWizard(step) {
            const controller = createTemporaryObject(controllerComponent, testRoot, { currentStep: step })
            verify(controller)
            wizard = windowComponent.createObject(testRoot, { controller: controller, visible: true })
            verify(wizard)
            return wizard
        }

        // The window must go before its temporary controller, which is destroyed right after cleanup().
        function cleanup() {
            if (wizard) {
                wizard.destroy()
                wizard = null
                wait(0)
            }
            Theme.wizardHideSelfHostButton = false
            Theme.wizardHideSignupButton = false
        }

        function findVisibleButton(item, text) {
            if (item.visible && item.text === text && item.hasOwnProperty("clicked")) {
                return item
            }
            for (let i = 0; i < item.children.length; ++i) {
                const found = findVisibleButton(item.children[i], text)
                if (found) {
                    return found
                }
            }
            return null
        }

        function buttonLabel(button) {
            for (let i = 0; i < button.contentItem.children.length; ++i) {
                const child = button.contentItem.children[i]
                if (child.objectName === "wizardButtonText") {
                    return child
                }
            }
            return null
        }

        function findByObjectName(item, objectName) {
            if (item.objectName === objectName) {
                return item
            }
            for (let i = 0; i < item.children.length; ++i) {
                const found = findByObjectName(item.children[i], objectName)
                if (found) {
                    return found
                }
            }
            return null
        }

        // Popups are not visual children of the window, so look them up among its declared objects.
        function findPopup(wizard, objectName) {
            for (let i = 0; i < wizard.contentData.length; ++i) {
                if (wizard.contentData[i].objectName === objectName) {
                    return wizard.contentData[i]
                }
            }
            return null
        }

        function openCustomHeadersDialog(wizard) {
            const button = findVisibleButton(wizard.contentItem, "Custom headers")
            verify(button, "no visible Custom headers button")
            button.clicked()
            const dialog = findPopup(wizard, "customRequestHeadersDialog")
            verify(dialog)
            tryVerify(() => dialog.opened)
            return dialog
        }

        function relabel(wizard, text, newText) {
            const button = findVisibleButton(wizard.contentItem, text)
            verify(button, "no visible button labelled " + text)
            button.text = newText
            return button
        }

        function test_selfHostVisibility_data() {
            return [
                { tag: "default", hidden: false, step: AccountWizardController.ServerStep, expected: true },
                { tag: "hidden by branding", hidden: true, step: AccountWizardController.ServerStep, expected: false },
                { tag: "other step", hidden: false, step: AccountWizardController.BasicAuthStep, expected: false },
                { tag: "hidden on other step", hidden: true, step: AccountWizardController.BasicAuthStep, expected: false }
            ]
        }

        function test_selfHostVisibility(data) {
            Theme.wizardHideSelfHostButton = data.hidden
            const wizard = createWizard(data.step)
            compare(findVisibleButton(wizard.contentItem, "Self-host") !== null, data.expected)
            if (data.step === AccountWizardController.ServerStep) {
                verify(findVisibleButton(wizard.contentItem, "Sign up"))
                verify(findVisibleButton(wizard.contentItem, "Log in"))
            }
        }

        function test_signupVisibility_data() {
            const cases = []
            for (const hideSelfHost of [false, true]) {
                for (const hideSignup of [false, true]) {
                    for (const step of [AccountWizardController.ServerStep, AccountWizardController.BasicAuthStep]) {
                        cases.push({
                            tag: "self-host hidden " + hideSelfHost + ", signup hidden " + hideSignup + ", step " + step,
                            hideSelfHost: hideSelfHost,
                            hideSignup: hideSignup,
                            step: step,
                            signupVisible: step === AccountWizardController.ServerStep && !hideSignup,
                            selfHostVisible: step === AccountWizardController.ServerStep && !hideSelfHost
                        })
                    }
                }
            }
            return cases
        }

        function test_signupVisibility(data) {
            Theme.wizardHideSelfHostButton = data.hideSelfHost
            Theme.wizardHideSignupButton = data.hideSignup
            const wizard = createWizard(data.step)
            compare(findVisibleButton(wizard.contentItem, "Sign up") !== null, data.signupVisible)
            compare(findVisibleButton(wizard.contentItem, "Self-host") !== null, data.selfHostVisible)
            if (data.step === AccountWizardController.ServerStep) {
                verify(findVisibleButton(wizard.contentItem, "Log in"))
            }
        }

        function test_widthIsConstantAcrossSteps() {
            const wizard = createWizard(AccountWizardController.ServerStep)
            const steps = [
                AccountWizardController.BrowserAuthStep,
                AccountWizardController.BasicAuthStep,
                AccountWizardController.SyncOptionsStep,
                AccountWizardController.ServerStep
            ]

            compare(wizard.minimumWidth, Style.accountWizardWidth)
            compare(wizard.width, Style.accountWizardWidth)
            for (const step of steps) {
                wizard.controller.currentStep = step
                compare(wizard.width, Style.accountWizardWidth, "width changed at step " + step)
                compare(wizard.minimumWidth, Style.accountWizardWidth)
            }
            compare(wizard.height, Style.compactDialogHeight)
        }

        function test_syncOptionsFooterFitsGermanLabels() {
            if (Qt.platform.os !== "osx") {
                skip("Style.accountWizardWidth was measured with the macOS system font")
            }

            const wizard = createWizard(AccountWizardController.SyncOptionsStep)
            const buttons = [
                relabel(wizard, "Cancel", "Abbrechen"),
                relabel(wizard, "Set up later", "Später einrichten"),
                relabel(wizard, "Advanced", "Fortgeschritten"),
                relabel(wizard, "Done", "Erledigt")
            ]

            for (const button of buttons) {
                const label = buttonLabel(button)
                verify(label)
                tryVerify(() => label.width > 0)
                verify(!label.truncated, button.text + " is truncated")
            }
        }

        function test_chooseButtonGrowsWithLabel() {
            const wizard = createWizard(AccountWizardController.SyncOptionsStep)
            const button = relabel(wizard, "Choose", "Auswählen")
            const label = buttonLabel(button)

            tryVerify(() => button.implicitWidth > Style.wizardInlineButtonMinimumWidth)
            tryCompare(button, "width", button.implicitWidth)
            verify(!label.truncated)
        }

        function test_chooseButtonKeepsMinimumWidthForShortLabel() {
            const wizard = createWizard(AccountWizardController.SyncOptionsStep)
            const button = relabel(wizard, "Choose", "OK")

            tryVerify(() => button.implicitWidth < Style.wizardInlineButtonMinimumWidth)
            tryCompare(button, "width", Style.wizardInlineButtonMinimumWidth)
        }

        function test_customHeadersDialogSubmitsEditedHeaders() {
            const wizard = createWizard(AccountWizardController.ServerStep)
            wizard.controller.customRequestHeadersText = "P-Access-Token: old"
            const dialog = openCustomHeadersDialog(wizard)
            const textArea = findByObjectName(dialog.contentItem, "customRequestHeadersTextArea")
            verify(textArea)
            compare(textArea.text, "P-Access-Token: old")

            textArea.text = "P-Access-Token: new"
            findVisibleButton(dialog.contentItem, "Done").clicked()

            compare(wizard.controller.submittedCustomRequestHeadersText, "P-Access-Token: new")
            tryVerify(() => !dialog.visible)
        }

        function test_customHeadersDialogStaysOpenForRejectedHeaders() {
            const wizard = createWizard(AccountWizardController.ServerStep)
            wizard.controller.customRequestHeadersError = "Line 1: This header is set by the client and cannot be changed."
            const dialog = openCustomHeadersDialog(wizard)
            const errorLabel = findByObjectName(dialog.contentItem, "customRequestHeadersErrorLabel")
            verify(errorLabel)
            verify(!errorLabel.visible)

            findByObjectName(dialog.contentItem, "customRequestHeadersTextArea").text = "Authorization: Basic abc"
            findVisibleButton(dialog.contentItem, "Done").clicked()

            verify(dialog.visible)
            verify(errorLabel.visible)
            compare(errorLabel.text, wizard.controller.customRequestHeadersError)

            findVisibleButton(dialog.contentItem, "Cancel").clicked()
            tryVerify(() => !dialog.visible)
            compare(wizard.controller.customRequestHeadersText, "")
        }

        function test_customHeadersDialogScrollsLongHeadersInsideWindow() {
            const wizard = createWizard(AccountWizardController.ServerStep)
            // A long token wraps into many lines.
            wizard.controller.customRequestHeadersText = "P-Access-Token: " + "x".repeat(2000)
            const dialog = openCustomHeadersDialog(wizard)
            const flickable = findByObjectName(dialog.contentItem, "customRequestHeadersFlickable")
            verify(flickable)

            // Layouts apply size changes when they are polished.
            tryVerify(() => !isPolishScheduled(dialog.contentItem))
            verify(flickable.contentHeight > flickable.height, "the headers do not scroll")
            const doneButton = findVisibleButton(dialog.contentItem, "Done")
            const bottomEdge = doneButton.mapToItem(null, 0, doneButton.height).y
            verify(bottomEdge <= wizard.height, "Done button ends at " + bottomEdge)
        }

        function test_overlongFooterLabelElidesInsideWindow() {
            const wizard = createWizard(AccountWizardController.SyncOptionsStep)
            const button = relabel(wizard, "Set up later", "Später einrichten ".repeat(20))
            const label = buttonLabel(button)

            tryVerify(() => label.truncated)
            const rightEdge = button.mapToItem(wizard.contentItem, button.width, 0).x
            verify(rightEdge <= wizard.width - Style.wizardWindowMargin, "button overflows to " + rightEdge)
        }
    }
}
