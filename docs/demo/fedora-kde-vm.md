## Fedora KDE Demo Environment

Use this workflow when you need a real Plasma session for live testing, polished screenshots, or release-candidate verification from a Windows workstation.

## Why a VM is the canonical path

- the widget is a Plasma 6 plasmoid, so screenshot quality depends on a real KDE desktop session
- a Windows-created Python virtual environment is not portable to Fedora Linux
- the demo workflow needs both the compiled QML plugin and the plasmoid package installed in a Linux environment

## Can VS Code run the VM?

Not directly in a portable repo-owned way. VS Code can **attach to** the Fedora KDE 44 guest and orchestrate the workflow, but the guest itself still needs to be started by Hyper-V, VMware, or VirtualBox.

Use the two modes below:

- **Remote SSH into the Fedora VM** — best for real Plasma testing, `plasmawindowed`, screenshots, and widget install/reload flows
- **Dev Container in VS Code** — best for headless build/test/clang-tidy/mock-server work when you do not need a live KDE desktop session

Repo support for both is now included:

- `.vscode/tasks.json` — Remote-friendly tasks for bootstrap, build, test, demo server, and demo plasmoid launch
- `.vscode/extensions.json` — recommended VS Code extensions (`Remote - SSH`, `Dev Containers`, `CMake Tools`, `C/C++`, `Python`)
- `.devcontainer/` — Fedora 44 headless build/test container definition
- `scripts/demo/install_fedora_ssh_key.ps1` — Windows helper to install your existing `~/.ssh/id_ed25519.pub` on the Fedora laptop for passwordless reuse
- `scripts/demo/copy_windows_ssh_public_key.ps1` + `scripts/demo/install_fedora_ssh_key_locally.sh` — clipboard/local fallback when password-authenticated remote key install is awkward

## Recommended VS Code workflow

### Option A — real VM via Remote SSH

1. start the Fedora KDE 44 VM with your hypervisor
1. from Windows, install your SSH key once:

  ```powershell
  powershell -ExecutionPolicy Bypass -File scripts/demo/install_fedora_ssh_key.ps1
  ```

1. test the alias from the Windows host:

  ```powershell
  ssh fedora-kde-demo hostnamectl --static
  ```

If the one-shot remote installer cannot complete because of password-prompt limitations, use this manual fallback:

1. on Windows, copy your public key:

  ```powershell
  powershell -ExecutionPolicy Bypass -File scripts/demo/copy_windows_ssh_public_key.ps1
  ```

1. on the Fedora laptop, run:

  ```bash
  bash scripts/demo/install_fedora_ssh_key_locally.sh
  ```

1. paste the key, press Enter, then press Ctrl-D
1. re-run `ssh fedora-kde-demo hostnamectl --static` from Windows

1. connect from VS Code using **Remote - SSH** and choose `fedora-kde-demo`
1. open this repository inside the Fedora guest
1. run one of the repo tasks:

- `Fedora Remote: Bootstrap Test Env`
- `Fedora Remote: Prepare Widget`
- `Fedora Remote: Demo Server`
- `Fedora Remote: Launch Demo Plasmoid`

This is the preferred path for real widget testing.

### Option B — headless Fedora container via Dev Containers

1. in VS Code, run **Dev Containers: Reopen in Container**
2. let the Fedora 44 container build from `.devcontainer/Dockerfile`
3. run build/test/demo server tasks inside the container

This is useful for compile/test/mock-server work, but it does **not** replace a real KDE Plasma session.

## Recommended host setup

- Windows 11 host with Hyper-V, VMware, or VirtualBox
- Fedora KDE 44 guest with a shared folder that points at this repository
- Firefox installed in the guest if you want to exercise Browser Sync diagnostics manually
- a clean user profile in the guest for screenshot capture

## Fedora guest packages

Install the normal build dependencies plus Python tooling for the demo helpers:

```bash
sudo dnf install \
  cmake extra-cmake-modules gcc-c++ just python3 python3-pip python3-venv firefox \
  qt6-qtbase qt6-qtbase-devel qt6-qtdeclarative-devel libplasma-devel \
  kf6-kwallet-devel kf6-ki18n-devel kf6-knotifications-devel kf6-kcoreaddons-devel
```

## Fast path

From inside the Fedora KDE 44 guest, the quickest reproducible setup is:

```bash
cd /path/to/shared/plasma-ai-usage-monitor
bash scripts/demo/setup_fedora_kde_test_env.sh --fedora 44 --install-missing
source .venv/bin/activate
```

Add `--prepare-widget` if you want the helper to build the debug tree, install the compiled plugin, install the user-local plasmoid, and reload Plasma for you.

## Shared-workspace virtual environment

If you want the manual version of the same setup, create the repo-local `.venv` from inside the Fedora VM so the interpreter, scripts, and native paths all match the Linux runtime:

```bash
cd /path/to/shared/plasma-ai-usage-monitor
bash scripts/demo/bootstrap_demo_env.sh
source .venv/bin/activate
```

This environment is for the mock server and future capture helpers. Do not create the live-test `.venv` on Windows first.

## Optional Windows host helper environment

If you want to validate the mock server or JSON preset from the Windows workspace before booting the Fedora VM, you can create a separate host-only environment:

```powershell
./scripts/demo/bootstrap_demo_env.ps1
```

That script creates `.venv-host`, which is safe for Windows-side helper usage and does not conflict with the Fedora `.venv` used for real widget testing.

## Start the mocked demo server

With the Linux `.venv` active:

```bash
python scripts/demo/mock_ai_usage_server.py
```

By default the mock server listens on `http://127.0.0.1:8080` so it matches the default `PLASMA_AI_MONITOR_DEMO=1` routes inside the widget. It also serves the `scripts/demo/showcase_preset.json` values. If port 8080 is occupied, start the server with `--port 18080` and launch the widget with `PLASMA_AI_MONITOR_DEMO_BASE_URL=http://127.0.0.1:18080`.

## Install the widget inside the VM

For a first full install in the guest, prefer the compiled-plugin path:

```bash
just build-debug
sudo cmake --install build
just install-user
just reload
```

For later QML-only iterations, you can usually use:

```bash
just dev
```

## Suggested mock base URLs

If you want deterministic provider cards without relying solely on the demo flag fallback, point providers at the following custom base URLs:

| Surface       | Custom base URL                               |
| ------------- | --------------------------------------------- |
| OpenAI        | `http://127.0.0.1:8080/mock/openai/v1`        |
| Anthropic     | `http://127.0.0.1:8080/mock/anthropic/v1`     |
| Google Gemini | `http://127.0.0.1:8080/mock/google/v1beta`    |
| Mistral       | `http://127.0.0.1:8080/mock/mistral`          |
| DeepSeek      | `http://127.0.0.1:8080/mock/deepseek`         |
| Groq          | `http://127.0.0.1:8080/mock/groq`             |
| xAI           | `http://127.0.0.1:8080/mock/xai`              |
| OpenRouter    | `http://127.0.0.1:8080/mock/openrouter`       |
| Together AI   | `http://127.0.0.1:8080/mock/together`         |
| Cohere        | `http://127.0.0.1:8080/mock/cohere`           |
| Google Veo    | `http://127.0.0.1:8080/mock/googleveo/v1beta` |
| Azure OpenAI  | `http://127.0.0.1:8080/mock/azure`            |

## Canonical demo preset

The mocked preset is designed for media capture rather than exhaustive functional testing:

- multiple connected providers with visible cost, request, and quota bars
- one richer card with balance or credits data
- subscription cards can use the built-in demo-mode mock endpoints instead of live browser sessions
- a clean history/compare scene populated by manual refreshes during the session

Adjust `scripts/demo/showcase_preset.json` if you need different numbers for a capture session.

## Screenshot flow

1. boot the VM into a clean Plasma session
2. start the mock server
3. install or reload the widget
4. apply the canonical mock base URLs
5. use the shot list in `assets/screenshots/README.md`
6. review every capture at full size before replacing the canonical assets

## Notes for manual QA

- verify the popup after a cold Plasma restart
- verify at least one manual refresh per enabled provider
- verify that screenshots do not include personal data, local hostnames, or browser sessions
- keep the panel layout stable between shots so the GitHub and KDE Store media set looks intentional
