# ADR: Flatpak is not a v12 distribution route

Status: accepted for v12.0.0

The former manifest copied only the Plasma QML package and did not build or ship
the architecture-specific C++ QML plugin. Publishing it would therefore promise
an installation route that cannot produce the complete widget.

v12 removes that manifest from release validation and publication claims. A
future Flatpak experiment must first document a viable Plasma host-integration
model, build the plugin, install both payloads, and pass the same full plasmoid
smoke and upgrade tests as COPR/source packages. Until then, COPR is the primary
Fedora route and source builds are the supported portable route.
