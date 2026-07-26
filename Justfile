# Justfile — Plasma AI Usage Monitor
# Usage: just <recipe>  (requires https://github.com/casey/just)
# Install: cargo install just  OR  sudo dnf install just

# List all available recipes
default:
    @just --list

# ── Build & Test ──────────────────────────────────────────────────────────────

# Configure and build (Release mode)
build:
    cmake --preset release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build --preset release --parallel $(nproc)

# Configure and build (Debug mode, enables unit tests)
build-debug:
    cmake --preset debug -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build --preset debug --parallel $(nproc)

# Run unit tests
test: build-debug
    ctest --preset debug

# Lint all shipped QML with the built module import path
qml-lint:
    python3 scripts/qml_lint.py --build-dir build/debug

# Check release metadata against the canonical VERSION file
check:
    bash scripts/check_version_consistency.sh
    python3 scripts/check_version_policy.py
    python3 scripts/test_version_policy.py
    bash scripts/check_no_hardcoded_versions.sh
    python3 scripts/check_provider_catalog.py
    python3 scripts/check_config_defaults_exist_in_catalog.py
    python3 scripts/check_provider_ui_catalog_models.py
    python3 scripts/check_subscription_catalog.py
    python3 scripts/check_no_hardcoded_pricing.py
    python3 scripts/check_config_portability.py
    python3 scripts/check_kcm_contracts.py
    python3 scripts/check_daily_ui_accessibility.py
    python3 scripts/check_qml_localization.py
    python3 scripts/check_qml_registered_types.py
    python3 scripts/check_non_invasive_monitoring.py
    python3 scripts/check_provider_capability_docs.py
    python3 scripts/generate_wiki_docs.py --check
    python3 scripts/check_demo_contract.py
    python3 scripts/check_release_policy.py
    python3 scripts/check_release_media.py
    python3 scripts/test_release_media_evidence.py
    bash scripts/test_verify_exact_tag.sh

# Regenerate the provider capability matrix from Catalog v5
generate-provider-docs:
    python3 scripts/generate_provider_capabilities.py

# Regenerate the versioned GitHub wiki mirror from docs/user-guide
generate-wiki-docs:
    python3 scripts/generate_wiki_docs.py

# Validate install prerequisites (dependencies + runtime commands)
doctor:
    bash scripts/install_doctor.sh

# Validate and install missing Fedora dependencies automatically
doctor-fix:
    bash scripts/install_doctor.sh --install-missing

# Show installed versions: repo vs user-local vs system
versions:
    bash scripts/show_installed_versions.sh

# Smoke-check the active dev install and next-step hints
smoke:
    bash scripts/dev_smoke_check.sh

# Strict release validation gate
release-check: build-debug
    @echo "=== Running strict release checks ==="
    bash scripts/check_version_consistency.sh
    python3 scripts/check_version_policy.py
    python3 scripts/test_version_policy.py
    bash scripts/check_no_hardcoded_versions.sh
    python3 scripts/check_provider_catalog.py
    python3 scripts/check_config_defaults_exist_in_catalog.py
    python3 scripts/check_provider_ui_catalog_models.py
    python3 scripts/check_subscription_catalog.py
    python3 scripts/check_no_hardcoded_pricing.py
    python3 scripts/check_config_portability.py
    python3 scripts/check_kcm_contracts.py
    python3 scripts/check_daily_ui_accessibility.py
    python3 scripts/check_qml_localization.py
    python3 scripts/check_qml_registered_types.py
    python3 scripts/check_non_invasive_monitoring.py
    python3 scripts/check_provider_capability_docs.py
    python3 scripts/generate_wiki_docs.py --check
    python3 scripts/check_demo_contract.py
    python3 scripts/check_release_policy.py
    python3 scripts/check_release_media.py
    python3 scripts/test_release_media_evidence.py
    bash scripts/test_verify_exact_tag.sh
    PYTHONNOUSERSITE=1 python3 scripts/smoke_test_qml_import.py --strict --build-dir build/debug --expected-version "$(< VERSION)"
    @if command -v appstreamcli >/dev/null 2>&1; then appstreamcli validate --no-net com.github.loofi.aiusagemonitor.metainfo.xml; else echo "Warning: appstreamcli not found, skipping validation. Run 'sudo dnf install appstream' on Fedora."; exit 1; fi
    @if command -v rpmlint >/dev/null 2>&1; then rpmlint plasma-ai-usage-monitor.spec; else echo "Warning: rpmlint not found, skipping validation. Run 'sudo dnf install rpmlint' on Fedora."; exit 1; fi
    bash scripts/package_source_tarball.sh --check
    bash scripts/package_plasmoid.sh --check
    python3 scripts/check_package_payload.py

# Phase 7 database performance, async lifecycle, accessibility, and QML gates
phase7-check: build-debug
    ctest --test-dir build/debug --output-on-failure -R 'phase7_performance|phase7_qml|daily_ui_accessibility'
    cmake --preset release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build --preset release --parallel $(nproc) --target test_phase7_performance
    ctest --test-dir build/release --output-on-failure -R phase7_performance
    python3 scripts/check_daily_ui_accessibility.py
    python3 scripts/qml_lint.py --build-dir build/debug
    QT_QPA_PLATFORM=offscreen dbus-run-session -- bash scripts/smoke_test_plasmoid.sh build/debug

# Fedora KDE 44 release environment validation
fedora44-check:
    bash scripts/fedora44_check.sh

# Create a tarball package for distribution
package:
    bash scripts/package_source_tarball.sh

# Clean the build directory
clean:
    rm -rf build

# Safely clean stale user-local installs
clean-local:
    bash scripts/clean_local_installs.sh

# Safely clean stale user-local installs (dry run)
clean-local-dry-run:
    bash scripts/clean_local_installs.sh --dry-run

# ── System-Wide Install (requires sudo) ───────────────────────────────────────

# Build then install to /usr (requires sudo)
install: build
    sudo cmake --install build/release

# Reinstall: uninstall then install
reinstall: uninstall install

# Uninstall system-wide install using CMake's install manifest
uninstall:
    #!/usr/bin/env bash
    set -euo pipefail
    if [ -f build/release/install_manifest.txt ]; then
        sudo xargs rm -f < build/release/install_manifest.txt
        sudo ldconfig
        echo "Uninstalled. Removed files listed in build/install_manifest.txt"
    else
        echo "No build/release/install_manifest.txt found."
        echo "Run 'just install' first, or use 'just copr-remove' for COPR installs."
        exit 1
    fi

# Guided interactive uninstall: detects and removes user-local and/or system installs
uninstall-guided:
    bash uninstall.sh

# ── User-Local Install (no sudo, QML package only) ───────────────────────────

# Install QML package to ~/.local/share/plasma/plasmoids/ (no sudo needed)
install-user:
    bash scripts/install_local_plasmoid.sh

# Uninstall the user-local QML package
uninstall-user:
    kpackagetool6 --type Plasma/Applet --remove com.github.loofi.aiusagemonitor

# Restart plasmashell to pick up QML changes
reload:
    bash scripts/reload_plasma.sh

# One-step dev iteration: install user-local package + reload plasmashell
dev: install-user reload

# Guided bootstrap installer (auto picks COPR on Fedora, source elsewhere)
bootstrap:
    bash scripts/install_bootstrap.sh

# Guided source build/install with dependency auto-fix on Fedora
bootstrap-source:
    bash scripts/install_bootstrap.sh --method source --install-missing

# Bootstrap a Fedora KDE 44 guest for live testing and screenshots
demo-bootstrap:
    bash scripts/demo/setup_fedora_kde_test_env.sh --fedora 44

# Bootstrap the Fedora KDE 44 guest, install missing packages, and prepare the widget for testing
demo-bootstrap-install:
    bash scripts/demo/setup_fedora_kde_test_env.sh --fedora 44 --install-missing --prepare-widget

# Start the deterministic demo mock server from the Linux .venv
demo-server:
    .venv/bin/python scripts/demo/mock_ai_usage_server.py

# Generate deterministic v12 state and 1k/10k/100k observation fixtures
reliability-fixtures:
    python3 scripts/demo/generate_reliability_fixtures.py

# Guided COPR installation on Fedora
bootstrap-copr:
    bash scripts/install_bootstrap.sh --method copr

# User-local plasmoid-only install + reload (no system plugin install)
bootstrap-user:
    bash scripts/install_bootstrap.sh --method user

# ── COPR / DNF ────────────────────────────────────────────────────────────────

# Build an SRPM suitable for COPR submission into ./dist
copr-srpm:
    mkdir -p dist
    bash scripts/build_srpm.sh --output-dir dist

# Build and submit an SRPM to an existing COPR project
copr-submit TAG PROJECT="loofitheboss/plasma-ai-usage-monitor":
    mkdir -p dist
    bash scripts/copr_submit_build.sh --project "{{PROJECT}}" --tag "{{TAG}}" --output-dir dist

# Verify clean Fedora 44 install plus base upgrade, rollback, re-upgrade, reinstall, and removal
rpm-lifecycle-check BASE_RPM CANDIDATE_RPM:
    bash scripts/test_fedora44_rpm_lifecycle.sh "{{BASE_RPM}}" "{{CANDIDATE_RPM}}"

# Enable the COPR repository
copr-enable:
    sudo dnf copr enable loofitheboss/plasma-ai-usage-monitor

# Enable COPR and install the package
copr-install: copr-enable
    sudo dnf install plasma-ai-usage-monitor

# Upgrade the installed COPR package to the latest version
copr-update:
    sudo dnf upgrade plasma-ai-usage-monitor

# Remove the package and the COPR repository
copr-remove:
    sudo dnf remove plasma-ai-usage-monitor
    sudo dnf copr remove loofitheboss/plasma-ai-usage-monitor

# ── Version Management ────────────────────────────────────────────────────────

# Update all release surfaces from the canonical VERSION source
bump VERSION:
    bash scripts/bump_version.sh {{VERSION}}
