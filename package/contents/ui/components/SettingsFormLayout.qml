import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.FormLayout {
    id: root

    readonly property int readableMaximumWidth: Kirigami.Units.gridUnit * 32

    anchors.left: parent.left
    anchors.top: parent.top
    width: Math.min(parent.width, readableMaximumWidth)
    wideMode: false

    function constrainChildWidths() {
        // Kirigami.FormLayout stores its GridLayout and helper Item first.
        for (let index = 2; index < root.children.length; ++index) {
            const item = root.children[index];
            if (!item || !item.Layout) continue;
            const currentMaximum = Number(item.Layout.maximumWidth);
            if (!Number.isFinite(currentMaximum) || currentMaximum <= 0
                    || currentMaximum > root.width) {
                const declaredMaximum = Number.isFinite(currentMaximum)
                    && currentMaximum > 0
                    ? currentMaximum : root.readableMaximumWidth;
                item.Layout.maximumWidth = Qt.binding(function() {
                    return Math.min(declaredMaximum, root.width,
                                    root.readableMaximumWidth);
                });
            }

            if (item.Layout.fillWidth) {
                const currentPreferred = Number(item.Layout.preferredWidth);
                const naturalPreferred = Number.isFinite(currentPreferred)
                    && currentPreferred > 0
                    ? currentPreferred : Math.ceil(item.implicitWidth);
                if (naturalPreferred > root.width) {
                    item.Layout.preferredWidth = Qt.binding(function() {
                        return Math.min(naturalPreferred, root.width,
                                        root.readableMaximumWidth);
                    });
                }

                item.width = Qt.binding(function() {
                    return Math.min(root.width, root.readableMaximumWidth);
                });
            }
        }
    }

    Connections {
        target: root
        function onChildrenChanged() {
            constrainWidthsTimer.restart();
        }
        function onWidthChanged() {
            constrainWidthsTimer.restart();
        }
    }

    Timer {
        id: constrainWidthsTimer
        interval: 0
        repeat: false
        running: true
        onTriggered: root.constrainChildWidths()
    }
}
