import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami

/**
 * A neutral KPI card for displaying the ratio of output to input tokens.
 */
Kirigami.Card {
    id: efficiencyCard
    
    property double efficiencyRatio: 1.0
    
    header: Kirigami.Heading {
        objectName: "ratioTitle"
        text: i18n("Output / Input Ratio")
        level: 4
    }
    
    contentItem: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing
        
        PlasmaComponents.Label {
            objectName: "ratioValue"
            text: efficiencyRatio.toFixed(2) + "x"
            font.pointSize: 24
            font.weight: Font.Bold
            color: Kirigami.Theme.textColor
        }
        
        PlasmaComponents.Label {
            objectName: "ratioDescription"
            text: i18n("Output tokens divided by input tokens")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        
    }
}
