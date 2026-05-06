import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0

Item {
    id: setupWizardRoot

    property int currentStep: 0
    // 0: Welcome
    // 1: Choose Path (Recommended / Custom)
    // 2: Provider Setup (OpenAI/Anthropic/Google/OpenRouter/Bedrock)
    // 3: Subscription Setup (Claude Code/Codex CLI/Copilot)
    // 4: Completion

    property string setupPath: "recommended" // "recommended" or "custom"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        PlasmaExtras.Heading {
            level: 3
            text: i18n("First-Run Setup")
            Layout.alignment: Qt.AlignHCenter
        }

        // --- Step 0: Welcome ---
        ColumnLayout {
            visible: currentStep === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Kirigami.Units.largeSpacing

            PlasmaComponents.Label {
                text: i18n("Welcome to AI Usage Monitor v6.0.1. Let's get your integrations set up.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Item { Layout.fillHeight: true }
        }

        // --- Step 1: Choose Path ---
        ColumnLayout {
            visible: currentStep === 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Kirigami.Units.largeSpacing

            PlasmaComponents.Label {
                text: i18n("How would you like to set up?")
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            QQC2.ButtonGroup { id: pathGroup }

            QQC2.RadioButton {
                text: i18n("Recommended (Top Providers & Subscriptions)")
                checked: true
                QQC2.ButtonGroup.group: pathGroup
                onCheckedChanged: if (checked) setupPath = "recommended"
            }

            QQC2.RadioButton {
                text: i18n("Custom (I know what I'm doing)")
                QQC2.ButtonGroup.group: pathGroup
                onCheckedChanged: if (checked) setupPath = "custom"
            }

            Item { Layout.fillHeight: true }
        }

        // --- Step 2: Provider Setup ---
        ColumnLayout {
            visible: currentStep === 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Kirigami.Units.smallSpacing

            PlasmaExtras.Heading { level: 4; text: i18n("Core Providers"); }

            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    width: parent.width - 20
                    spacing: Kirigami.Units.largeSpacing

                    // OpenAI
                    ColumnLayout {
                        QQC2.Switch {
                            id: openaiSwitch
                            text: "OpenAI"
                            checked: plasmoid.configuration.openaiEnabled
                            onCheckedChanged: plasmoid.configuration.openaiEnabled = checked
                        }
                        QQC2.TextField {
                            visible: openaiSwitch.checked
                            placeholderText: "sk-admin-..."
                            echoMode: TextInput.Password
                            Layout.fillWidth: true
                            onTextChanged: {
                                if (text.length > 0) secrets.storeKey("openai", text);
                            }
                        }
                        QQC2.Button {
                            visible: openaiSwitch.checked
                            text: i18n("Test Connection")
                            onClicked: {
                                openaiBackend.testConnection();
                            }
                        }
                    }

                    // Anthropic
                    ColumnLayout {
                        QQC2.Switch {
                            id: anthropicSwitch
                            text: "Anthropic"
                            checked: plasmoid.configuration.anthropicEnabled
                            onCheckedChanged: plasmoid.configuration.anthropicEnabled = checked
                        }
                        QQC2.TextField {
                            visible: anthropicSwitch.checked
                            placeholderText: "sk-ant-..."
                            echoMode: TextInput.Password
                            Layout.fillWidth: true
                            onTextChanged: {
                                if (text.length > 0) secrets.storeKey("anthropic", text);
                            }
                        }
                    }

                    // Google Gemini
                    ColumnLayout {
                        QQC2.Switch {
                            id: googleSwitch
                            text: "Google Gemini"
                            checked: plasmoid.configuration.googleEnabled
                            onCheckedChanged: plasmoid.configuration.googleEnabled = checked
                        }
                        QQC2.TextField {
                            visible: googleSwitch.checked
                            placeholderText: "AIza..."
                            echoMode: TextInput.Password
                            Layout.fillWidth: true
                            onTextChanged: {
                                if (text.length > 0) secrets.storeKey("google", text);
                            }
                        }
                    }
                }
            }
        }

        // --- Step 3: Subscription Setup ---
        ColumnLayout {
            visible: currentStep === 3
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Kirigami.Units.smallSpacing

            PlasmaExtras.Heading { level: 4; text: i18n("Coding Tools"); }

            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    width: parent.width - 20
                    spacing: Kirigami.Units.largeSpacing

                    QQC2.Switch {
                        text: "Claude Code"
                        checked: plasmoid.configuration.claudeCodeEnabled
                        onCheckedChanged: plasmoid.configuration.claudeCodeEnabled = checked
                    }
                    QQC2.Switch {
                        text: "GitHub Copilot"
                        checked: plasmoid.configuration.copilotEnabled
                        onCheckedChanged: plasmoid.configuration.copilotEnabled = checked
                    }
                    QQC2.Switch {
                        text: "Codex CLI"
                        checked: plasmoid.configuration.codexEnabled
                        onCheckedChanged: plasmoid.configuration.codexEnabled = checked
                    }
                }
            }
        }

        // --- Step 4: Completion ---
        ColumnLayout {
            visible: currentStep === 4
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Kirigami.Units.largeSpacing

            PlasmaComponents.Label {
                text: i18n("Setup Complete!")
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            PlasmaComponents.Label {
                text: i18n("You can always change these settings later in Configure > Providers.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Item { Layout.fillHeight: true }
        }

        // --- Navigation Bar ---
        RowLayout {
            Layout.fillWidth: true

            PlasmaComponents.Button {
                text: i18n("Skip Setup")
                visible: currentStep < 4
                onClicked: {
                    plasmoid.configuration.setupWizardDismissed = true;
                    plasmoid.configuration.setupWizardCompleted = true;
                }
            }

            Item { Layout.fillWidth: true }

            PlasmaComponents.Button {
                text: i18n("Back")
                visible: currentStep > 0 && currentStep < 4
                onClicked: currentStep--
            }

            PlasmaComponents.Button {
                text: currentStep === 4 ? i18n("Finish") : i18n("Next")
                onClicked: {
                    if (currentStep === 1 && setupPath === "custom") {
                        // Skip to completion, open standard config
                        plasmoid.configuration.setupWizardCompleted = true;
                        plasmoid.internalAction("configure").trigger();
                    } else if (currentStep < 4) {
                        currentStep++;
                    } else {
                        plasmoid.configuration.setupWizardCompleted = true;
                        plasmoid.configuration.setupWizardDismissed = false;
                        root.refreshAll();
                    }
                }
            }
        }
    }
}
