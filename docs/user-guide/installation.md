# Install and update

AI Usage Monitor needs two matching parts: the Plasma package and a compiled Qt plugin. The Fedora COPR and source install include both.

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

The build requires CMake, a C++20 compiler, Qt 6, Plasma 6 development files, Extra CMake Modules, KWallet, KI18n, KNotifications, OpenSSL, and SQLite support from Qt.

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

## KDE Store package

The KDE Store plasmoid contains QML, catalogs, icons, and metadata. It does not contain the architecture-specific compiled plugin. Install the matching plugin from COPR or a source build before using the Store package.

## Check for mixed versions

A user-local widget can override the system package. If the panel shows an old version or the plugin fails to load:

~~~bash
./scripts/show_installed_versions.sh
./scripts/smoke_test_plasmoid.sh
~~~

The two versions must match. Continue with [Troubleshooting](troubleshooting.md) if they do not.
