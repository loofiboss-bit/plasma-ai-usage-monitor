# Contributing

AI Usage Monitor is a C++20 and QML Plasma 6 widget. Keep changes local-first, preserve unknown values, and do not add billable background requests.

## Set up a Fedora development environment

~~~bash
sudo dnf install cmake extra-cmake-modules gcc-c++ git python3 just ninja-build \
    qt6-qtbase qt6-qtbase-devel qt6-qtdeclarative-devel \
    libplasma-devel kf6-kwallet-devel kf6-ki18n-devel \
    kf6-knotifications-devel kf6-kcoreaddons-devel openssl-devel \
    protobuf-devel protobuf-compiler xorg-x11-server-Xvfb

git clone https://github.com/loofiboss-bit/plasma-ai-usage-monitor.git
cd plasma-ai-usage-monitor
just doctor
just build-debug
just test
just check
~~~

The underlying build uses plain CMake with ECM and KDEInstallDirs. The checked-in presets support the Justfile commands, while direct CMake builds remain available.

## Development loops

For QML-only changes:

~~~bash
just dev
just smoke
~~~

For C++ plugin changes:

~~~bash
just install
just reload
just smoke
~~~

Run the widget outside the panel with:

~~~bash
plasmawindowed com.github.loofi.aiusagemonitor
~~~

The deterministic demo environment is documented in [docs/demo/fedora-kde-vm.md](docs/demo/fedora-kde-vm.md).

## Repository layout

| Path | Responsibility |
| --- | --- |
| package/ | Plasma package, QML UI, settings, catalogs, icons, metadata |
| plugin/ | Native QML plugin, providers, history, secrets, local monitors |
| plugin/tests/ | CTest and Qt Test coverage |
| scripts/ | Validation, packaging, install, smoke, demo, and release tools |
| docs/user-guide/ | Canonical, editable end-user documentation |
| docs/wiki/ | Generated GitHub wiki mirror; never edit directly |
| docs/architecture/ | Current technical contracts |
| docs/release/ | Version-specific release evidence |

## Core contracts

Read [docs/architecture/reliability-core.md](docs/architecture/reliability-core.md) before changing providers, history, secrets, or refresh scheduling.

The rules that must survive a change are:

- scheduled provider calls are read-only
- missing values remain unknown
- actual billing, estimates, balances, published caps, and connectivity stay distinct
- mixed currencies are never silently summed
- secrets do not enter QML, config exports, logs, history, or diagnostics
- provider facts come from Catalog v5
- COPR/source installs include both the Plasma package and compiled plugin

## C++ style

- C++20 and Qt 6/KF6 APIs
- camelCase for methods and variables; PascalCase for classes
- Q_EMIT for signal emission
- QStringLiteral for fixed QString data
- Q_PROPERTY entries need a NOTIFY signal when the value can change
- effectiveBaseUrl() for provider endpoints that allow overrides
- typed ProviderBackend state instead of control flow based on error strings
- KLocalizedString for user-visible C++ text

Format touched C++ files with clang-format. Use clang-tidy through the repo-owned checks when the change affects native code.

## QML style

- Plasma 6 and Kirigami APIs
- KCM.SimpleKCM for settings pages
- Kirigami.Units and Kirigami.Theme for sizes and colors
- accessible names and roles for interactive or informational controls
- provider metadata from ProviderCatalog and ProviderRegistry
- no duplicated provider capability arrays or per-provider refresh timers
- budgets are stored as integer cents in KConfig

Run the QML checks before submitting:

~~~bash
just qml-lint
just smoke
~~~

After changing end-user documentation, regenerate the wiki mirror and verify
that no manually maintained copy has drifted:

~~~bash
python3 scripts/generate_wiki_docs.py
python3 scripts/generate_wiki_docs.py --check
~~~

## Add or change a provider

Provider identity, authentication slots, endpoints, safe refresh policy, capabilities, models, pricing sources, config keys, and expected metric sources belong in Catalog v5.

Use a descriptor adapter when the provider fits an existing contract. Add a custom ProviderBackend subclass only when the API needs custom request signing, account endpoints, or response parsing.

A provider change normally needs:

1. catalog descriptor and reviewed official source references
2. backend or descriptor binding
3. KConfig keys and provider settings fields
4. deterministic HTTP fixtures and parser tests
5. demo-contract coverage
6. regenerated provider capability documentation
7. user setup notes when the fields or limitations are not obvious

Regenerate and validate provider docs:

~~~bash
python3 scripts/generate_provider_capabilities.py
python3 scripts/check_provider_capability_docs.py
python3 scripts/check_demo_contract.py
~~~

Do not use a completion request as a scheduled connectivity check. Manual inference tests must be explicit and labeled as potentially billable.

## Validation

Choose the smallest relevant loop while developing, then run the full local gate before a pull request:

~~~bash
just test
just check
~~~

Release-affecting changes also require:

~~~bash
just release-check
just fedora44-check
~~~

The local release gate checks version consistency, catalogs, config portability, QML types and imports, package payload, non-invasive monitoring, deterministic provider contracts, AppStream, RPM policy, and source and plasmoid artifacts. The tag workflow creates checksums and an SPDX source SBOM.

## Commits and pull requests

Use the repository commit format:

~~~text
type(scope): description
~~~

Common types are feat, fix, refactor, docs, test, chore, ci, perf, revert, and style. Keep the scope in kebab-case and the subject under 100 characters.

In the pull request, state:

- what changed
- why it changed
- user or developer impact
- checks you ran
- screenshots for visible QML changes

Do not mix unrelated cleanup into a focused change.

## Report bugs

Open a [GitHub issue](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/issues) with the Plasma version, distribution, installed package version, reproduction steps, redacted Diagnostics support report, and the smallest relevant log excerpt.

Use a private [GitHub Security Advisory](https://github.com/loofiboss-bit/plasma-ai-usage-monitor/security/advisories/new) for vulnerabilities.

## License

Contributions are licensed under GPL-3.0-or-later.
