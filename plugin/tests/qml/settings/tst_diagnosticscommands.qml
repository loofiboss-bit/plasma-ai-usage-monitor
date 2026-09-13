import QtQuick
import QtTest
import "../../../../package/contents/ui/DiagnosticsCommands.js" as DiagnosticsCommands

TestCase {
    name: "DiagnosticsCommands"

    function test_versionCheckCommand_data() {
        return [
            {
                tag: "fedora",
                productType: "fedora",
                expected: "plasmashell --version; rpm -q plasma-ai-usage-monitor"
            },
            {
                tag: "generic-linux",
                productType: "linux",
                expected: "plasmashell --version"
            },
            {
                tag: "freebsd",
                productType: "freebsd",
                expected: "plasmashell --version"
            },
            {
                tag: "unknown",
                productType: "",
                expected: "plasmashell --version"
            }
        ];
    }

    function test_versionCheckCommand(data) {
        compare(DiagnosticsCommands.versionCheckCommand(data.productType), data.expected);
    }
}
