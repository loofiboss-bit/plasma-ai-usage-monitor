# Installation

The Fedora COPR package includes the Plasma widget and compiled Qt plugin.

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

The KDE Store plasmoid is frontend-only and needs a matching native plugin from COPR or a source build.

Source build instructions, version checks, and mixed-installation guidance live in the [canonical installation guide](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/blob/main/docs/user-guide/installation.md).

Next: [Getting started](Getting-Started)
