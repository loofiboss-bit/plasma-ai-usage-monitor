import QtQuick
import org.kde.plasma.configuration
import org.kde.plasma.plasmoid

ConfigModel {
    ConfigCategory {
        name: Plasmoid.configuration.budgetPolicySelectionRequest
            ? i18n("Budget Control") : i18n("Sources")
        icon: Plasmoid.configuration.budgetPolicySelectionRequest
            ? "security-high" : "network-connect"
        source: Plasmoid.configuration.budgetPolicySelectionRequest
            ? "configBudget.qml" : "configProviders.qml"
    }
    ConfigCategory {
        name: i18n("General")
        icon: "configure"
        source: "configGeneral.qml"
        visible: !Plasmoid.configuration.budgetPolicySelectionRequest
    }
    ConfigCategory {
        name: i18n("Budget Control")
        icon: "security-high"
        source: "configBudget.qml"
        visible: !Plasmoid.configuration.budgetPolicySelectionRequest
    }
    ConfigCategory {
        name: i18n("Alerts")
        icon: "dialog-warning"
        source: "configAlerts.qml"
    }
    ConfigCategory {
        name: i18n("History")
        icon: "office-chart-line"
        source: "configHistory.qml"
    }
    ConfigCategory {
        name: i18n("Diagnostics")
        icon: "tools-report-bug"
        source: "configDiagnostics.qml"
    }
}
