pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.plasmoid
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0
import "components" as Components

KCM.SimpleKCM {
    id: budgetPage

    signal configurationChanged()

    property bool cfg_forecastUiEnabled: Plasmoid.configuration.forecastUiEnabled
    property bool cfg_forecastNotificationsEnabled:
        Plasmoid.configuration.forecastNotificationsEnabled
    property int cfg_forecastLeadTimeHours:
        Plasmoid.configuration.forecastLeadTimeHours
    readonly property bool unsavedChanges: policyDrafts.dirty
    property string saveMessage: ""
    property bool saveError: false

    ProviderCatalog { id: providerCatalog }
    BudgetPolicyRepository {
        id: policyRepository
        // Plasma's attached qmltypes omit the stable applet instance id.
        // qmllint disable unresolved-type
        ownerId: "applet:" + String(Plasmoid["id"])
        // qmllint enable unresolved-type
        Component.onCompleted: {
            init();
        }
    }
    Components.BudgetPolicyDraftStore {
        id: policyDrafts
        repository: policyRepository
        copySuffix: i18n("copy")
        onStaged: budgetPage.configurationChanged()
    }

    function saveConfig() {
        if (!policyEditor.commitLimitText()) {
            saveError = true;
            saveMessage = i18n("Budget policies were not saved. Correct the highlighted amount and retry Apply.");
            Qt.callLater(function() { budgetPage.configurationChanged(); });
            return false;
        }
        if (!policyDrafts.dirty) return true;
        var ok = policyDrafts.apply();
        saveError = !ok;
        saveMessage = ok ? i18n("Budget policies saved.")
            : i18n("Budget policies were not saved. Correct the highlighted policy and retry Apply.");
        if (!ok) Qt.callLater(function() { budgetPage.configurationChanged(); });
        return ok;
    }

    function sourceLabel(sourceId) {
        var rows = providerCatalog.budgetProviders || [];
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].configKey === sourceId)
                return rows[i].displayName || rows[i].label || sourceId;
        }
        return sourceId;
    }

    function periodLabel(period) {
        var labels = {
            calendar_day: i18n("Daily"),
            iso_week: i18n("Weekly"),
            calendar_month: i18n("Monthly"),
            anchored_month: i18n("Anchored monthly"),
            provider_reset: i18n("Provider reset")
        };
        return labels[period] || period;
    }

    function scopeLabel(policy) {
        if (policy.scopeMode !== "scoped") return i18n("All spend");
        return policy.scopeLabel || i18n("%1 scope", policy.scopeKind || i18n("Unknown"));
    }

    Component.onDestruction: policyDrafts.discard()

    QQC2.Dialog {
        id: deleteDialog
        title: i18n("Delete budget policy?")
        modal: true
        standardButtons: QQC2.Dialog.Cancel | QQC2.Dialog.Ok
        onAccepted: policyDrafts.deleteSelected()

        QQC2.Label {
            width: Kirigami.Units.gridUnit * 22
            text: i18n("This removes the selected policy when you apply the settings. Historical provider observations remain local and unchanged.")
            wrapMode: Text.WordWrap
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.mediumSpacing

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 1
                    text: i18n("Budget Control")
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    text: i18n("Set local spending policies by source, period, and reported scope. No provider setting is changed.")
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
            }

            QQC2.CheckBox {
                text: i18n("Show pacing")
                checked: budgetPage.cfg_forecastUiEnabled
                activeFocusOnTab: true
                Accessible.name: i18n("Show budget pacing in the popup")
                onToggled: budgetPage.cfg_forecastUiEnabled = checked
            }
            QQC2.CheckBox {
                text: i18n("Notify")
                checked: budgetPage.cfg_forecastNotificationsEnabled
                activeFocusOnTab: true
                Accessible.name: i18n("Enable transition notifications for budget policies")
                onToggled: budgetPage.cfg_forecastNotificationsEnabled = checked
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: budgetPage.saveMessage.length > 0
                || policyDrafts.errorKey.length > 0
            type: budgetPage.saveError || policyDrafts.errorKey.length > 0
                ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive
            text: policyDrafts.errorKey === "snooze-unavailable"
                ? i18n("Snooze needs a stable period end. Verified provider reset data is not available in settings.")
                : policyDrafts.errorDetail.length > 0
                  ? policyDrafts.errorDetail : budgetPage.saveMessage
            Accessible.name: text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                Layout.fillWidth: true
                placeholderText: i18n("Search policies")
                text: policyDrafts.filterText
                activeFocusOnTab: true
                Accessible.name: i18n("Search budget policies")
                onTextEdited: policyDrafts.filterText = text
            }

            QQC2.Button {
                id: createButton
                text: i18n("Create")
                icon.name: "list-add"
                activeFocusOnTab: true
                Accessible.name: i18n("Create budget policy from a template")
                onClicked: createMenu.open()

                QQC2.Menu {
                    id: createMenu
                    y: createButton.height
                    QQC2.MenuItem {
                        text: i18n("Monthly aggregate")
                        onTriggered: policyDrafts.createPolicy(
                            providerCatalog.budgetProviders[0]?.configKey || "openai",
                            "calendar_month")
                    }
                    QQC2.MenuItem {
                        text: i18n("Weekly aggregate")
                        onTriggered: policyDrafts.createPolicy(
                            providerCatalog.budgetProviders[0]?.configKey || "openai",
                            "iso_week")
                    }
                    QQC2.MenuItem {
                        text: i18n("Daily aggregate")
                        onTriggered: policyDrafts.createPolicy(
                            providerCatalog.budgetProviders[0]?.configKey || "openai",
                            "calendar_day")
                    }
                }
            }

            QQC2.Button {
                text: i18n("Duplicate")
                icon.name: "edit-copy"
                enabled: !!policyDrafts.selectedPolicy.policyId
                activeFocusOnTab: true
                Accessible.name: i18n("Duplicate selected budget policy")
                onClicked: policyDrafts.duplicateSelected()
            }

            QQC2.Button {
                text: i18n("Delete")
                icon.name: "edit-delete"
                enabled: !!policyDrafts.selectedPolicy.policyId
                activeFocusOnTab: true
                Accessible.name: i18n("Delete selected budget policy")
                onClicked: deleteDialog.open()
            }
        }

        GridLayout {
            id: policyGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: budgetPage.width < Kirigami.Units.gridUnit * 42 ? 1 : 2
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.mediumSpacing

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: policyGrid.columns !== 1
                Layout.preferredWidth: Kirigami.Units.gridUnit * 15
                Layout.preferredHeight: policyGrid.columns === 1
                    ? Kirigami.Units.gridUnit * 11 : -1
                radius: Kirigami.Units.cornerRadius
                color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.7)
                border.color: Qt.alpha(Kirigami.Theme.textColor, 0.14)

                ListView {
                    id: policyList
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.smallSpacing
                    clip: true
                    model: policyDrafts.visiblePolicies
                    currentIndex: {
                        for (var i = 0; i < count; ++i) {
                            if (policyDrafts.policyKey(model[i])
                                    === policyDrafts.selectedKey) return i;
                        }
                        return -1;
                    }
                    activeFocusOnTab: true
                    Accessible.role: Accessible.List
                    Accessible.name: i18n("Budget policies")
                    Keys.onUpPressed: decrementCurrentIndex()
                    Keys.onDownPressed: incrementCurrentIndex()
                    onCurrentIndexChanged: {
                        if (currentIndex >= 0 && model[currentIndex])
                            policyDrafts.select(policyDrafts.policyKey(model[currentIndex]));
                    }

                    delegate: QQC2.ItemDelegate {
                        id: policyDelegate
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        highlighted: policyDrafts.policyKey(modelData)
                            === policyDrafts.selectedKey
                        activeFocusOnTab: true
                        Accessible.role: Accessible.ListItem
                        Accessible.name: budgetPage.sourceLabel(modelData.sourceId)
                            + ", " + budgetPage.periodLabel(modelData.periodType)
                            + ", " + budgetPage.scopeLabel(modelData)
                        onClicked: policyDrafts.select(policyDrafts.policyKey(modelData))

                        contentItem: RowLayout {
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                QQC2.Label {
                                    Layout.fillWidth: true
                                    text: budgetPage.sourceLabel(policyDelegate.modelData.sourceId)
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                QQC2.Label {
                                    Layout.fillWidth: true
                                    text: budgetPage.periodLabel(policyDelegate.modelData.periodType)
                                        + i18n(" · ") + budgetPage.scopeLabel(policyDelegate.modelData)
                                    color: Kirigami.Theme.disabledTextColor
                                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                                    elide: Text.ElideRight
                                }
                            }
                            Kirigami.Icon {
                                source: policyDelegate.modelData.enabled
                                    ? "dialog-ok-symbolic" : "media-playback-pause-symbolic"
                                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                Layout.preferredHeight: width
                            }
                        }
                    }

                    QQC2.Label {
                        anchors.centerIn: parent
                        width: parent.width - Kirigami.Units.largeSpacing * 2
                        visible: policyList.count === 0
                        text: policyDrafts.filterText.length > 0
                            ? i18n("No policy matches this search.")
                            : i18n("No budget policy yet. Create one from a template.")
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }

            Components.BudgetPolicyEditor {
                id: policyEditor
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: Kirigami.Units.gridUnit * 18
                store: policyDrafts
                providerCatalog: providerCatalog
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: policyDrafts.dirty

            Kirigami.Icon {
                source: "document-edit-symbolic"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: width
            }
            QQC2.Label {
                Layout.fillWidth: true
                text: i18n("Staged policy changes will be written atomically when you choose Apply. Cancel closes without saving them.")
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.neutralTextColor
            }
            QQC2.Button {
                text: i18n("Discard staged changes")
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: policyDrafts.discard()
            }
        }
    }
}
