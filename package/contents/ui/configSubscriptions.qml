import QtQuick
import org.kde.plasma.plasmoid
import org.kde.ki18n
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0

KCM.SimpleKCM {
    id: subscriptionsPage
    signal configurationChanged()

    property bool advancedMode: Plasmoid.configuration.advancedSettingsMode

    onAdvancedModeChanged: {
        Plasmoid.configuration.advancedSettingsMode = advancedMode
    }

    // ── Browser Sync ──
    property alias cfg_browserSyncEnabled: browserSyncSwitch.checked
    property alias cfg_browserSyncBrowser: browserSyncBrowserCombo.currentIndex
    property alias cfg_browserSyncInterval: browserSyncIntervalSpin.value
    property string cfg_browserSyncProfile: Plasmoid.configuration.browserSyncProfile || ""

    // ── Google Antigravity ──
    property alias cfg_antigravityEnabled: antigravitySwitch.checked
    property alias cfg_antigravityNotifications: antigravityNotifySwitch.checked
    property alias cfg_antigravityRefreshInterval: antigravityRefreshSpin.value

    // ── Claude Code ──
    property alias cfg_claudeCodeEnabled: claudeCodeSwitch.checked
    property alias cfg_claudeCodePlan: claudeCodePlanCombo.currentIndex
    property string cfg_claudeCodePlanId: Plasmoid.configuration.claudeCodePlanId || "pro"
    property alias cfg_claudeCodeCustomLimit: claudeCodeLimitSpin.value
    property alias cfg_claudeCodeNotifications: claudeCodeNotifySwitch.checked

    // ── Codex CLI ──
    property alias cfg_codexEnabled: codexSwitch.checked
    property alias cfg_codexPlan: codexPlanCombo.currentIndex
    property string cfg_codexPlanId: Plasmoid.configuration.codexPlanId || "plus"
    property alias cfg_codexCustomLimit: codexLimitSpin.value
    property alias cfg_codexNotifications: codexNotifySwitch.checked

    // ── GitHub Copilot ──
    property alias cfg_copilotEnabled: copilotSwitch.checked
    property alias cfg_copilotPlan: copilotPlanCombo.currentIndex
    property string cfg_copilotPlanId: Plasmoid.configuration.copilotPlanId || "free"
    property alias cfg_copilotCustomLimit: copilotLimitSpin.value
    property string cfg_copilotBillingMode: normalizeCopilotBillingMode(Plasmoid.configuration.copilotBillingMode || "auto")
    property alias cfg_copilotResetDay: copilotResetDaySpin.value
    property alias cfg_copilotNotifications: copilotNotifySwitch.checked
    property alias cfg_copilotOrgName: copilotOrgField.text

    // ── Cursor ──
    property alias cfg_cursorEnabled: cursorSwitch.checked
    property alias cfg_cursorPlan: cursorPlanCombo.currentIndex
    property string cfg_cursorPlanId: Plasmoid.configuration.cursorPlanId || "pro"
    property alias cfg_cursorCustomLimit: cursorLimitSpin.value
    property alias cfg_cursorNotifications: cursorNotifySwitch.checked

    // ── Windsurf ──
    property alias cfg_windsurfEnabled: windsurfSwitch.checked
    property alias cfg_windsurfPlan: windsurfPlanCombo.currentIndex
    property string cfg_windsurfPlanId: Plasmoid.configuration.windsurfPlanId || "pro"
    property alias cfg_windsurfCustomLimit: windsurfLimitSpin.value
    property alias cfg_windsurfNotifications: windsurfNotifySwitch.checked

    // ── JetBrains AI ──
    property alias cfg_jetbrainsAiEnabled: jetbrainsAiSwitch.checked
    property alias cfg_jetbrainsAiPlan: jetbrainsAiPlanCombo.currentIndex
    property string cfg_jetbrainsAiPlanId: Plasmoid.configuration.jetbrainsAiPlanId || "ai_free"
    property alias cfg_jetbrainsAiCustomLimit: jetbrainsAiLimitSpin.value
    property alias cfg_jetbrainsAiNotifications: jetbrainsAiNotifySwitch.checked

    readonly property bool unsavedChanges: secretChanges.dirty
    property string secretStatusMessage: ""
    property bool secretStatusError: false

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
        if (normalized === "connected") return KI18n.i18n("%1 session looks valid in Firefox.", serviceLabel);
        if (normalized === "profile_missing") return KI18n.i18n("Choose a supported browser profile or open the browser once.");
        if (normalized === "cookie_db_missing") return KI18n.i18n("Open Firefox once, sign in to %1, then retry so the cookie database exists.", serviceLabel);
        if (normalized === "cookies_not_found") return KI18n.i18n("Open %1 in Firefox and sign in at least once.", serviceLabel);
        if (normalized === "session_missing_or_expired") return KI18n.i18n("Log in to %1 again in Firefox, then retry.", serviceLabel);
        if (normalized === "unsupported_browser") return KI18n.i18n("The selected browser profile is not supported for sync.");
        return KI18n.i18n("Check your browser session and retry.");
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
        Plasmoid.configuration[configKey] = id;
    }

    function reloadBrowserProfiles() {
        var profiles = syncDetector.browserProfiles();
        var entries = [KI18n.i18n("Auto (Default Profile)")];
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

    AntigravityMonitor {
        id: antigravityDetector
        Component.onCompleted: {
            checkToolInstalled()
            if (Plasmoid.configuration.antigravityEnabled) refreshQuota()
        }
    }

    // ── KWallet Integration for Copilot token ──
    SecretsManager {
        id: secrets

        onWalletOpenChanged: {
            if (walletOpen && !secretChanges.dirty) {
                subscriptionsPage.loadCopilotToken();
            }
        }

        onKeyStored: function(provider) {
            console.log("Key stored for", provider);
        }

        onError: function(message) {
            console.warn("SecretsManager error:", message);
        }
    }

    SecretChangeSet {
        id: secretChanges
        store: secrets
    }

    function loadCopilotToken() {
        if (secrets.hasKey("copilot_github")) {
            copilotTokenField.text = "********";
        } else {
            copilotTokenField.text = "";
        }
    }

    function saveConfig() {
        var result = secretChanges.commit();
        secretStatusError = !result.ok;
        if (result.ok) {
            secretStatusMessage = result.appliedKeys.length > 0
                ? KI18n.i18n("GitHub token saved securely in KDE Wallet.") : "";
            if (result.appliedKeys.length > 0) subscriptionsPage.loadCopilotToken();
        } else if (result.message === "wallet-not-open") {
            secretStatusMessage = KI18n.i18n("KDE Wallet is not open. Unlock it and retry Apply.");
        } else {
            secretStatusMessage = KI18n.i18n("The GitHub token could not be saved. Retry Apply.");
        }
        if (!result.ok) {
            Qt.callLater(function() { subscriptionsPage.configurationChanged(); });
        }
    }

    Component.onCompleted: {
        if (secrets.walletOpen) {
            subscriptionsPage.loadCopilotToken();
        }
        subscriptionsPage.reloadBrowserProfiles();
    }

    Kirigami.FormLayout {
        anchors.fill: parent

        Kirigami.InlineMessage {
            visible: subscriptionsPage.secretStatusMessage.length > 0
            text: subscriptionsPage.secretStatusMessage
            type: subscriptionsPage.secretStatusError ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive
            Layout.fillWidth: true
        }
        
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("Settings Mode")
        }
        
        QQC2.Switch {
            id: advancedModeSwitch
            Kirigami.FormData.label: KI18n.i18n("Advanced Mode:")
            checked: subscriptionsPage.advancedMode
            onCheckedChanged: subscriptionsPage.advancedMode = checked
            QQC2.ToolTip.text: KI18n.i18n("Show advanced configuration options like custom limits, notifications, and Labs features.")
            QQC2.ToolTip.visible: hovered
        }
        

        // ── Description ──
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: KI18n.i18n("Track subscription quotas for AI coding tools. Most tools use local activity estimates; "
                     + "supported authenticated sources such as Antigravity local can report live quota windows.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
        }

        // ══════════════════════════════════════════════
        // ── Google Antigravity ──
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("Google Antigravity")
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("Google Antigravity:")
            QQC2.Switch {
                id: antigravitySwitch
                checked: Plasmoid.configuration.antigravityEnabled
                text: antigravityDetector.installed ? KI18n.i18n("Installed") : KI18n.i18n("Not installed")
            }
            QQC2.Button {
                text: antigravityDetector.syncing ? KI18n.i18n("Refreshing…") : KI18n.i18n("Refresh / Test connection")
                enabled: !antigravityDetector.syncing
                icon.name: "view-refresh"
                onClicked: antigravityDetector.refreshQuota()
            }
        }

        QQC2.Label {
            Kirigami.FormData.label: KI18n.i18n("Detected plan:")
            text: antigravityDetector.detectedPlanLabel || KI18n.i18n("Detected automatically")
        }

        QQC2.Label {
            Kirigami.FormData.label: KI18n.i18n("Daemon status:")
            text: antigravityDetector.connectionState
                  + (antigravityDetector.readinessCode ? " (" + antigravityDetector.readinessCode + ")" : "")
        }

        QQC2.Label {
            Kirigami.FormData.label: KI18n.i18n("Last successful sync:")
            text: antigravityDetector.lastSuccessfulRefresh
                  ? Qt.formatDateTime(antigravityDetector.lastSuccessfulRefresh,
                                      Locale.ShortFormat)
                  : KI18n.i18n("Never")
        }

        QQC2.SpinBox {
            id: antigravityRefreshSpin
            Kirigami.FormData.label: KI18n.i18n("Refresh interval:")
            from: 60
            to: 3600
            stepSize: 60
            value: Math.max(60, Plasmoid.configuration.antigravityRefreshInterval || 300)
            enabled: antigravitySwitch.checked
            textFromValue: function(value) { return KI18n.i18n("%1 seconds", value); }
        }

        QQC2.Switch {
            id: antigravityNotifySwitch
            Kirigami.FormData.label: KI18n.i18n("Notifications:")
            text: KI18n.i18n("Warn when model quota is low")
            checked: Plasmoid.configuration.antigravityNotifications
            enabled: antigravitySwitch.checked
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Information
            visible: true
            text: KI18n.i18n("Antigravity must be installed, running, and signed in. The monitor reads only plan and model quota from its authenticated localhost daemon; credentials and prompts are never exported.")
        }

        // ── Claude Code ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("Claude Code")
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("Enable:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: claudeCodeSwitch
                checked: Plasmoid.configuration.claudeCodeEnabled
            }

            // Detection status
            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: claudeDetector.installed
                    ? "✓ " + KI18n.i18n("Detected")
                    : "✗ " + KI18n.i18n("Not found")
                color: claudeDetector.installed
                    ? Kirigami.Theme.positiveTextColor
                    : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: claudeCodePlanCombo
            Kirigami.FormData.label: KI18n.i18n("Plan:")
            enabled: claudeCodeSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            model: claudeDetector.availablePlans()
            currentIndex: subscriptionsPage.planIndexFor(claudeDetector, subscriptionsPage.cfg_claudeCodePlanId, Plasmoid.configuration.claudeCodePlan)
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
            Kirigami.FormData.label: KI18n.i18n("Usage limit (per 5h):")
            spacing: Kirigami.Units.smallSpacing
            visible: subscriptionsPage.advancedMode

            QQC2.SpinBox {
                id: claudeCodeLimitSpin
                enabled: claudeCodeSwitch.checked
                from: 0
                to: 99999
                value: Plasmoid.configuration.claudeCodeCustomLimit
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
                text: KI18n.i18n("Custom")
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.Label {
            visible: claudeCodeSwitch.checked
            text: KI18n.i18n("Claude Code also has a weekly rolling limit. The secondary limit "
                     + "is automatically calculated from the plan tier.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Switch {
            id: claudeCodeNotifySwitch
            Kirigami.FormData.label: KI18n.i18n("Notifications:")
            enabled: claudeCodeSwitch.checked
            visible: subscriptionsPage.advancedMode
            checked: Plasmoid.configuration.claudeCodeNotifications
        }

        // ══════════════════════════════════════════════
        // ── Codex CLI ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("Codex CLI")
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("Enable:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: codexSwitch
                checked: Plasmoid.configuration.codexEnabled
            }

            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: codexDetector.installed
                    ? "✓ " + KI18n.i18n("Detected")
                    : "✗ " + KI18n.i18n("Not found")
                color: codexDetector.installed
                    ? Kirigami.Theme.positiveTextColor
                    : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: codexPlanCombo
            Kirigami.FormData.label: KI18n.i18n("Plan:")
            enabled: codexSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            model: codexDetector.availablePlans()
            currentIndex: subscriptionsPage.planIndexFor(codexDetector, subscriptionsPage.cfg_codexPlanId, Plasmoid.configuration.codexPlan)
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
            Kirigami.FormData.label: KI18n.i18n("Usage limit (per 5h):")
            spacing: Kirigami.Units.smallSpacing
            visible: subscriptionsPage.advancedMode

            QQC2.SpinBox {
                id: codexLimitSpin
                enabled: codexSwitch.checked
                from: 0
                to: 99999
                value: Plasmoid.configuration.codexCustomLimit
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
                text: KI18n.i18n("Custom")
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.Switch {
            id: codexNotifySwitch
            Kirigami.FormData.label: KI18n.i18n("Notifications:")
            enabled: codexSwitch.checked
            visible: subscriptionsPage.advancedMode
            checked: Plasmoid.configuration.codexNotifications
        }

        // ══════════════════════════════════════════════
        // ── GitHub Copilot ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("GitHub Copilot")
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("Enable:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: copilotSwitch
                checked: Plasmoid.configuration.copilotEnabled
            }

            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: copilotDetector.installed
                    ? "✓ " + KI18n.i18n("Detected")
                    : "✗ " + KI18n.i18n("Not found")
                color: copilotDetector.installed
                    ? Kirigami.Theme.positiveTextColor
                    : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: copilotPlanCombo
            Kirigami.FormData.label: KI18n.i18n("Plan:")
            enabled: copilotSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            model: copilotDetector.availablePlans()
            currentIndex: subscriptionsPage.planIndexFor(copilotDetector, subscriptionsPage.cfg_copilotPlanId, Plasmoid.configuration.copilotPlan)
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
            Kirigami.FormData.label: KI18n.i18n("Premium requests (monthly):")
            spacing: Kirigami.Units.smallSpacing
            visible: subscriptionsPage.advancedMode

            QQC2.SpinBox {
                id: copilotLimitSpin
                enabled: copilotSwitch.checked
                from: 0
                to: 99999
                value: Plasmoid.configuration.copilotCustomLimit
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
                text: KI18n.i18n("Custom")
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: copilotBillingModeCombo
            Kirigami.FormData.label: KI18n.i18n("Billing mode:")
            enabled: copilotSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            textRole: "text"
            valueRole: "value"
            model: [
                { text: KI18n.i18n("Auto"), value: "auto" },
                { text: KI18n.i18n("Premium requests legacy"), value: "premium_requests_legacy" },
                { text: KI18n.i18n("AI credits usage-based"), value: "ai_credits_usage_based" }
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
                Plasmoid.configuration.copilotBillingMode = currentValue;
            }
        }

        QQC2.SpinBox {
            id: copilotResetDaySpin
            Kirigami.FormData.label: KI18n.i18n("Reset day:")
            enabled: copilotSwitch.checked
            visible: subscriptionsPage.advancedMode
            from: 1
            to: 28
            value: Plasmoid.configuration.copilotResetDay || 1
            editable: true
        }

        QQC2.Label {
            visible: copilotSwitch.checked
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: {
                if (subscriptionsPage.cfg_copilotBillingMode === "ai_credits_usage_based") {
                    return KI18n.i18n("AI credits mode keeps local activity estimates separate from exact GitHub billing data.");
                }
                if (subscriptionsPage.cfg_copilotBillingMode === "auto") {
                    return KI18n.i18n("Auto switches from premium requests to AI credits on the GitHub transition date.");
                }
                return KI18n.i18n("Premium request legacy mode keeps the monthly request counter and configurable reset day.");
            }
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
        }

        QQC2.Switch {
            id: copilotNotifySwitch
            Kirigami.FormData.label: KI18n.i18n("Notifications:")
            enabled: copilotSwitch.checked
            visible: subscriptionsPage.advancedMode
            checked: Plasmoid.configuration.copilotNotifications
        }

        // ── Optional GitHub API integration ──
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("GitHub API (Optional)")
            visible: copilotSwitch.checked
        }

        QQC2.Label {
            visible: copilotSwitch.checked
            text: KI18n.i18n("Provide a GitHub Personal Access Token to fetch organization-level "
                     + "Copilot seat metrics. Requires 'manage_billing:copilot' scope.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("GitHub Token:")
            visible: copilotSwitch.checked
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: copilotTokenField
                enabled: copilotSwitch.checked && secrets.walletOpen
                echoMode: copilotTokenVisible.checked ? TextInput.Normal : TextInput.Password
                placeholderText: KI18n.i18n("ghp_...")
                Layout.fillWidth: true
                onTextEdited: {
                    if (text.length > 0) secretChanges.stageStore("copilot_github", text);
                    else secretChanges.stageRemove("copilot_github");
                    subscriptionsPage.secretStatusMessage = "";
                    subscriptionsPage.secretStatusError = false;
                }
            }

            QQC2.ToolButton {
                id: copilotTokenVisible
                checkable: true; checked: false
                icon.name: checked ? "password-show-off" : "password-show-on"
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: checked ? KI18n.i18n("Hide token") : KI18n.i18n("Show token")
                QQC2.ToolTip.visible: hovered
            }

            QQC2.ToolButton {
                icon.name: "edit-clear"
                enabled: secrets.walletOpen && copilotTokenField.text.length > 0
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: KI18n.i18n("Clear token"); QQC2.ToolTip.visible: hovered
                onClicked: {
                    copilotTokenField.text = "";
                    secretChanges.stageRemove("copilot_github");
                    subscriptionsPage.secretStatusMessage = "";
                    subscriptionsPage.secretStatusError = false;
                }
            }
        }

        QQC2.TextField {
            id: copilotOrgField
            Kirigami.FormData.label: KI18n.i18n("Organization:")
            visible: copilotSwitch.checked
            enabled: copilotSwitch.checked
            text: Plasmoid.configuration.copilotOrgName
            placeholderText: KI18n.i18n("my-org-name")
            Layout.fillWidth: true
        }

        // ══════════════════════════════════════════════
        // ── Cursor ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("Cursor")
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("Enable:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: cursorSwitch
                checked: Plasmoid.configuration.cursorEnabled
            }

            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: cursorDetector.installed ? "✓ " + KI18n.i18n("Detected") : "✗ " + KI18n.i18n("Not found")
                color: cursorDetector.installed ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: cursorPlanCombo
            Kirigami.FormData.label: KI18n.i18n("Plan:")
            enabled: cursorSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            model: cursorDetector.availablePlans()
            currentIndex: subscriptionsPage.planIndexFor(cursorDetector, subscriptionsPage.cfg_cursorPlanId, Plasmoid.configuration.cursorPlan)
            onActivated: subscriptionsPage.persistPlanId(cursorDetector, cursorPlanCombo, "cfg_cursorPlanId", "cursorPlanId")
        }

        QQC2.SpinBox {
            id: cursorLimitSpin
            Kirigami.FormData.label: KI18n.i18n("Usage limit:")
            enabled: cursorSwitch.checked
            visible: subscriptionsPage.advancedMode
            from: 0
            to: 99999
            value: Plasmoid.configuration.cursorCustomLimit || cursorDetector.defaultLimitForPlan(cursorDetector.availablePlans()[cursorPlanCombo.currentIndex] || "Pro")
            editable: true
        }

        QQC2.Switch {
            id: cursorNotifySwitch
            Kirigami.FormData.label: KI18n.i18n("Notifications:")
            enabled: cursorSwitch.checked
            visible: subscriptionsPage.advancedMode
            checked: Plasmoid.configuration.cursorNotifications
        }

        // ══════════════════════════════════════════════
        // ── Windsurf ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("Windsurf")
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("Enable:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: windsurfSwitch
                checked: Plasmoid.configuration.windsurfEnabled
            }

            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: windsurfDetector.installed ? "✓ " + KI18n.i18n("Detected") : "✗ " + KI18n.i18n("Not found")
                color: windsurfDetector.installed ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: windsurfPlanCombo
            Kirigami.FormData.label: KI18n.i18n("Plan:")
            enabled: windsurfSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            model: windsurfDetector.availablePlans()
            currentIndex: subscriptionsPage.planIndexFor(windsurfDetector, subscriptionsPage.cfg_windsurfPlanId, Plasmoid.configuration.windsurfPlan)
            onActivated: subscriptionsPage.persistPlanId(windsurfDetector, windsurfPlanCombo, "cfg_windsurfPlanId", "windsurfPlanId")
        }

        QQC2.SpinBox {
            id: windsurfLimitSpin
            Kirigami.FormData.label: KI18n.i18n("Usage limit:")
            enabled: windsurfSwitch.checked
            visible: subscriptionsPage.advancedMode
            from: 0
            to: 99999
            value: Plasmoid.configuration.windsurfCustomLimit || windsurfDetector.defaultLimitForPlan(windsurfDetector.availablePlans()[windsurfPlanCombo.currentIndex] || "Pro")
            editable: true
        }

        QQC2.Switch {
            id: windsurfNotifySwitch
            Kirigami.FormData.label: KI18n.i18n("Notifications:")
            enabled: windsurfSwitch.checked
            visible: subscriptionsPage.advancedMode
            checked: Plasmoid.configuration.windsurfNotifications
        }

        // ══════════════════════════════════════════════
        // ── JetBrains AI ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("JetBrains AI")
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("Enable:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: jetbrainsAiSwitch
                checked: Plasmoid.configuration.jetbrainsAiEnabled
            }

            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: jetbrainsAiDetector.installed ? "✓ " + KI18n.i18n("Detected") : "✗ " + KI18n.i18n("Not found")
                color: jetbrainsAiDetector.installed ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: jetbrainsAiPlanCombo
            Kirigami.FormData.label: KI18n.i18n("Plan:")
            enabled: jetbrainsAiSwitch.checked
            visible: subscriptionsPage.advancedMode
            Layout.fillWidth: true
            model: jetbrainsAiDetector.availablePlans()
            currentIndex: subscriptionsPage.planIndexFor(jetbrainsAiDetector, subscriptionsPage.cfg_jetbrainsAiPlanId, Plasmoid.configuration.jetbrainsAiPlan)
            onActivated: subscriptionsPage.persistPlanId(jetbrainsAiDetector, jetbrainsAiPlanCombo, "cfg_jetbrainsAiPlanId", "jetbrainsAiPlanId")
        }

        QQC2.SpinBox {
            id: jetbrainsAiLimitSpin
            Kirigami.FormData.label: KI18n.i18n("Usage limit:")
            enabled: jetbrainsAiSwitch.checked
            visible: subscriptionsPage.advancedMode
            from: 0
            to: 99999
            value: Plasmoid.configuration.jetbrainsAiCustomLimit || jetbrainsAiDetector.defaultLimitForPlan(jetbrainsAiDetector.availablePlans()[jetbrainsAiPlanCombo.currentIndex] || "AI Free")
            editable: true
        }

        QQC2.Switch {
            id: jetbrainsAiNotifySwitch
            Kirigami.FormData.label: KI18n.i18n("Notifications:")
            enabled: jetbrainsAiSwitch.checked
            visible: subscriptionsPage.advancedMode
            checked: Plasmoid.configuration.jetbrainsAiNotifications
        }

        // ══════════════════════════════════════════════
        // ── Browser Sync (Experimental) ──
        // ══════════════════════════════════════════════

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: KI18n.i18n("Labs: Browser Sync (Experimental)")
            visible: subscriptionsPage.advancedMode
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: KI18n.i18n("Sync real-time usage data from Claude browser cookies and the existing local Codex login. "
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
                text: KI18n.i18n("⚠ This feature uses internal, undocumented APIs. It may stop working "
                         + "if services change their API. Credentials are sent only to the "
                         + "corresponding official service.")
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.neutralTextColor
            }
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("Enable sync:")
            spacing: Kirigami.Units.largeSpacing

            QQC2.Switch {
                id: browserSyncSwitch
                checked: Plasmoid.configuration.browserSyncEnabled
            }

        QQC2.Label {
            Layout.fillWidth: true
            elide: Text.ElideRight
            text: syncDetector.hasCurrentBrowserProfile
                ? "✓ " + KI18n.i18n("Browser profile found")
                : "✗ " + KI18n.i18n("No browser profile")
            color: syncDetector.hasCurrentBrowserProfile
                    ? Kirigami.Theme.positiveTextColor
                    : Kirigami.Theme.disabledTextColor
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        QQC2.ComboBox {
            id: browserSyncBrowserCombo
            Kirigami.FormData.label: KI18n.i18n("Browser:")
            enabled: browserSyncSwitch.checked
            Layout.fillWidth: true
            model: [
                KI18n.i18n("Firefox"),
                KI18n.i18n("Chrome"),
                KI18n.i18n("Chromium"),
                KI18n.i18n("Brave")
            ]
            currentIndex: Plasmoid.configuration.browserSyncBrowser

            onActivated: {
                subscriptionsPage.cfg_browserSyncBrowser = currentIndex;
                syncDetector.browserType = currentIndex;
                subscriptionsPage.reloadBrowserProfiles();
            }
        }

        QQC2.Label {
            visible: browserSyncSwitch.checked
            text: KI18n.i18n("Browser Sync supports Firefox plus Linux Chrome, Chromium, and Brave profiles when readable cookies and safe-storage secrets are available. If Labs sync is not ready, local estimation still works.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: browserSyncSwitch.checked
            Kirigami.FormData.label: KI18n.i18n("Readiness:")
            text: syncDetector.readinessSummary
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: syncDetector.hasCurrentBrowserProfile && syncDetector.hasSafeStorageAccess
                ? Kirigami.Theme.positiveTextColor
                : Kirigami.Theme.neutralTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("Browser profile:")
            visible: browserSyncSwitch.checked
            enabled: browserSyncSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.ComboBox {
                id: firefoxProfileCombo
                Layout.fillWidth: true
                model: [KI18n.i18n("Auto (Default Profile)")]
                onActivated: {
                    if (currentIndex <= 0) {
                        subscriptionsPage.cfg_browserSyncProfile = "";
                        syncDetector.selectedFirefoxProfile = "";
                    } else {
                        subscriptionsPage.cfg_browserSyncProfile = currentText;
                        syncDetector.selectedFirefoxProfile = currentText;
                    }
                }
            }

            QQC2.ToolButton {
                icon.name: "view-refresh"
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: KI18n.i18n("Reload browser profiles")
                QQC2.ToolTip.visible: hovered
                onClicked: subscriptionsPage.reloadBrowserProfiles()
            }
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("Sync interval:")
            enabled: browserSyncSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.SpinBox {
                id: browserSyncIntervalSpin
                from: 60
                to: 3600
                stepSize: 60
                value: Plasmoid.configuration.browserSyncInterval
                editable: true

                textFromValue: function(value, locale) {
                    return Math.floor(value / 60) + " min";
                }
                valueFromText: function(text, locale) {
                    return parseInt(text) * 60;
                }
            }

            QQC2.Label {
                text: KI18n.i18n("(minimum 60 seconds)")
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.disabledTextColor
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        // Connection test
        RowLayout {
            Kirigami.FormData.label: KI18n.i18n("Connection test:")
            visible: browserSyncSwitch.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.Button {
                text: KI18n.i18n("Test Claude.ai")
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
                text: KI18n.i18n("Test ChatGPT")
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
