# Plasma AI Usage Monitor v21 — Distribution, Onboarding & Community Growth Plan

| Field | Decision |
| --- | --- |
| Proposed release | 21.0.0 (or v20.2.0 milestone) |
| Review date | 2026-09-14 |
| Plan status | Approved for execution via design review |
| Baseline | `742afb8` — v20.1.0 (Quota Observability & Platform Flexibility) |
| Primary objective | Eliminate distribution friction (Arch AUR, universal installer, recovery UX) and accelerate adoption through auto-onboarding and KDE community outreach |

---

## 1. Executive Summary & Full Codebase Review

### 1.1 Architecture & Code Quality Audit
The `plasma-ai-usage-monitor` codebase demonstrates enterprise-grade engineering rarely seen in desktop widget ecosystems:
* **Scale & Tech Stack:** Over 61,000 lines of modern C++20, Qt 6, and KDE Frameworks 6 (Kirigami, KWallet, KNotifications, KCoreAddons, KI18n).
* **Integrity Ethos:** Guided by the foundational principle *"Protect daily truth"*. It strictly segregates verified provider quotas, local activity estimations, and unavailable metrics without ever synthesizing fake zero balances or inventing billing numbers.
* **Core Subsystems:**
  * **Cost Engine v2 & Catalog v7:** Minor-unit currency math with strict schema validation, offline JSON catalogs, and drift alerting.
  * **Budget Control v18:** Multi-scope, calendar-aware pacing policies with restart-safe transitions.
  * **Codex Local Auth Sync (v20.1.0):** Native token and quota window discovery directly from CLI credentials, bypassing brittle browser cookie scraping.
  * **Security & Privacy:** API keys locked in KWallet, loopback-default Prometheus scraping, strictly local SQLite storage, zero unconsented telemetry or cloud dependencies.
* **Test & Release Gates:** Over 30 unit and integration test suites, rigorous static checks (`check_version_policy`, `check_catalog_drift`, QML accessibility and localization audits).

### 1.2 Adoption Bottlenecks (Root Cause Analysis)
Despite technical excellence, the project has struggled to achieve widespread mainstream adoption. The code and distribution audit reveals four primary friction points:

1. **The Distribution Monopoly (Fedora COPR Only):**
   * While Fedora users enjoy automated RPM packaging via COPR, the KDE Plasma 6 user base is heavily concentrated on **Arch Linux, EndeavourOS, Manjaro, and KDE neon / Ubuntu**.
   * Arch Linux power users rely almost exclusively on the **AUR (`yay -S ...`)**. The lack of an official `PKGBUILD` excludes the most vocal and engaged KDE power users.
   * Debian/Ubuntu users lack prebuilt `.deb` releases or automated installation paths.
2. **The KDE Store "C++ Plugin Trap":**
   * The KDE "Get New Widgets" store only extracts QML plasmoids to user data directories (`~/.local/share/plasma/plasmoids/`). It cannot install the native compiled C++ module (`AIUsageMonitorPlugin.so`).
   * Users installing from the store encounter a missing-plugin recovery state. Although gracefully presented, directing users to manually clone git and run CMake from source creates excessive friction.
3. **Onboarding & First-Run Discovery Friction:**
   * AI developers installing the widget often already have tools installed locally (e.g. `codex`, `claude`, `antigravity`, `ollama`).
   * Current Guided Setup requires users to manually select and configure sources step-by-step, rather than automatically discovering installed tools and offering a zero-configuration one-click setup.
4. **Hero Presentation & Visual Appeal:**
   * The README is dense, technical, and specification-heavy. It lacks an animated hero GIF/demo demonstrating the live panel widget and fluid Kirigami flyout in action.

---

## 2. Strategic Roadmap: 2-Phase Execution

Based on the architectural review and design interview, the next development cycle is structured into two sequential phases:

```
┌─────────────────────────────────────────────────────────────┐
│ Phase 1: Distribution & Packaging (Remove Install Friction) │
├─────────────────────────────────────────────────────────────┤
│ 1. Arch Linux AUR Package (PKGBUILD + .SRCINFO + release sync)
│ 2. Universal One-Line Installer (`curl -fsSL ... | bash`)   │
│ 3. Interactive Recovery Screen in QML (One-click copy)      │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ Phase 2: User Onboarding, Visuals & Community Outreach      │
├─────────────────────────────────────────────────────────────┤
│ 4. Auto-Detection in Guided Setup ("Detected on your system")│
│ 5. Visual README Modernization (Hero GIF / Quickstart cards)│
│ 6. KDE Community Launch Kit (KDE Discuss, Store, r/kde)     │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Phase 1: Distribution & Packaging Specification

### 3.1 Arch Linux AUR Package (`PKGBUILD`)
* **Target Location:** `packaging/arch/PKGBUILD` and `packaging/arch/.SRCINFO`.
* **Package Name:** `plasma-ai-usage-monitor`.
* **Build Mechanism:** Builds from official GitHub release tarball (`v${pkgver}.tar.gz`).
* **Dependencies:**
  * `makedepends`: `cmake`, `extra-cmake-modules`, `gcc`, `git`, `protobuf`
  * `depends`: `plasma-workspace>=6.0`, `qt6-base`, `qt6-declarative`, `libplasma`, `kwallet`, `ki18n`, `knotifications`, `kcoreaddons`, `openssl`
* **Lifecycle Automation:**
  * Script `scripts/update_arch_pkgbuild.sh` to update version and compute SHA256 checksums from git tags.
  * Integration into `Justfile` under `just check-arch` and `just release-check`.

### 3.2 Universal One-Line Install Script (`scripts/quick_install.sh`)
* **Usage:**
  ```bash
  curl -fsSL https://raw.githubusercontent.com/loofiboss-bit/plasma-ai-usage-monitor/main/scripts/quick_install.sh | bash
  ```
* **Distro Detection & Execution Paths:**
  * **Fedora:** Prompts for sudo and runs:
    ```bash
    sudo dnf copr enable -y loofitheboss/plasma-ai-usage-monitor
    sudo dnf install -y plasma-ai-usage-monitor
    ```
  * **Arch Linux / Manjaro / EndeavourOS:** Checks for `yay` or `paru`, executing AUR installation; falls back to clean `makepkg -si` in `/tmp`.
  * **Debian / Ubuntu / KDE neon / openSUSE / Generic Linux:**
    1. Downloads release tarball to `/tmp/ai-usage-monitor-install`.
    2. Runs prerequisite diagnostic checks.
    3. Builds via CMake with `CMAKE_INSTALL_PREFIX=/usr`.
    4. Executes `sudo cmake --install`.
    5. Cleans up temporary artifacts.
* **Safety & Usability Flags:**
  * `--dry-run`: Prints planned actions without system modifications.
  * `--yes` / `-y`: Non-interactive mode.
  * Clean detection of stale user-local plasmoids shadowing system libraries.
  * Optional automatic `plasmashell` reload prompt.

### 3.3 Interactive Missing-Plugin Recovery UX
* **Target File:** `package/contents/ui/DependencyBootstrap.qml`.
* **Problem:** Users from KDE Store see a static warning and must hunt for build instructions.
* **Solution:** Replace static text with an interactive command switcher providing copy-to-clipboard buttons (leveraging `ClipboardHelper`):
  * Tab/Card 1 (Fedora): `sudo dnf copr enable loofitheboss/plasma-ai-usage-monitor && sudo dnf install plasma-ai-usage-monitor`
  * Tab/Card 2 (Arch Linux): `yay -S plasma-ai-usage-monitor`
  * Tab/Card 3 (Quick Installer): `curl -fsSL https://raw.githubusercontent.com/loofiboss-bit/plasma-ai-usage-monitor/main/scripts/quick_install.sh | bash`
* **Documentation:** Refresh `docs/store/README.md` with explicit, prominent installation notices at the very top.

---

## 4. Phase 2: Onboarding, Visual Showcase & KDE Community Outreach

### 4.1 Auto-Detection in "Guided First Success"
* **Target Files:**
  * `package/contents/ui/onboarding/GuidedSetupController.qml`
  * `package/contents/ui/onboarding/SetupSourceStep.qml`
* **Implementation:**
  * Interrogate `SubscriptionToolBackend::isInstalled` on registered local monitors (`CodexCliMonitor`, `ClaudeCodeMonitor`, `AntigravityMonitor`).
  * Add a dedicated, elevated section in `SetupSourceStep.qml`: **"Detected on your system"** with a visual accent badge.
  * Pre-select the detected tool to enable instant, friction-free activation upon initial launch.

### 4.2 Visual README Modernization
* **Target File:** `README.md`.
* **Enhancements:**
  * Add an animated hero visual (GIF/WebM or high-impact screenshot carousel) directly beneath the logo, demonstrating panel interaction, quota resets, and attention states.
  * Add concise "Feature Pills" / badges: `KDE Plasma 6 Native` | `C++20 & Qt 6` | `Zero Telemetry` | `KWallet Secured` | `Offline First`.
  * Add a Quick-Start Install Matrix (Fedora COPR, Arch AUR, Quick Script) above the fold.

### 4.3 KDE Community Launch Kit
* **Target File:** `docs/release/KDE_COMMUNITY_ANNOUNCEMENT.md`.
* **Deliverables:**
  * **KDE Discuss (`discuss.kde.org`):** High-quality community post focusing on KDE native integration, privacy, KWallet integration, and desktop workflow synergy.
  * **Reddit (`r/kde`):** Showcase post highlighting native Plasma 6 support, truth-in-monitoring, and zero-telemetry architecture.
  * **KDE Store Listing:** Optimized description text with clear installation prerequisites to eliminate 1-star reviews from missing C++ plugins.
  * **KDE Matrix:** Announcement snippets for `#kde:kde.org` and `#plasma:kde.org`.

---

## 5. Verification & Acceptance Criteria

1. **Packaging Quality:**
   * Arch `PKGBUILD` parses cleanly and conforms to Arch Packaging Standards.
   * `scripts/update_arch_pkgbuild.sh` accurately syncs version strings and sha256 checksums with `VERSION`.
2. **Installer Robustness:**
   * `scripts/quick_install.sh --dry-run` executes without errors on Fedora, Arch, and Debian containers.
   * Parameter validation (`-y`, `--dry-run`, `-h`) functions predictably.
3. **QML Integrity:**
   * `just qml-lint` passes without binding or scope warnings.
   * `DependencyBootstrap.qml` clipboard interactions test cleanly.
   * Guided Setup correctly displays detected local tools.
4. **CI & Policy Conformance:**
   * `just check` passes all existing test suites.
