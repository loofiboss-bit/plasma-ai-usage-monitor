# Flatpak distribution decision

Flatpak is not a supported distribution route.

AI Usage Monitor needs a native Qt QML plugin in the host Plasma import path. The former frontend-only scaffold could package QML but could not provide or exercise that plugin, so it produced an incomplete installation.

Supported complete installations are:

- Fedora COPR, which ships the Plasma package and compiled plugin
- source build, which installs the same two parts

The KDE Store plasmoid is also frontend-only and must be paired with a matching compiled plugin.

Do not restore a Flatpak manifest until it can install the native module in a path Plasma loads, pass the QML import smoke test, and exercise the widget in a real Plasma session.
