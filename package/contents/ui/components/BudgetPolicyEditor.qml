pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

QQC2.ScrollView {
    id: editor

    required property var store
    required property var providerCatalog
    readonly property var policy: store && store.selectedPolicy
        ? store.selectedPolicy : ({})
    property int previewPercent: 65
    property string amountError: ""
    readonly property bool hasInputError: amountError.length > 0

    Accessible.role: Accessible.Pane
    Accessible.name: i18n("Budget policy editor")

    onPolicyChanged: {
        amountError = "";
        limitField.textEdited = false;
    }

    function providerFor(sourceId) {
        var rows = providerCatalog && providerCatalog.budgetProviders || [];
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].configKey === sourceId) return rows[i];
        }
        return {};
    }

    function providerLabel() {
        var entry = providerFor(policy ? policy.sourceId : "");
        return entry.displayName || entry.label || entry.name
            || policy.sourceId || i18n("Budget policy");
    }

    function supportedScopeValues() {
        var entry = providerFor(policy ? policy.sourceId : "");
        return entry && entry.supportedBudgetScopes
            ? entry.supportedBudgetScopes : ["aggregate"];
    }

    function supportedCycleValues() {
        var entry = providerFor(policy ? policy.sourceId : "");
        return entry && entry.supportedBillingCycles
            ? entry.supportedBillingCycles
            : ["calendar_day", "iso_week", "calendar_month", "anchored_month"];
    }

    function providerIndex(sourceId) {
        var rows = providerCatalog && providerCatalog.budgetProviders || [];
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].configKey === sourceId) return i;
        }
        return -1;
    }

    function optionIndex(rows, value) {
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].value === value) return i;
        }
        return -1;
    }

    function scopeOptions() {
        var labels = {
            project: i18n("Project"),
            workspace: i18n("Workspace"),
            model: i18n("Model"),
            service_tier: i18n("Service tier"),
            line_item: i18n("Line item")
        };
        var result = [];
        var supported = supportedScopeValues();
        for (var i = 0; i < supported.length; ++i) {
            var value = supported[i];
            if (value !== "aggregate")
                result.push({ value: value, label: labels[value] || value });
        }
        return result;
    }

    function periodOptions() {
        var labels = {
            calendar_day: i18n("Calendar day"),
            iso_week: i18n("ISO week · Monday start"),
            calendar_month: i18n("Calendar month"),
            anchored_month: i18n("Anchored month"),
            provider_reset: i18n("Verified provider reset")
        };
        var result = [];
        var supported = supportedCycleValues();
        for (var i = 0; i < supported.length; ++i) {
            var value = supported[i];
            result.push({ value: value, label: labels[value] || value });
        }
        return result;
    }

    function riskLabel() {
        if (previewPercent >= 100) return i18n("Exceeded");
        if (previewPercent >= Number(policy.criticalPercent || 90))
            return i18n("Critical");
        if (previewPercent >= Number(policy.warningPercent || 80))
            return i18n("Warning");
        return i18n("Safe");
    }

    function riskColor() {
        if (previewPercent >= 100
                || previewPercent >= Number(policy.criticalPercent || 90))
            return Kirigami.Theme.negativeTextColor;
        if (previewPercent >= Number(policy.warningPercent || 80))
            return Kirigami.Theme.neutralTextColor;
        return Kirigami.Theme.positiveTextColor;
    }

    function validation() {
        if (!store || !store.repository || !policy.policyId
                || typeof store.repository.validatePolicy !== "function")
            return { ok: false, error: "" };
        return store.repository.validatePolicy(policy);
    }

    function amountErrorText(reason) {
        switch (reason) {
        case "unknown-currency":
            return i18n("The currency has no known ISO 4217 minor-unit precision.");
        case "non-positive-amount":
            return i18n("The budget limit must be greater than zero.");
        default:
            return i18n("Enter a valid budget amount.");
        }
    }

    function commitLimitText() {
        if (!limitField.textEdited) return !hasInputError;
        var parsed = store && store.repository
            ? store.repository.parseMajorAmount(
                  limitField.text, policy.currency || "USD")
            : ({ ok: false, error: "invalid-amount" });
        amountError = parsed.ok ? "" : amountErrorText(parsed.error);
        if (!parsed.ok) return false;
        store.setField("limitMinor", Number(parsed.minor));
        limitField.textEdited = false;
        return true;
    }

    ColumnLayout {
        width: editor.availableWidth
        spacing: Kirigami.Units.largeSpacing
        visible: !!editor.policy.policyId

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 2
                    text: editor.providerLabel()
                    elide: Text.ElideRight
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    text: i18n("Changes stay staged until Apply.")
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
            }

            QQC2.Switch {
                text: i18n("Enabled")
                checked: !!editor.policy.enabled
                activeFocusOnTab: true
                Accessible.name: i18n("Enable this budget policy")
                onToggled: editor.store.setField("enabled", checked)
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: editor.hasInputError || (!editor.validation().ok
                && editor.validation().error !== "")
            type: Kirigami.MessageType.Error
            text: editor.amountError || editor.validation().error || ""
            Accessible.name: text
        }

        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: editor.supportedScopeValues()

                Rectangle {
                    id: capabilityBadge
                    required property string modelData
                    width: badgeLabel.implicitWidth + Kirigami.Units.mediumSpacing * 2
                    height: badgeLabel.implicitHeight + Kirigami.Units.smallSpacing
                    radius: height / 2
                    color: Qt.alpha(Kirigami.Theme.highlightColor, 0.12)
                    border.color: Qt.alpha(Kirigami.Theme.highlightColor, 0.32)

                    QQC2.Label {
                        id: badgeLabel
                        anchors.centerIn: parent
                        text: capabilityBadge.modelData.replace(/_/g, " ")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                    }
                }
            }
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            QQC2.ComboBox {
                id: sourceCombo
                Kirigami.FormData.label: i18n("Source:")
                model: editor.providerCatalog
                    ? editor.providerCatalog.budgetProviders || [] : []
                textRole: "displayName"
                currentIndex: editor.providerIndex(editor.policy.sourceId)
                activeFocusOnTab: true
                Accessible.name: i18n("Budget source")
                onActivated: function(index) {
                    var selected = editor.providerCatalog
                        && editor.providerCatalog.budgetProviders
                        ? editor.providerCatalog.budgetProviders[index] || {} : {};
                    editor.store.setField("sourceId", selected.configKey || "");
                    editor.store.setField("scopeMode", "aggregate");
                }
            }

            QQC2.ComboBox {
                id: scopeModeCombo
                Kirigami.FormData.label: i18n("Scope:")
                model: [
                    { value: "aggregate", label: i18n("All reported spend") },
                    { value: "scoped", label: i18n("One reported dimension") }
                ]
                textRole: "label"
                valueRole: "value"
                currentIndex: editor.policy.scopeMode === "scoped" ? 1 : 0
                activeFocusOnTab: true
                Accessible.name: i18n("Budget scope mode")
                onActivated: {
                    editor.store.setField("scopeMode", currentValue);
                    var options = editor.scopeOptions();
                    if (currentValue === "scoped" && options.length > 0
                            && !editor.policy.scopeKind)
                        editor.store.setField("scopeKind", options[0].value);
                }
            }

            QQC2.ComboBox {
                id: scopeKindCombo
                Kirigami.FormData.label: i18n("Dimension:")
                visible: editor.policy.scopeMode === "scoped"
                enabled: count > 0
                model: editor.scopeOptions()
                textRole: "label"
                valueRole: "value"
                currentIndex: editor.optionIndex(editor.scopeOptions(),
                                                  editor.policy.scopeKind)
                activeFocusOnTab: true
                Accessible.name: i18n("Reported scope dimension")
                onActivated: editor.store.setField("scopeKind", currentValue)
            }

            QQC2.TextField {
                Kirigami.FormData.label: i18n("Local identity:")
                visible: editor.policy.scopeMode === "scoped"
                text: editor.policy.scopeIdentity || ""
                placeholderText: i18n("Exact provider-reported identity")
                activeFocusOnTab: true
                Accessible.name: i18n("Local scope identity")
                onTextEdited: editor.store.setField("scopeIdentity", text.trim())
            }

            QQC2.TextField {
                Kirigami.FormData.label: i18n("Display label:")
                visible: editor.policy.scopeMode === "scoped"
                text: editor.policy.scopeLabel || ""
                placeholderText: i18n("Safe local label for renamed or deleted scopes")
                activeFocusOnTab: true
                Accessible.name: i18n("Scope display label")
                onTextEdited: editor.store.setField("scopeLabel", text.trim())
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: i18n("Value class:")
                model: [
                    { value: "actual", label: i18n("Actual provider cost") },
                    { value: "estimated", label: i18n("Local estimate") }
                ]
                textRole: "label"
                valueRole: "value"
                currentIndex: editor.policy.valueClass === "estimated" ? 1 : 0
                activeFocusOnTab: true
                Accessible.name: i18n("Cost value class")
                onActivated: editor.store.setField("valueClass", currentValue)
            }

            QQC2.ComboBox {
                id: currencyCombo
                Kirigami.FormData.label: i18n("Currency:")
                editable: true
                model: ["USD", "EUR", "SEK", "GBP", "JPY", "KWD"]
                currentIndex: model.indexOf(editor.policy.currency || "USD")
                editText: editor.policy.currency || "USD"
                activeFocusOnTab: true
                Accessible.name: i18n("ISO 4217 budget currency")
                onActivated: editor.store.setField("currency", currentText.toUpperCase())
                onAccepted: editor.store.setField("currency", editText.trim().toUpperCase())
            }

            QQC2.TextField {
                id: limitField
                objectName: "budgetLimitField"
                property bool textEdited: false
                Kirigami.FormData.label: i18n("Limit:")
                activeFocusOnTab: true
                Accessible.name: i18n("Budget limit in %1", editor.policy.currency || "USD")
                onTextEdited: {
                    textEdited = true;
                    var parsed = editor.store && editor.store.repository
                        ? editor.store.repository.parseMajorAmount(
                              text, editor.policy.currency || "USD")
                        : ({ ok: false, error: "invalid-amount" });
                    editor.amountError = parsed.ok ? ""
                        : editor.amountErrorText(parsed.error);
                }
                onEditingFinished: editor.commitLimitText()

                Binding {
                    target: limitField
                    property: "text"
                    when: !limitField.activeFocus && !limitField.textEdited
                    value: editor.store && editor.store.repository
                        ? editor.store.repository.formatMinorAmount(
                              Number(editor.policy.limitMinor || 0),
                              editor.policy.currency || "USD")
                        : String(editor.policy.limitMinor || "")
                    restoreMode: Binding.RestoreBindingOrValue
                }
            }

            QQC2.ComboBox {
                id: periodCombo
                Kirigami.FormData.label: i18n("Period:")
                model: editor.periodOptions()
                textRole: "label"
                valueRole: "value"
                currentIndex: editor.optionIndex(editor.periodOptions(),
                                                  editor.policy.periodType)
                activeFocusOnTab: true
                Accessible.name: i18n("Budget billing period")
                onActivated: editor.store.setField("periodType", currentValue)
            }

            QQC2.SpinBox {
                Kirigami.FormData.label: i18n("Anchor day:")
                visible: editor.policy.periodType === "anchored_month"
                from: 1
                to: 28
                value: Number(editor.policy.anchorDay || 1)
                activeFocusOnTab: true
                Accessible.name: i18n("Monthly billing anchor day")
                onValueModified: editor.store.setField("anchorDay", value)
            }

            QQC2.TextField {
                Kirigami.FormData.label: i18n("Time zone:")
                text: editor.policy.timeZoneId || "UTC"
                placeholderText: i18n("IANA time zone, for example Europe/Stockholm")
                activeFocusOnTab: true
                Accessible.name: i18n("Policy time zone")
                onTextEdited: editor.store.setField("timeZoneId", text.trim())
            }

            QQC2.SpinBox {
                Kirigami.FormData.label: i18n("Warning:")
                from: 1
                to: 100
                value: Number(editor.policy.warningPercent || 80)
                textFromValue: function(value, locale) { return i18n("%1%", value); }
                activeFocusOnTab: true
                Accessible.name: i18n("Warning threshold percentage")
                onValueModified: editor.store.setField("warningPercent", value)
            }

            QQC2.SpinBox {
                Kirigami.FormData.label: i18n("Critical:")
                from: 1
                to: 100
                value: Number(editor.policy.criticalPercent || 90)
                textFromValue: function(value, locale) { return i18n("%1%", value); }
                activeFocusOnTab: true
                Accessible.name: i18n("Critical threshold percentage")
                onValueModified: editor.store.setField("criticalPercent", value)
            }

            QQC2.CheckBox {
                Kirigami.FormData.label: i18n("Notifications:")
                text: i18n("Notify on state transitions")
                checked: !!editor.policy.notifyEnabled
                activeFocusOnTab: true
                Accessible.name: text
                onToggled: editor.store.setField("notifyEnabled", checked)
            }

            RowLayout {
                Kirigami.FormData.label: i18n("Snooze:")

                QQC2.Label {
                    Layout.fillWidth: true
                    text: editor.policy.snoozedUntilUtc
                        ? i18n("Until %1", new Date(editor.policy.snoozedUntilUtc)
                               .toLocaleString(Qt.locale()))
                        : i18n("Not snoozed")
                    elide: Text.ElideRight
                }
                QQC2.Button {
                    text: editor.policy.snoozedUntilUtc
                        ? i18n("Clear") : i18n("Until reset")
                    activeFocusOnTab: true
                    Accessible.name: editor.policy.snoozedUntilUtc
                        ? i18n("Clear policy snooze")
                        : i18n("Snooze policy until the next period")
                    onClicked: editor.policy.snoozedUntilUtc
                        ? editor.store.clearSnooze()
                        : editor.store.setSnoozedUntilNextPeriod()
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            Accessible.role: Accessible.Grouping
            Accessible.name: i18n("Live policy preview: %1", editor.riskLabel())

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    Kirigami.Heading {
                        Layout.fillWidth: true
                        level: 4
                        text: i18n("Live threshold preview")
                    }
                    QQC2.Label {
                        text: editor.riskLabel()
                        color: editor.riskColor()
                        font.bold: true
                    }
                }

                Item {
                    id: pacingRail
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: Kirigami.Units.smallSpacing
                        radius: height / 2
                        color: Qt.alpha(Kirigami.Theme.textColor, 0.12)
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * Math.min(1, editor.previewPercent / 100)
                        height: Kirigami.Units.smallSpacing
                        radius: height / 2
                        color: editor.riskColor()
                    }
                    Rectangle {
                        x: parent.width * Number(editor.policy.warningPercent || 80) / 100
                        anchors.verticalCenter: parent.verticalCenter
                        width: 2
                        height: Kirigami.Units.gridUnit
                        color: Kirigami.Theme.neutralTextColor
                    }
                    Rectangle {
                        x: parent.width * Number(editor.policy.criticalPercent || 90) / 100
                        anchors.verticalCenter: parent.verticalCenter
                        width: 2
                        height: Kirigami.Units.gridUnit
                        color: Kirigami.Theme.negativeTextColor
                    }
                }

                QQC2.Slider {
                    Layout.fillWidth: true
                    from: 0
                    to: 200
                    stepSize: 1
                    value: editor.previewPercent
                    activeFocusOnTab: true
                    Accessible.name: i18n("Preview consumed percentage")
                    onMoved: editor.previewPercent = Math.round(value)
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    text: i18n("Preview at %1% consumed · warning %2% · critical %3%",
                               editor.previewPercent,
                               Number(editor.policy.warningPercent || 80),
                               Number(editor.policy.criticalPercent || 90))
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                }
            }
        }

        Item { Layout.preferredHeight: Kirigami.Units.largeSpacing }
    }

    QQC2.Label {
        anchors.centerIn: parent
        width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2,
                        Kirigami.Units.gridUnit * 22)
        visible: !editor.policy.policyId
        text: i18n("Create a policy or select one from the list.")
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: Kirigami.Theme.disabledTextColor
        Accessible.name: text
    }
}
