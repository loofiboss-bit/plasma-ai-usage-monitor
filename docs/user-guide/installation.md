# Install both required parts

AI Usage Monitor needs a Plasma frontend and a matching compiled Qt plugin. The Fedora COPR and source install include both. The KDE Store package contains the frontend only.

## Choose an installation route

| Your system | Recommended route | Included |
| --- | --- | --- |
| Fedora with KDE Plasma 6 | [COPR package](#fedora-copr-fedora-only) | Widget and native plugin |
| Another KDE Plasma 6 distribution | [Source installation](#guided-source-install) | Widget and native plugin |
| KDE Store / Get New Widgets | [Store frontend](#kde-store-package), after the matching native plugin is installed | Widget only |

The widget requires KDE Plasma 6. Plasma 5 and the legacy Flatpak scaffold are
not supported. An API key is only needed for a provider that requires one;
local coding-session history can be used on its own.

## Fedora COPR (Fedora only)

Check [available Fedora builds](https://copr.fedorainfracloud.org/coprs/loofitheboss/plasma-ai-usage-monitor/)
for your release. Fedora 44 is the release baseline. Install the complete package:

~~~bash
sudo dnf copr enable loofitheboss/plasma-ai-usage-monitor
sudo dnf install plasma-ai-usage-monitor
~~~

Log out and back in after the first install. Right-click the panel or desktop,
choose **Add Widgets**, search for **AI Usage Monitor**, and add it. Continue
with [first setup](getting-started.md).

Update with:

~~~bash
sudo dnf upgrade --refresh plasma-ai-usage-monitor
~~~

Log out and back in after updating so Plasma loads the new native plugin. In
**Settings → Diagnostics**, confirm that the frontend and native-plugin
versions match. V25.0.1 retains V24.2.0 and V25.0.0 settings, aliases, session opt-ins and history;
you do not need to delete data or reimport sessions.

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

The [KDE Store listing](https://store.kde.org/p/2353976) is also available through
Plasma's **Get New Widgets** catalog. Fedora COPR users already have the
frontend and do not need this extra installation.

Install the matching native plugin before installing the KDE Store plasmoid. On Fedora, use the COPR package above. On other supported Plasma 6 systems, use the source installation below. Keep the Store frontend and native plugin on the same version.

If the plugin is missing or mismatched, the widget opens a recovery screen instead of a blank popup. It shows both detected versions, a source-install link, and a redacted bootstrap report. Because the native plugin may be unavailable, this screen does not assume a distribution or package manager. Follow the source installation guide for the current platform, then restart Plasma or log out and back in.

## Guided source install

Use this route on supported Plasma 6 systems when you are not installing the Fedora COPR package:

~~~bash
git clone --branch v25.0.1 --depth 1 https://github.com/loofiboss-bit/plasma-ai-usage-monitor.git
cd plasma-ai-usage-monitor
./scripts/install_bootstrap.sh --method source
~~~

The tag selects the V25.0.1 release rather than an in-progress development branch.
The installer checks dependencies, builds both parts and asks before installing
system-wide to `/usr` with `sudo`. It also offers to reload Plasma.

On Fedora, the installer can add missing build dependencies:

~~~bash
./scripts/install_bootstrap.sh --method source --install-missing
~~~

The build requires CMake, a C++20 compiler, Qt 6, Plasma 6 development files, Extra CMake Modules, KWallet, KI18n, KNotifications, OpenSSL, Protobuf compiler/development files, and SQLite support from Qt.

## Manual source build

From the matching release checkout above, after installing the development
dependencies for your distribution:

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

The two versions must match. Native Diagnostics also shows the frontend layer, plugin layer, loaded plugin path, and recovery guidance. On Fedora it can offer a `dnf` repair command; other systems receive source-install guidance. Continue with [Troubleshooting](troubleshooting.md) if the layers or versions do not agree.

## Next step

[Configure your first source](getting-started.md), or go straight to
[local coding-session history](coding-sessions.md#try-it-with-one-tool) if you
only want to review Codex or Claude Code tokens.
