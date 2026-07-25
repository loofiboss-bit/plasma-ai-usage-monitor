pragma ComponentBehavior: Bound

import QtQuick
import org.kde.ki18n
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: step
    required property var controller
    spacing: Kirigami.Units.mediumSpacing

    PlasmaExtras.Heading {
        level: 4
        text: KI18n.i18n("What do you want to track first?")
    }
    PlasmaComponents.Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: KI18n.i18n("Choose one useful source. You can add everything else later in Settings.")
    }
    PlasmaComponents.Label {
        visible: step.controller.recommendedSource.stableId !== undefined
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.bold: true
        text: KI18n.i18n("Recommended: %1 — %2",
                   step.controller.recommendedSource.displayName || "",
                   step.controller.monitoringLevelLabel(step.controller.recommendedSource))
    }

    Repeater {
        model: [
            { key: "local", title: KI18n.i18n("A coding tool on this computer"), detail: KI18n.i18n("Start with detected Claude Code, Codex CLI, Copilot, Cursor, Windsurf, or JetBrains AI activity.") },
            { key: "usage", title: KI18n.i18n("API usage, spend, or balance"), detail: KI18n.i18n("Prioritize providers that expose actual reporting data instead of connectivity alone.") },
            { key: "provider", title: KI18n.i18n("A gateway or API provider"), detail: KI18n.i18n("Connect LiteLLM or verify a provider endpoint with a safe read-only request.") }
        ]

        PlasmaComponents.Button {
            id: goalButton
            required property var modelData
            Layout.fillWidth: true
            text: goalButton.modelData.title
            Accessible.description: goalButton.modelData.detail
            onClicked: step.controller.chooseGoal(goalButton.modelData.key)

            PlasmaComponents.ToolTip { text: goalButton.modelData.detail }
        }
    }
}
