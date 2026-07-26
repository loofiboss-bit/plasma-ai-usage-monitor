pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import "Utils.js" as Utils

KCM.SimpleKCM {
    id: budgetPage

    // Config stores cents (Int). SpinBox value is also cents.
    property int cfg_openaiDailyBudget
    property int cfg_openaiMonthlyBudget
    property int cfg_anthropicDailyBudget
    property int cfg_anthropicMonthlyBudget
    property int cfg_googleDailyBudget
    property int cfg_googleMonthlyBudget
    property int cfg_mistralDailyBudget
    property int cfg_mistralMonthlyBudget
    property int cfg_deepseekDailyBudget
    property int cfg_deepseekMonthlyBudget
    property int cfg_groqDailyBudget
    property int cfg_groqMonthlyBudget
    property int cfg_xaiDailyBudget
    property int cfg_xaiMonthlyBudget
    property int cfg_ollamaDailyBudget
    property int cfg_ollamaMonthlyBudget
    property int cfg_openrouterDailyBudget
    property int cfg_openrouterMonthlyBudget
    property int cfg_togetherDailyBudget
    property int cfg_togetherMonthlyBudget
    property int cfg_cohereDailyBudget
    property int cfg_cohereMonthlyBudget
    property int cfg_azureDailyBudget
    property int cfg_azureMonthlyBudget
    property int cfg_bedrockDailyBudget
    property int cfg_bedrockMonthlyBudget
    property int cfg_googleveoDailyBudget
    property int cfg_googleveoMonthlyBudget
    property int cfg_litellmDailyBudget
    property int cfg_litellmMonthlyBudget
    property alias cfg_budgetWarningPercent: warningPercentSlider.value

    property ProviderCatalog providerCatalog: ProviderCatalog {}

    // Shared formatting functions
    function centsToText(value) {
        return Utils.formatMoney(value / 100, "USD");
    }

    function textToCents(text) {
        var numeric = text.replace(/[^0-9,.-]/g, "");
        var val = Number.fromLocaleString(Qt.locale(), numeric);
        return isNaN(val) ? 0 : Math.round(val * 100);
    }

    Kirigami.FormLayout {
        anchors.fill: parent

        QQC2.Label {
            text: i18n("Budgets are stored in USD. Set a value to zero to disable it. A budget is automatically disabled when observed data uses another currency.")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Warning Threshold")
        }

        ColumnLayout {
            Kirigami.FormData.label: i18n("Warn at:")
            spacing: Kirigami.Units.smallSpacing

            QQC2.Slider {
                id: warningPercentSlider
                Layout.fillWidth: true
                from: 50
                to: 100
                stepSize: 5
                QQC2.ToolTip.text: i18n("Trigger a desktop notification when spending reaches this percentage of the budget")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: 500
            }

            QQC2.Label {
                text: i18n("%1% of budget", warningPercentSlider.value)
                color: Kirigami.Theme.disabledTextColor
                Layout.alignment: Qt.AlignHCenter
            }
        }

        // ── Per-provider budget sections (data-driven) ──
        Repeater {
            model: budgetPage.providerCatalog.budgetProviders

            ColumnLayout {
                id: budgetRow
                required property var modelData
                spacing: 0
                Layout.fillWidth: true

                Kirigami.Separator {
                    Kirigami.FormData.isSection: true
                    Kirigami.FormData.label: budgetRow.modelData.label
                    Layout.fillWidth: true
                }

                QQC2.SpinBox {
                    id: dailyField
                    Kirigami.FormData.label: i18n("Daily budget (USD):")
                    from: 0; to: 100000; stepSize: 100
                    value: budgetPage["cfg_" + budgetRow.modelData.dailyBudgetConfigKey]

                    textFromValue: function(value, locale) {
                        return budgetPage.centsToText(value);
                    }
                    valueFromText: function(text, locale) {
                        return budgetPage.textToCents(text);
                    }

                    onValueModified: {
                        budgetPage["cfg_" + budgetRow.modelData.dailyBudgetConfigKey] = value;
                    }

                    Component.onCompleted: {
                        value = budgetPage["cfg_" + budgetRow.modelData.dailyBudgetConfigKey];
                    }
                }

                QQC2.SpinBox {
                    id: monthlyField
                    Kirigami.FormData.label: i18n("Monthly budget (USD):")
                    from: 0; to: 1000000; stepSize: 500
                    value: budgetPage["cfg_" + budgetRow.modelData.monthlyBudgetConfigKey]

                    textFromValue: function(value, locale) {
                        return budgetPage.centsToText(value);
                    }
                    valueFromText: function(text, locale) {
                        return budgetPage.textToCents(text);
                    }

                    onValueModified: {
                        budgetPage["cfg_" + budgetRow.modelData.monthlyBudgetConfigKey] = value;
                    }

                    Component.onCompleted: {
                        value = budgetPage["cfg_" + budgetRow.modelData.monthlyBudgetConfigKey];
                    }
                }
            }
        }
    }
}
