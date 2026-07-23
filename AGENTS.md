# Plasma AI Usage Monitor — Agent Instructions

> KDE Plasma 6 panel widget for monitoring AI API token usage, rate limits, and costs.
> C++20 | Qt6/KF6 | QML | CMake

## Build, Test Commands

```bash
# Preferred workflow (Justfile wraps the checked-in CMake presets)
just build
just build-debug
just test
just dev           # QML-only dev loop

# Build directly
cmake --preset debug -DCMAKE_INSTALL_PREFIX=/usr
cmake --build --preset debug

# Test
ctest --preset debug

# Install locally
cmake --install build/debug --prefix ~/.local

# Clean rebuild
cmake --fresh --preset debug && cmake --build --preset debug
```

- `CMakePresets.json` is the active build contract used by the Justfile.
- Use `just dev` for QML-only changes and `just install` / `just reload` when C++ plugin code changes.
- This repo uses CMake + ECM/KDEInstallDirs and checked-in presets; do not assume `vcpkg.json` exists.

## Project Layout

```text
plugin/
├── *.cpp, *.h                 # Native QML plugin, providers, history, secrets
└── tests/                     # Qt Test, Quick Test, and contract coverage
package/
├── contents/
│   ├── ui/                    # Plasma UI and configuration pages
│   ├── config/main.xml        # Stable KConfig contract
│   └── catalog/               # Provider and subscription catalogs
└── metadata.json              # KDE package metadata
docs/user-guide/               # Canonical editable user documentation
docs/wiki/                     # Generated wiki mirror
CMakeLists.txt                 # Build system
CMakePresets.json              # Canonical configure/build/test presets
```

## Code Style

- C++20 standard, Qt6/KF6 APIs
- QML: follow KDE Human Interface Guidelines
- Formatter: clang-format
- Linter: clang-tidy
- CMake: modern target-based API (`target_link_libraries`, `target_include_directories`)
- Use KDE Frameworks conventions for plugin structure

## Key Conventions

- Use `Q_PROPERTY` for QML-exposed properties
- Use signals/slots for async communication
- Follow KDE Plasma applet lifecycle (init, configChanged, etc.)
- Store settings via `Plasma::Applet::config()`
- Use `KLocalizedString` (i18n) for user-visible strings
- Keep `plasma_install_package(package com.github.loofi.aiusagemonitor)` intact when editing packaging/install rules
- Preserve `notifyrc` and AppStream metainfo installation rules in `CMakeLists.txt`

## Commits

Format: `type(scope): description`
Types: feat, fix, refactor, docs, test, chore, ci, perf, revert, style
Scope: kebab-case, max 100 chars subject.

Prefer repo-owned workflows from `README.md` and `Justfile` when choosing build or install commands.
