pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import com.github.loofi.aiusagemonitor 1.0
import "components" as Components

KCM.SimpleKCM {
    id: generalPage

    property alias cfg_refreshInterval: refreshSlider.value
    property string cfg_compactDisplayMode: Plasmoid.configuration.compactDisplayMode
    property string cfg_panelSourceScope: Plasmoid.configuration.panelSourceScope || "automatic"
    property string cfg_panelSourceId: Plasmoid.configuration.panelSourceId
    property bool cfg_advancedSettingsMode: Plasmoid.configuration.advancedSettingsMode

    property int cfg_openaiRefreshInterval: Plasmoid.configuration.openaiRefreshInterval
    property int cfg_anthropicRefreshInterval: Plasmoid.configuration.anthropicRefreshInterval
    property int cfg_googleRefreshInterval: Plasmoid.configuration.googleRefreshInterval
    property int cfg_mistralRefreshInterval: Plasmoid.configuration.mistralRefreshInterval
    property int cfg_deepseekRefreshInterval: Plasmoid.configuration.deepseekRefreshInterval
    property int cfg_groqRefreshInterval: Plasmoid.configuration.groqRefreshInterval
    property int cfg_xaiRefreshInterval: Plasmoid.configuration.xaiRefreshInterval
    property int cfg_ollamaRefreshInterval: Plasmoid.configuration.ollamaRefreshInterval
    property int cfg_openrouterRefreshInterval: Plasmoid.configuration.openrouterRefreshInterval
    property int cfg_togetherRefreshInterval: Plasmoid.configuration.togetherRefreshInterval
    property int cfg_cohereRefreshInterval: Plasmoid.configuration.cohereRefreshInterval
    property int cfg_googleveoRefreshInterval: Plasmoid.configuration.googleveoRefreshInterval
    property int cfg_azureRefreshInterval: Plasmoid.configuration.azureRefreshInterval
    property int cfg_bedrockRefreshInterval: Plasmoid.configuration.bedrockRefreshInterval
    property int cfg_litellmRefreshInterval: Plasmoid.configuration.litellmRefreshInterval
    property int cfg_cerebrasRefreshInterval: Plasmoid.configuration.cerebrasRefreshInterval
    property int cfg_fireworksRefreshInterval: Plasmoid.configuration.fireworksRefreshInterval
    property int cfg_perplexityRefreshInterval: Plasmoid.configuration.perplexityRefreshInterval

    property ProviderCatalog providerCatalog: ProviderCatalog {}
    readonly property var enabledProviders: providerCatalog.providers.filter(function(provider) {
        return !!Plasmoid.configuration[provider.enabledConfigKey];
    })

    readonly property var panelSourceChoices: {
        var rows = [{id: "", label: i18n("Choose a source")}];
        var providers = providerCatalog.providers;
        for (var i = 0; i < providers.length; ++i) {
            var provider = providers[i];
            rows.push({id: provider.configKey, label: provider.displayName
                + (!Plasmoid.configuration[provider.enabledConfigKey] ? i18n(" (disabled)") : "")});
        }
        var enabledKeys = {"google-antigravity": "antigravityEnabled", "claude-code": "claudeCodeEnabled",
            "codex-cli": "codexEnabled", "github-copilot": "copilotEnabled", "cursor": "cursorEnabled",
            "windsurf": "windsurfEnabled", "jetbrains-ai": "jetbrainsAiEnabled"};
        var tools = SubscriptionPlanCatalog.tools();
        for (var t = 0; t < tools.length; ++t) {
            var tool = tools[t];
            rows.push({id: tool.key, label: tool.label
                + (!Plasmoid.configuration[enabledKeys[tool.key]] ? i18n(" (disabled)") : "")});
        }
        if (cfg_panelSourceId && !rows.some(function(row) { return row.id === generalPage.cfg_panelSourceId; }))
            rows.push({id: cfg_panelSourceId, label: i18n("Saved source (unavailable)")});
        return rows;
    }

    function refreshValue(refreshConfigKey) {
        return generalPage["cfg_" + refreshConfigKey];
    }

    function setRefreshValue(refreshConfigKey, value) {
        generalPage["cfg_" + refreshConfigKey] = value;
    }

    function formatInterval(secs) {
        if (secs >= 60) {
            var mins = Math.floor(secs / 60);
            return i18np("%1 minute", "%1 minutes", mins);
        }
        return i18np("%1 second", "%1 seconds", secs);
    }

    Components.SettingsFormLayout {

        ColumnLayout {
            Kirigami.FormData.label: i18n("Default refresh interval:")
            spacing: Kirigami.Units.smallSpacing

            QQC2.Slider {
                id: refreshSlider
                Layout.fillWidth: true
                from: 60
                to: 1800
                stepSize: 60
                value: Plasmoid.configuration.refreshInterval
                QQC2.ToolTip.text: i18n("How often to poll provider APIs for updated data (60s–30min)")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: 500
            }

            QQC2.Label {
                text: generalPage.formatInterval(refreshSlider.value)
                color: Kirigami.Theme.disabledTextColor
                Layout.alignment: Qt.AlignHCenter
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Panel Display")
        }

        QQC2.ComboBox {
            id: compactModeCombo
            Kirigami.FormData.label: i18n("Show in panel:")
            model: [
                i18n("Icon only"),
                i18n("Highest-priority source"),
                i18n("Lowest live quota"),
                i18n("Next live reset"),
                i18n("Actual provider spend"),
                i18n("Active sources"),
                i18n("Local tokens today"),
                i18n("Local sessions today")
            ]
            QQC2.ToolTip.text: i18n("Choose what to display next to the icon in the system panel")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: 500
            currentIndex: {
                switch (generalPage.cfg_compactDisplayMode) {
                case "critical":
                case "attention": return 1;
                case "lowest-quota": return 2;
                case "next-reset": return 3;
                case "cost":
                case "actual-spend": return 4;
                case "count":
                case "active-sources": return 5;
                case "session-tokens": return 6;
                case "session-count": return 7;
                default: return 0;
                }
            }
            onCurrentIndexChanged: {
                switch (currentIndex) {
                case 1: generalPage.cfg_compactDisplayMode = "attention"; break;
                case 2: generalPage.cfg_compactDisplayMode = "lowest-quota"; break;
                case 3: generalPage.cfg_compactDisplayMode = "next-reset"; break;
                case 4: generalPage.cfg_compactDisplayMode = "actual-spend"; break;
                case 5: generalPage.cfg_compactDisplayMode = "active-sources"; break;
                case 6: generalPage.cfg_compactDisplayMode = "session-tokens"; break;
                case 7: generalPage.cfg_compactDisplayMode = "session-count"; break;
                default: generalPage.cfg_compactDisplayMode = "icon"; break;
                }
            }
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("Panel sources:")
            model: [i18n("Automatic (favorites, otherwise all)"), i18n("All sources"), i18n("One source")]
            currentIndex: ["automatic", "all", "source"].indexOf(generalPage.cfg_panelSourceScope)
            onActivated: function(index) { generalPage.cfg_panelSourceScope = ["automatic", "all", "source"][index]; }
        }
        QQC2.ComboBox {
            objectName: "panelSourceSelector"
            Kirigami.FormData.label: i18n("Source:")
            visible: generalPage.cfg_panelSourceScope === "source"
            model: generalPage.panelSourceChoices
            textRole: "label"
            valueRole: "id"
            currentIndex: generalPage.panelSourceChoices.findIndex(function(row) {
                return row.id === generalPage.cfg_panelSourceId;
            })
            onActivated: function(index) { generalPage.cfg_panelSourceId = generalPage.panelSourceChoices[index].id; }
            Accessible.name: i18n("Panel source")
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18n("Automatic uses your favorites when configured. Disabled or unavailable favorites remain unavailable; global warnings stay visible.")
            color: Kirigami.Theme.disabledTextColor
        }
        QQC2.Switch {
            Kirigami.FormData.label: i18n("Advanced settings:")
            checked: generalPage.cfg_advancedSettingsMode
            text: i18n("Show per-source scheduling controls")
            Accessible.name: i18n("Show advanced scheduling settings")
            onToggled: generalPage.cfg_advancedSettingsMode = checked
        }

        Kirigami.Separator {
            visible: generalPage.cfg_advancedSettingsMode
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Advanced scheduling")
        }

        QQC2.Label {
            visible: generalPage.cfg_advancedSettingsMode
            text: generalPage.enabledProviders.length > 0
                ? i18n("Only enabled sources are shown. Set an interval to 0 to use the default above.")
                : i18n("Enable a provider source to configure its individual refresh interval.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Repeater {
            model: generalPage.cfg_advancedSettingsMode ? generalPage.enabledProviders : []

            ColumnLayout {
                id: providerSchedule
                required property var modelData
                spacing: 2
                Kirigami.FormData.label: providerSchedule.modelData.label + ":"

                QQC2.Slider {
                    id: providerRefreshSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 1800
                    stepSize: 60
                    value: generalPage.refreshValue(
                        providerSchedule.modelData.refreshConfigKey)
                    Accessible.name: i18n(
                        "Refresh interval for %1",
                        providerSchedule.modelData.label)
                    onValueChanged: generalPage.setRefreshValue(
                        providerSchedule.modelData.refreshConfigKey, value)
                }

                QQC2.Label {
                    text: providerRefreshSlider.value === 0
                        ? i18n("Use default")
                        : generalPage.formatInterval(providerRefreshSlider.value)
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.disabledTextColor
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("About")
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Icon:")

            Kirigami.Icon {
                source: Qt.resolvedUrl("../icons/logo.png")
                Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
            }

            QQC2.Label {
                text: i18n("AI Usage Monitor")
                opacity: 0.8
            }
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Version:")
            // Plasma's KPluginMetaData value type is absent from its installed qmltypes.
            // qmllint disable unresolved-type
            text: (Plasmoid["metaData"]
                   && Plasmoid["metaData"]["version"])
                  ? Plasmoid["metaData"]["version"]
                  : AppInfo.version
            // qmllint enable unresolved-type
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Description:")
            text: i18n("Monitor AI API token usage, rate limits, costs, and budgets across multiple providers")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
