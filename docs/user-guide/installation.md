# Install both required parts

AI Usage Monitor needs a Plasma frontend and a matching compiled Qt plugin. The Fedora COPR and source install include both. The KDE Store package contains the frontend only.

## Fedora COPR

Install the supported package:

~~~bash
sudo dnf copr enable loofitheboss/plasma-ai-usage-monitor
sudo dnf install plasma-ai-usage-monitor
~~~

Log out and back in after the first install. Add **AI Usage Monitor** from Plasma's widget picker.

Update with:

~~~bash
sudo dnf upgrade plasma-ai-usage-monitor
~~~

Check the installed version:

~~~bash
rpm -q plasma-ai-usage-monitor
~~~

Remove the package and COPR configuration with:

~~~bash
sudo dnf remove plasma-ai-usage-monitor
sudo dnf copr remove loofitheboss/plasma-ai-usage-monitor
~~~

Removing the package does not delete your local KWallet entries or history database.

## KDE Store package

Install the native plugin from Fedora COPR or a source build before installing the KDE Store plasmoid. Keep the Store frontend and native plugin on the same version.

If the plugin is missing or mismatched, the widget opens a recovery screen instead of a blank popup. It shows both detected versions, a copyable COPR command, a source-install link, and a redacted bootstrap report. Install or update the matching package, then restart Plasma or log out and back in.

## Guided source install

Use this route on Plasma 6 systems where COPR is unavailable:

~~~bash
git clone https://github.com/loofiboss-bit/plasma-ai-usage-monitor.git
cd plasma-ai-usage-monitor
./scripts/install_bootstrap.sh
~~~

On Fedora, the installer can add missing build dependencies:

~~~bash
./scripts/install_bootstrap.sh --method source --install-missing
~~~

The build requires CMake, a C++20 compiler, Qt 6, Plasma 6 development files, Extra CMake Modules, KWallet, KI18n, KNotifications, OpenSSL, Protobuf compiler/development files, and SQLite support from Qt.

## Manual source build

~~~bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
sudo cmake --install build
~~~

Restart Plasma or log out and back in:

~~~bash
./scripts/reload_plasma.sh
~~~

## Check for mixed versions

A user-local widget can override the system package. If the panel shows an old version or the plugin fails to load:

~~~bash
./scripts/show_installed_versions.sh
./scripts/smoke_test_plasmoid.sh
~~~

The two versions must match. Native Diagnostics also shows the frontend layer, plugin layer, loaded plugin path, and a repair command when it can identify one. Continue with [Troubleshooting](troubleshooting.md) if the layers or versions do not agree.
