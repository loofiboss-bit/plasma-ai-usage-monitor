# Installation

AI Usage Monitor needs a Plasma frontend and matching compiled Qt plugin. The Fedora COPR package includes both.

~~~bash
sudo dnf copr enable loofitheboss/plasma-ai-usage-monitor
sudo dnf install plasma-ai-usage-monitor
~~~

Log out and back in. Add **AI Usage Monitor** from Plasma's widget picker.

Update with:

~~~bash
sudo dnf upgrade plasma-ai-usage-monitor
~~~

Remove it with:

~~~bash
sudo dnf remove plasma-ai-usage-monitor
sudo dnf copr remove loofitheboss/plasma-ai-usage-monitor
~~~

Before installing the KDE Store plasmoid, install the matching native plugin from COPR or a source build. The Store package is frontend-only.

A missing, older, or newer plugin opens an in-widget recovery screen. It shows both versions, a copyable COPR command, a source-install link, and a redacted report. Repair the installation, then restart Plasma or log out and back in.

Source build instructions, version checks, and mixed-installation guidance live in the [canonical installation guide](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/user-guide/installation.md).

Next: [Getting started](Getting-Started)
