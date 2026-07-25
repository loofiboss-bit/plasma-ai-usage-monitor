pragma ComponentBehavior: Bound
import QtQuick
import org.kde.ki18n
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

Item {
    id: flow

    required property var runtime
    required property var readinessModel
    required property var secretStore
    required property var configuration
    property string previewState: ""
    property alias guidedController: controller

    function startAgain() { controller.startAgain(); }
    function resume() { controller.resume(); }

    GuidedSetupController {
        id: controller
        readinessModel: flow.readinessModel
        secretStore: flow.secretStore
        configuration: flow.configuration
        sourceApi: flow.runtime
        previewState: flow.previewState
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        radius: Kirigami.Units.cornerRadius
        color: Kirigami.Theme.backgroundColor
        border.width: 1
        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.2)

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.mediumSpacing

            RowLayout {
                Layout.fillWidth: true
                PlasmaExtras.Heading {
                    level: 3
                    text: KI18n.i18n("Guided first success")
                    Layout.fillWidth: true
                }
                PlasmaComponents.Label {
                    visible: controller.step <= controller.resultStep
                    opacity: 0.7
                    text: KI18n.i18n("Step %1 of 5", controller.step + 1)
                }
            }

            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                Loader {
                    width: parent.width
                    sourceComponent: controller.step === controller.goalStep ? goalComponent
                                   : controller.step === controller.sourceStep ? sourceComponent
                                   : controller.step === controller.configureStep ? configureComponent
                                   : controller.step === controller.verificationStep ? verificationComponent
                                   : controller.step === controller.resultStep ? resultComponent
                                   : pausedComponent
                }
            }

            PlasmaComponents.Button {
                visible: controller.step < controller.resultStep
                Layout.alignment: Qt.AlignHCenter
                flat: true
                text: KI18n.i18n("Skip for now")
                onClicked: controller.skip()
            }
        }
    }

    Component {
        id: goalComponent
        SetupGoalStep { controller: flow.guidedController }
    }
    Component {
        id: sourceComponent
        SetupSourceStep { controller: flow.guidedController }
    }
    Component {
        id: configureComponent
        SetupConfigureStep { controller: flow.guidedController }
    }
    Component {
        id: verificationComponent
        SetupVerificationStep { controller: flow.guidedController }
    }
    Component {
        id: resultComponent
        SetupResultStep { controller: flow.guidedController }
    }
    Component {
        id: pausedComponent
        ColumnLayout {
            spacing: Kirigami.Units.mediumSpacing
            PlasmaExtras.Heading { level: 4; text: KI18n.i18n("Setup is paused") }
            PlasmaComponents.Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: KI18n.i18n("No source was marked as successfully configured. Resume here at any time, or use Settings for full control.")
            }
            RowLayout {
                PlasmaComponents.Button {
                    text: KI18n.i18n("Resume setup")
                    icon.name: "media-playback-start"
                    onClicked: controller.resume()
                }
                PlasmaComponents.Button {
                    text: KI18n.i18n("Open Settings")
                    onClicked: Plasmoid.internalAction("configure").trigger()
                }
            }
        }
    }
}
