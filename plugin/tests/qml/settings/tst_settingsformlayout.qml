import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtTest
import org.kde.kirigami as Kirigami
import "../../../../package/contents/ui/components" as Components

TestCase {
    id: testCase
    name: "SettingsFormLayout"

    Component {
        id: hostComponent

        Item {
            width: 1200
            height: 600

            property alias form: formLayout
            property alias description: descriptionLabel

            Components.SettingsFormLayout {
                id: formLayout

                QQC2.Label {
                    id: descriptionLabel
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "Long settings help text should wrap inside the readable form column instead of expanding beyond the screen."
                }
            }
        }
    }

    function test_wideWindowKeepsFormLeftAlignedAndReadable() {
        var host = createTemporaryObject(hostComponent, testCase);
        verify(host !== null);
        compare(host.form.x, 0);
        compare(host.form.width, Kirigami.Units.gridUnit * 32);
        verify(!host.form.wideMode);
        tryCompare(host.description.Layout, "maximumWidth", host.form.width);
        compare(host.description.Layout.preferredWidth, host.form.width);
        tryCompare(host.description, "width", host.form.width);
        verify(host.description.height > host.description.font.pixelSize * 1.5);
    }

    function test_narrowWindowUsesAllAvailableWidth() {
        var host = createTemporaryObject(hostComponent, testCase, { width: 420 });
        verify(host !== null);
        compare(host.form.x, 0);
        compare(host.form.width, 420);
        verify(!host.form.wideMode);
        tryCompare(host.description.Layout, "maximumWidth", host.form.width);
        tryCompare(host.description, "width", host.form.width);
    }

    function test_contentReflowsWhenWindowGetsNarrower() {
        var host = createTemporaryObject(hostComponent, testCase);
        verify(host !== null);
        tryCompare(host.description, "width", host.form.width);

        host.width = 420;
        tryCompare(host.form, "width", 420);
        tryCompare(host.description, "width", 420);
    }
}
