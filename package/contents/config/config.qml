import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18n("General")
        icon: "configure"
        source: "configGeneral.qml"
    }
    ConfigCategory {
        name: i18n("Providers")
        icon: "network-connect"
        source: "configProviders.qml"
    }
    ConfigCategory {
        name: i18n("Alerts")
        icon: "dialog-warning"
        source: "configAlerts.qml"
    }
    ConfigCategory {
        name: i18n("Guardrails")
        icon: "security-high"
        source: "configBudget.qml"
    }
    ConfigCategory {
        name: i18n("Subscriptions")
        icon: "view-task"
        source: "configSubscriptions.qml"
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
