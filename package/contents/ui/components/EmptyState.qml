import QtQuick
import org.kde.plasma.extras as PlasmaExtras

PlasmaExtras.PlaceholderMessage {
    required property string title
    required property string details
    text: title
    explanation: details
    iconName: "view-history"
}
