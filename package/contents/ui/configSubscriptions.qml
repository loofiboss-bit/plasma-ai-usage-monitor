import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0

KCM.SimpleKCM {
    id: subscriptionsPage

    property bool advancedMode: plasmoid.configuration.advancedSettingsMode

    onAdvancedModeChanged: {
        plasmoid.configuration.advancedSettingsMode = advancedMode
    }

    // ── Browser Sync ──
    property alias cfg_browserSyncEnabled: browserSyncSwitch.checked
    property alias cfg_browserSyncBrowser: browserSyncBrowserCombo.currentIndex
    property alias cfg_browserSyncInterval: browserSyncIntervalSpin.value
    property string cfg_browserSyncProfile: plasmoid.configuration.browserSyncProfile || ""

    // ── Claude Code ──
    property alias cfg_claudeCodeEnabled: claudeCodeSwitch.checked
    property alias cfg_claudeCodePlan: claudeCodePlanCombo.currentIndex
    property string cfg_claudeCodePlanId: plasmoid.configuration.claudeCodePlanId || "pro"
    property alias cfg_claudeCodeCustomLimit: claudeCodeLimitSpin.value
    property alias cfg_claudeCodeNotifications: claudeCodeNotifySwitch.checked

    // ── Codex CLI ──
    property alias cfg_codexEnabled: codexSwitch.checked
    property alias cfg_codexPlan: codexPlanCombo.currentIndex
    property string cfg_codexPlanId: plasmoid.configuration.codexPlanId || "plus"
    property alias cfg_codexCustomLimit: codexLimitSpin.value
    property alias cfg_codexNotifications: codexNotifySwitch.checked

    // ── GitHub Copilot ──
    property alias cfg_copilotEnabled: copilotSwitch.checked
    property alias cfg_copilotPlan: copilotPlanCombo.currentIndex
    property string cfg_copilotPlanId: plasmoid.configuration.copilotPlanId || "free"
    property alias cfg_copilotCustomLimit: copilotLimitSpin.value
    property string cfg_copilotBillingMode: normalizeCopilotBillingMode(plasmoid.configuration.copilotBillingMode || "auto")
    property alias cfg_copilotResetDay: copilotResetDaySpin.value
    property alias cfg_copilotNotifications: copilotNotifySwitch.checked
    property alias cfg_copilotOrgName: copilotOrgField.text

    // ── Cursor ──
    property alias cfg_cursorEnabled: cursorSwitch.checked
    property alias cfg_cursorPlan: cursorPlanCombo.currentIndex
    property string cfg_cursorPlanId: plasmoid.configuration.cursorPlanId || "pro"
    property alias cfg_cursorCustomLimit: cursorLimitSpin.value
    property alias cfg_cursorNotifications: cursorNotifySwitch.checked

    // ── Windsurf ──
    property alias cfg_windsurfEnabled: windsurfSwitch.checked
    property alias cfg_windsurfPlan: windsurfPlanCombo.currentIndex
    property string cfg_windsurfPlanId: plasmoid.configuration.windsurfPlanId || "pro"
    property alias cfg_windsurfCustomLimit: windsurfLimitSpin.value
    property alias cfg_windsurfNotifications: windsurfNotifySwitch.checked

    // ── JetBrains AI ──
    property alias cfg_jetbrainsAiEnabled: jetbrainsAiSwitch.checked
    property alias cfg_jetbrainsAiPlan: jetbrainsAiPlanCombo.currentIndex
    property string cfg_jetbrainsAiPlanId: plasmoid.configuration.jetbrainsAiPlanId || "ai_free"
    property alias cfg_jetbrainsAiCustomLimit: jetbrainsAiLimitSpin.value
    property alias cfg_jetbrainsAiNotifications: jetbrainsAiNotifySwitch.checked

    // Track key dirtiness for Copilot PAT
    property bool copilotTokenDirty: false

    function normalizedSyncCode(code) {
        if (code === "not_found") return "cookies_not_found";
        if (code === "expired") return "session_missing_or_expired";
        return code;
    }

    function normalizeCopilotBillingMode(mode) {
        if (!mode || mode === "auto") return "auto";
        if (mode === "premium_requests" || mode === "premium_requests_legacy") return "premium_requests_legacy";
        if (mode === "usage_based" || mode === "credits" || mode === "ai_credits_usage_based") return "ai_credits_usage_based";
        return "auto";
    }

    function syncStatusColor(code) {
        var normalized = normalizedSyncCode(code);
        if (normalized === "connected") return Kirigami.Theme.positiveTextColor;
        if (normalized === "session_missing_or_expired") return Kirigami.Theme.neutralTextColor;
        return Kirigami.Theme.negativeTextColor;
    }

    function syncGuidance(code, serviceLabel) {
        var normalized = normalizedSyncCode(code);
        if (normalized === "connected") return i18n("%1 session looks valid in Firefox.", serviceLabel);
        if (normalized === "profile_missing") return i18n("Choose a supported browser profile or open the browser once.");
        if (normalized === "cookie_db_missing") return i18n("Open Firefox once, sign in to %1, then retry so the cookie database exists.", serviceLabel);
        if (normalized === "cookies_not_found") return i18n("Open %1 in Firefox and sign in at least once.", serviceLabel);
        if (normalized === "session_missing_or_expired") return i18n("Log in to %1 again in Firefox, then retry.", serviceLabel);
        if (normalized === "unsupported_browser") return i18n("The selected browser profile is not supported for sync.");
        return i18n("Check your browser session and retry.");
    }

    function planIndexFor(detector, planId, legacyIndex) {
        var plans = detector.availablePlans();
        if (planId && detector.planIdForLabel) {
            for (var i = 0; i < plans.length; i++) {
                if (detector.planIdForLabel(plans[i]) === planId) {
                    return i;
                }
            }
        }
        if (legacyIndex >= 0 && legacyIndex < plans.length) {
            return legacyIndex;
        }
        return 0;
    }

    function persistPlanId(detector, combo, cfgProperty, configKey) {
        var plans = detector.availablePlans();
        if (combo.currentIndex < 0 || combo.currentIndex >= plans.length) {
            return;
        }
        var id = detector.planIdForLabel(plans[combo.currentIndex]);
        subscriptionsPage[cfgProperty] = id;
        plasmoid.configuration[configKey] = id;
    }

    function reloadBrowserProfiles() {
        var profiles = syncDetector.browserProfiles();
        var entries = [i18n("Auto (Default Profile)")];
        for (var i = 0; i < profiles.length; i++) {
            entries.push(profiles[i]);
        }
        firefoxProfileCombo.model = entries;

        if (!cfg_browserSyncProfile || cfg_browserSyncProfile.length === 0) {
            firefoxProfileCombo.currentIndex = 0;
            syncDetector.selectedFirefoxProfile = "";
            return;
        }

        var idx = entries.indexOf(cfg_browserSyncProfile);
        if (idx >= 0) {
            firefoxProfileCombo.currentIndex = idx;
            syncDetector.selectedFirefoxProfile = cfg_browserSyncProfile;
        } else {
            // Persisted profile no longer exists; fall back safely.
            firefoxProfileCombo.currentIndex = 0;
            cfg_browserSyncProfile = "";
            syncDetector.selectedFirefoxProfile = "";
        }
    }

    // ── Temporary monitors for detection ──
    ClaudeCodeMonitor {
        id: claudeDetector
        Component.onCompleted: checkToolInstalled()
    }

    CodexCliMonitor {
        id: codexDetector
        Component.onCompleted: checkToolInstalled()
    }

    CopilotMonitor {
        id: copilotDetector
        Component.onCompleted: checkToolInstalled()
    }

    CursorMonitor {
        id: cursorDetector
        Component.onCompleted: checkToolInstalled()
    }

    WindsurfMonitor {
        id: windsurfDetector
        Component.onCompleted: checkToolInstalled()
    }

    JetBrainsAiMonitor {
        id: jetbrainsAiDetector
        Component.onCompleted: checkToolInstalled()
    }

    // ── KWallet Integration for Copilot token ──
    SecretsManager {
        id: secrets

        onWalletOpenChanged: {
            if (walletOpen) {
                loadCopilotToken();
            }
        }

        onKeyStored: function(provider) {
            console.log("Key stored for", provider);
        }

        onError: function(message) {
            console.warn("SecretsManager error:", message);
        }
    }

    function loadCopilotToken() {
        if (secrets.hasKey("copilot_github")) {
            copilotTokenField.text = "********";
            copilotTokenDirty = false;
        } else {
            copilotTokenField.text = "";
        }
    }

    function saveCopilotToken() {
        if (copilotTokenDirty && copilotTokenField.text.length > 0 && copilotTokenField.text !== "********") {
            secrets.storeKey("copilot_github", copilotTokenField.text);
        } else if (copilotTokenDirty && copilotTokenField.text.length === 0) {
            secrets.removeKey("copilot_github");
        }
    }

    Component.onCompleted: {
        if (secrets.walletOpen) {
            loadCopilotToken();
        }
        reloadBrowserProfiles();
    }

    Component.onDestruction: {
        saveCopilotToken();
    }

    
    Kirigami.FormLayout {
        anchors.fill: parent
        
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Settings Mode")
        }
        
        QQC2.Switch {
            id: advancedModeSwitch
            Kirigami.FormData.label: i18n("Advanced Mode:")
            checked: subscriptionsPage.advancedMode
            onCheckedChanged: subscriptionsPage.advancedMode = checked
            QQC2.ToolTip.text: i18n("Show advanced configuration options like custom limits, notifications, and Labs features.")
            QQC2.ToolTip.visible: hovered
        }
        

        // ── Description ──
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18n("Track usage limits for AI coding tools with fixed subscription quotas. "
                     + "These tools don't expose public APIs for quota checking, so usage is "
                     + "tracked locally via filesystem monitoring and manual counting.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
        }

        // ══════════════════════════════════════════════
        // ── Claude Code ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Claude Code")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Enable:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: claudeCodeSwitch
                checked: plasmoid.configuration.claudeCodeEnabled
            }

            // Detection status
            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: claudeDetector.installed
                    ? "✓ " + i18n("Detected")
                    : "✗ " + i18n("Not found")
                color: claudeDetector.installed
                    ? Kirigami.Theme.positiveTextColor
                    : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: claudeCodePlanCombo
            Kirigami.FormData.label: i18n("Plan:")
            enabled: claudeCodeSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            model: claudeDetector.availablePlans()
            currentIndex: subscriptionsPage.planIndexFor(claudeDetector, subscriptionsPage.cfg_claudeCodePlanId, plasmoid.configuration.claudeCodePlan)
            onActivated: subscriptionsPage.persistPlanId(claudeDetector, claudeCodePlanCombo, "cfg_claudeCodePlanId", "claudeCodePlanId")
            onCurrentIndexChanged: {
                // Auto-fill default limit when plan changes
                var plans = claudeDetector.availablePlans();
                if (currentIndex >= 0 && currentIndex < plans.length) {
                    var def = claudeDetector.defaultLimitForPlan(plans[currentIndex]);
                    if (claudeCodeLimitSpin.value === 0 || !claudeCodeLimitOverride.checked) {
                        claudeCodeLimitSpin.value = def;
                    }
                }
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Usage limit (per 5h):")
            spacing: Kirigami.Units.smallSpacing
            visible: subscriptionsPage.advancedMode

            QQC2.SpinBox {
                id: claudeCodeLimitSpin
                enabled: claudeCodeSwitch.checked
                from: 0
                to: 99999
                value: plasmoid.configuration.claudeCodeCustomLimit
                editable: true

                Component.onCompleted: {
                    // Set default from plan if not custom
                    if (value === 0) {
                        var plans = claudeDetector.availablePlans();
                        var idx = claudeCodePlanCombo.currentIndex;
                        if (idx >= 0 && idx < plans.length) {
                            value = claudeDetector.defaultLimitForPlan(plans[idx]);
                        }
                    }
                }
            }

            QQC2.CheckBox {
                id: claudeCodeLimitOverride
                text: i18n("Custom")
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.Label {
            visible: claudeCodeSwitch.checked
            text: i18n("Claude Code also has a weekly rolling limit. The secondary limit "
                     + "is automatically calculated from the plan tier.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Switch {
            id: claudeCodeNotifySwitch
            Kirigami.FormData.label: i18n("Notifications:")
            enabled: claudeCodeSwitch.checked
            visible: subscriptionsPage.advancedMode
            checked: plasmoid.configuration.claudeCodeNotifications
        }

        // ══════════════════════════════════════════════
        // ── Codex CLI ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Codex CLI")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Enable:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: codexSwitch
                checked: plasmoid.configuration.codexEnabled
            }

            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: codexDetector.installed
                    ? "✓ " + i18n("Detected")
                    : "✗ " + i18n("Not found")
                color: codexDetector.installed
                    ? Kirigami.Theme.positiveTextColor
                    : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: codexPlanCombo
            Kirigami.FormData.label: i18n("Plan:")
            enabled: codexSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            model: codexDetector.availablePlans()
            currentIndex: subscriptionsPage.planIndexFor(codexDetector, subscriptionsPage.cfg_codexPlanId, plasmoid.configuration.codexPlan)
            onActivated: subscriptionsPage.persistPlanId(codexDetector, codexPlanCombo, "cfg_codexPlanId", "codexPlanId")
            onCurrentIndexChanged: {
                var plans = codexDetector.availablePlans();
                if (currentIndex >= 0 && currentIndex < plans.length) {
                    var def = codexDetector.defaultLimitForPlan(plans[currentIndex]);
                    if (codexLimitSpin.value === 0 || !codexLimitOverride.checked) {
                        codexLimitSpin.value = def;
                    }
                }
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Usage limit (per 5h):")
            spacing: Kirigami.Units.smallSpacing
            visible: subscriptionsPage.advancedMode

            QQC2.SpinBox {
                id: codexLimitSpin
                enabled: codexSwitch.checked
                from: 0
                to: 99999
                value: plasmoid.configuration.codexCustomLimit
                editable: true

                Component.onCompleted: {
                    if (value === 0) {
                        var plans = codexDetector.availablePlans();
                        var idx = codexPlanCombo.currentIndex;
                        if (idx >= 0 && idx < plans.length) {
                            value = codexDetector.defaultLimitForPlan(plans[idx]);
                        }
                    }
                }
            }

            QQC2.CheckBox {
                id: codexLimitOverride
                text: i18n("Custom")
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.Switch {
            id: codexNotifySwitch
            Kirigami.FormData.label: i18n("Notifications:")
            enabled: codexSwitch.checked
            visible: subscriptionsPage.advancedMode
            checked: plasmoid.configuration.codexNotifications
        }

        // ══════════════════════════════════════════════
        // ── GitHub Copilot ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("GitHub Copilot")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Enable:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: copilotSwitch
                checked: plasmoid.configuration.copilotEnabled
            }

            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: copilotDetector.installed
                    ? "✓ " + i18n("Detected")
                    : "✗ " + i18n("Not found")
                color: copilotDetector.installed
                    ? Kirigami.Theme.positiveTextColor
                    : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: copilotPlanCombo
            Kirigami.FormData.label: i18n("Plan:")
            enabled: copilotSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            model: copilotDetector.availablePlans()
            currentIndex: subscriptionsPage.planIndexFor(copilotDetector, subscriptionsPage.cfg_copilotPlanId, plasmoid.configuration.copilotPlan)
            onActivated: subscriptionsPage.persistPlanId(copilotDetector, copilotPlanCombo, "cfg_copilotPlanId", "copilotPlanId")
            onCurrentIndexChanged: {
                var plans = copilotDetector.availablePlans();
                if (currentIndex >= 0 && currentIndex < plans.length) {
                    var def = copilotDetector.defaultLimitForPlan(plans[currentIndex]);
                    if (copilotLimitSpin.value === 0 || !copilotLimitOverride.checked) {
                        copilotLimitSpin.value = def;
                    }
                }
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Premium requests (monthly):")
            spacing: Kirigami.Units.smallSpacing
            visible: subscriptionsPage.advancedMode

            QQC2.SpinBox {
                id: copilotLimitSpin
                enabled: copilotSwitch.checked
                from: 0
                to: 99999
                value: plasmoid.configuration.copilotCustomLimit
                editable: true

                Component.onCompleted: {
                    if (value === 0) {
                        var plans = copilotDetector.availablePlans();
                        var idx = copilotPlanCombo.currentIndex;
                        if (idx >= 0 && idx < plans.length) {
                            value = copilotDetector.defaultLimitForPlan(plans[idx]);
                        }
                    }
                }
            }

            QQC2.CheckBox {
                id: copilotLimitOverride
                text: i18n("Custom")
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: copilotBillingModeCombo
            Kirigami.FormData.label: i18n("Billing mode:")
            enabled: copilotSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            textRole: "text"
            valueRole: "value"
            model: [
                { text: i18n("Auto"), value: "auto" },
                { text: i18n("Premium requests legacy"), value: "premium_requests_legacy" },
                { text: i18n("AI credits usage-based"), value: "ai_credits_usage_based" }
            ]
            Component.onCompleted: {
                for (var i = 0; i < model.length; i++) {
                    if (model[i].value === subscriptionsPage.cfg_copilotBillingMode) {
                        currentIndex = i;
                        return;
                    }
                }
                currentIndex = 0;
            }
            onActivated: {
                subscriptionsPage.cfg_copilotBillingMode = currentValue;
                plasmoid.configuration.copilotBillingMode = currentValue;
            }
        }

        QQC2.SpinBox {
            id: copilotResetDaySpin
            Kirigami.FormData.label: i18n("Reset day:")
            enabled: copilotSwitch.checked
            visible: subscriptionsPage.advancedMode
            from: 1
            to: 28
            value: plasmoid.configuration.copilotResetDay || 1
            editable: true
        }

        QQC2.Label {
            visible: copilotSwitch.checked
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: {
                if (subscriptionsPage.cfg_copilotBillingMode === "ai_credits_usage_based") {
                    return i18n("AI credits mode keeps local activity estimates separate from exact GitHub billing data.");
                }
                if (subscriptionsPage.cfg_copilotBillingMode === "auto") {
                    return i18n("Auto switches from premium requests to AI credits on the GitHub transition date.");
                }
                return i18n("Premium request legacy mode keeps the monthly request counter and configurable reset day.");
            }
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
        }

        QQC2.Switch {
            id: copilotNotifySwitch
            Kirigami.FormData.label: i18n("Notifications:")
            enabled: copilotSwitch.checked
            visible: subscriptionsPage.advancedMode
            checked: plasmoid.configuration.copilotNotifications
        }

        // ── Optional GitHub API integration ──
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("GitHub API (Optional)")
            visible: copilotSwitch.checked
        }

        QQC2.Label {
            visible: copilotSwitch.checked
            text: i18n("Provide a GitHub Personal Access Token to fetch organization-level "
                     + "Copilot seat metrics. Requires 'manage_billing:copilot' scope.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Kirigami.FormData.label: i18n("GitHub Token:")
            visible: copilotSwitch.checked
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: copilotTokenField
                enabled: copilotSwitch.checked
                echoMode: copilotTokenVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: i18n("ghp_...")
                Layout.fillWidth: true
                onTextEdited: subscriptionsPage.copilotTokenDirty = true
            }

            QQC2.ToolButton {
                id: copilotTokenVisible
                checkable: true; checked: false
                icon.name: checked ? "password-show-off" : "password-show-on"
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: checked ? i18n("Hide token") : i18n("Show token")
                QQC2.ToolTip.visible: hovered
            }

            QQC2.ToolButton {
                icon.name: "edit-clear"
                enabled: copilotTokenField.text.length > 0
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Clear token"); QQC2.ToolTip.visible: hovered
                onClicked: { copilotTokenField.text = ""; subscriptionsPage.copilotTokenDirty = true; }
            }
        }

        QQC2.TextField {
            id: copilotOrgField
            Kirigami.FormData.label: i18n("Organization:")
            visible: copilotSwitch.checked
            enabled: copilotSwitch.checked
            text: plasmoid.configuration.copilotOrgName
            placeholderText: i18n("my-org-name")
            Layout.fillWidth: true
        }

        // ══════════════════════════════════════════════
        // ── Cursor ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Cursor")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Enable:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: cursorSwitch
                checked: plasmoid.configuration.cursorEnabled
            }

            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: cursorDetector.installed ? "✓ " + i18n("Detected") : "✗ " + i18n("Not found")
                color: cursorDetector.installed ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: cursorPlanCombo
            Kirigami.FormData.label: i18n("Plan:")
            enabled: cursorSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            model: cursorDetector.availablePlans()
            currentIndex: subscriptionsPage.planIndexFor(cursorDetector, subscriptionsPage.cfg_cursorPlanId, plasmoid.configuration.cursorPlan)
            onActivated: subscriptionsPage.persistPlanId(cursorDetector, cursorPlanCombo, "cfg_cursorPlanId", "cursorPlanId")
        }

        QQC2.SpinBox {
            id: cursorLimitSpin
            Kirigami.FormData.label: i18n("Usage limit:")
            enabled: cursorSwitch.checked
            visible: subscriptionsPage.advancedMode
            from: 0
            to: 99999
            value: plasmoid.configuration.cursorCustomLimit || cursorDetector.defaultLimitForPlan(cursorDetector.availablePlans()[cursorPlanCombo.currentIndex] || "Pro")
            editable: true
        }

        QQC2.Switch {
            id: cursorNotifySwitch
            Kirigami.FormData.label: i18n("Notifications:")
            enabled: cursorSwitch.checked
            visible: subscriptionsPage.advancedMode
            checked: plasmoid.configuration.cursorNotifications
        }

        // ══════════════════════════════════════════════
        // ── Windsurf ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Windsurf")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Enable:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: windsurfSwitch
                checked: plasmoid.configuration.windsurfEnabled
            }

            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: windsurfDetector.installed ? "✓ " + i18n("Detected") : "✗ " + i18n("Not found")
                color: windsurfDetector.installed ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: windsurfPlanCombo
            Kirigami.FormData.label: i18n("Plan:")
            enabled: windsurfSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            model: windsurfDetector.availablePlans()
            currentIndex: subscriptionsPage.planIndexFor(windsurfDetector, subscriptionsPage.cfg_windsurfPlanId, plasmoid.configuration.windsurfPlan)
            onActivated: subscriptionsPage.persistPlanId(windsurfDetector, windsurfPlanCombo, "cfg_windsurfPlanId", "windsurfPlanId")
        }

        QQC2.SpinBox {
            id: windsurfLimitSpin
            Kirigami.FormData.label: i18n("Usage limit:")
            enabled: windsurfSwitch.checked
            visible: subscriptionsPage.advancedMode
            from: 0
            to: 99999
            value: plasmoid.configuration.windsurfCustomLimit || windsurfDetector.defaultLimitForPlan(windsurfDetector.availablePlans()[windsurfPlanCombo.currentIndex] || "Pro")
            editable: true
        }

        QQC2.Switch {
            id: windsurfNotifySwitch
            Kirigami.FormData.label: i18n("Notifications:")
            enabled: windsurfSwitch.checked
            visible: subscriptionsPage.advancedMode
            checked: plasmoid.configuration.windsurfNotifications
        }

        // ══════════════════════════════════════════════
        // ── JetBrains AI ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("JetBrains AI")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Enable:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: jetbrainsAiSwitch
                checked: plasmoid.configuration.jetbrainsAiEnabled
            }

            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: jetbrainsAiDetector.installed ? "✓ " + i18n("Detected") : "✗ " + i18n("Not found")
                color: jetbrainsAiDetector.installed ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: jetbrainsAiPlanCombo
            Kirigami.FormData.label: i18n("Plan:")
            enabled: jetbrainsAiSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            model: jetbrainsAiDetector.availablePlans()
            currentIndex: subscriptionsPage.planIndexFor(jetbrainsAiDetector, subscriptionsPage.cfg_jetbrainsAiPlanId, plasmoid.configuration.jetbrainsAiPlan)
            onActivated: subscriptionsPage.persistPlanId(jetbrainsAiDetector, jetbrainsAiPlanCombo, "cfg_jetbrainsAiPlanId", "jetbrainsAiPlanId")
        }

        QQC2.SpinBox {
            id: jetbrainsAiLimitSpin
            Kirigami.FormData.label: i18n("Usage limit:")
            enabled: jetbrainsAiSwitch.checked
            visible: subscriptionsPage.advancedMode
            from: 0
            to: 99999
            value: plasmoid.configuration.jetbrainsAiCustomLimit || jetbrainsAiDetector.defaultLimitForPlan(jetbrainsAiDetector.availablePlans()[jetbrainsAiPlanCombo.currentIndex] || "AI Free")
            editable: true
        }

        QQC2.Switch {
            id: jetbrainsAiNotifySwitch
            Kirigami.FormData.label: i18n("Notifications:")
            enabled: jetbrainsAiSwitch.checked
            visible: subscriptionsPage.advancedMode
            checked: plasmoid.configuration.jetbrainsAiNotifications
        }

        // ══════════════════════════════════════════════
        // ── Browser Sync (Experimental) ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Labs: Browser Sync (Experimental)")
            visible: subscriptionsPage.advancedMode
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18n("Sync real-time usage data from Claude browser cookies and the existing local Codex login. "
                     + "Browser cookie databases are read-only; credentials are never stored by the widget.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
        }

        Rectangle {
            Layout.fillWidth: true
            height: disclaimerLabel.implicitHeight + Kirigami.Units.smallSpacing * 2
            radius: Kirigami.Units.cornerRadius
            color: Qt.alpha(Kirigami.Theme.neutralTextColor, 0.08)
            border.width: 1
            border.color: Qt.alpha(Kirigami.Theme.neutralTextColor, 0.2)

            QQC2.Label {
                id: disclaimerLabel
                anchors {
                    fill: parent
                    margins: Kirigami.Units.smallSpacing
                }
                wrapMode: Text.WordWrap
                text: i18n("⚠ This feature uses internal, undocumented APIs. It may stop working "
                         + "if services change their API. Credentials are sent only to the "
                         + "corresponding official service.")
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.neutralTextColor
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Enable sync:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: browserSyncSwitch
                checked: plasmoid.configuration.browserSyncEnabled
            }

        QQC2.Label {
            Layout.fillWidth: true
            elide: Text.ElideRight
            text: syncDetector.hasCurrentBrowserProfile
                ? "✓ " + i18n("Browser profile found")
                : "✗ " + i18n("No browser profile")
            color: syncDetector.hasCurrentBrowserProfile
                    ? Kirigami.Theme.positiveTextColor
                    : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: browserSyncBrowserCombo
            Kirigami.FormData.label: i18n("Browser:")
            enabled: browserSyncSwitch.checked
            Layout.fillWidth: true
            model: [
                i18n("Firefox"),
                i18n("Chrome"),
                i18n("Chromium"),
                i18n("Brave")
            ]
            currentIndex: plasmoid.configuration.browserSyncBrowser

            onActivated: {
                cfg_browserSyncBrowser = currentIndex;
                syncDetector.browserType = currentIndex;
                reloadBrowserProfiles();
            }
        }

        QQC2.Label {
            visible: browserSyncSwitch.checked
            text: i18n("Browser Sync supports Firefox plus Linux Chrome, Chromium, and Brave profiles when readable cookies and safe-storage secrets are available. If Labs sync is not ready, local estimation still works.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: browserSyncSwitch.checked
            Kirigami.FormData.label: i18n("Readiness:")
            text: syncDetector.readinessSummary
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: syncDetector.hasCurrentBrowserProfile && syncDetector.hasSafeStorageAccess
                ? Kirigami.Theme.positiveTextColor
                : Kirigami.Theme.neutralTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Browser profile:")
            visible: browserSyncSwitch.checked
            enabled: browserSyncSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.ComboBox {
                id: firefoxProfileCombo
                Layout.fillWidth: true
                model: [i18n("Auto (Default Profile)")]
                onActivated: {
                    if (currentIndex <= 0) {
                        cfg_browserSyncProfile = "";
                        syncDetector.selectedFirefoxProfile = "";
                    } else {
                        cfg_browserSyncProfile = currentText;
                        syncDetector.selectedFirefoxProfile = currentText;
                    }
                }
            }

            QQC2.ToolButton {
                icon.name: "view-refresh"
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Reload browser profiles")
                QQC2.ToolTip.visible: hovered
                onClicked: reloadBrowserProfiles()
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Sync interval:")
            enabled: browserSyncSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.SpinBox {
                id: browserSyncIntervalSpin
                from: 60
                to: 3600
                stepSize: 60
                value: plasmoid.configuration.browserSyncInterval
                editable: true

                textFromValue: function(value, locale) {
                    return Math.floor(value / 60) + " min";
                }
                valueFromText: function(text, locale) {
                    return parseInt(text) * 60;
                }
            }

            QQC2.Label {
                text: i18n("(minimum 60 seconds)")
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.disabledTextColor
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        // Connection test
        RowLayout {
            Kirigami.FormData.label: i18n("Connection test:")
            visible: browserSyncSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.Button {
                text: i18n("Test Claude.ai")
                icon.name: "network-connect"
                onClicked: {
                    var result = subscriptionsPage.normalizedSyncCode(syncDetector.testConnection("claude"));
                    claudeTestLabel.text = syncDetector.connectionMessage("claude", result);
                    claudeTestLabel.color = subscriptionsPage.syncStatusColor(result);
                    claudeTestLabel.visible = true;
                    claudeGuidanceLabel.text = subscriptionsPage.syncGuidance(result, "claude.ai");
                    claudeGuidanceLabel.visible = true;
                }
            }
            QQC2.Label {
                id: claudeTestLabel
                visible: false
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Kirigami.FormData.label: " "
            visible: browserSyncSwitch.checked && claudeGuidanceLabel.visible
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                id: claudeGuidanceLabel
                visible: false
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.disabledTextColor
                Layout.fillWidth: true
            }
        }

        RowLayout {
            Kirigami.FormData.label: " "
            visible: browserSyncSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.Button {
                text: i18n("Test ChatGPT")
                icon.name: "network-connect"
                onClicked: {
                    var result = subscriptionsPage.normalizedSyncCode(syncDetector.testConnection("chatgpt"));
                    chatgptTestLabel.text = syncDetector.connectionMessage("chatgpt", result);
                    chatgptTestLabel.color = subscriptionsPage.syncStatusColor(result);
                    chatgptTestLabel.visible = true;
                    chatgptGuidanceLabel.text = subscriptionsPage.syncGuidance(result, "chatgpt.com");
                    chatgptGuidanceLabel.visible = true;
                }
            }
            QQC2.Label {
                id: chatgptTestLabel
                visible: false
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Kirigami.FormData.label: " "
            visible: browserSyncSwitch.checked && chatgptGuidanceLabel.visible
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                id: chatgptGuidanceLabel
                visible: false
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.disabledTextColor
                Layout.fillWidth: true
            }
        }
    }

    // Browser Sync exposes readiness only; cookie values remain in C++.
    BrowserSyncService {
        id: syncDetector
        browserType: subscriptionsPage.cfg_browserSyncBrowser
        selectedFirefoxProfile: subscriptionsPage.cfg_browserSyncProfile
    }
}
