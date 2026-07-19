# Troubleshooting & FAQ

Refer to this guide to resolve common issues, version mismatches, and credential errors before deleting configuration files.

---

## 1. Widget is Missing After Installation
If you cannot find the **AI Usage Monitor** in Plasma's widget picker:
1. Log out of your desktop session and log back in.
2. If you want to force-reload the session without logging out, run:
   ```bash
   ./scripts/reload_plasma.sh
   ```
3. Verify that Plasma has registered the widget package:
   ```bash
   kpackagetool6 --type Plasma/Applet --show com.github.loofi.aiusagemonitor
   ```

---

## 2. Widget Shows an Old Version (shadowed packages)
If the widget displays old features or versions after updating the system package:
1. A user-local widget package in your home directory may be overriding the system-wide RPM package. Check the installed locations by running:
   ```bash
   ./scripts/show_installed_versions.sh
   ```
2. Run the widget in a standalone test window to verify the active package:
   ```bash
   ./scripts/smoke_test_plasmoid.sh
   ```
3. If a user-local installation shadows your updated RPM, remove it:
   ```bash
   kpackagetool6 --type Plasma/Applet --remove com.github.loofi.aiusagemonitor
   ```
4. Restart your Plasma shell.

---

## 3. QML Plugin Cannot Be Loaded (Recovery Screen)
If the QML frontend version does not match the compiled C++ plugin binary version, the widget will display a recovery screen instead of the dashboard.

* **On Fedora**: Reinstall the COPR package to overwrite mismatched components:
  ```bash
  sudo dnf reinstall plasma-ai-usage-monitor
  ```
* **On Source Builds**: Ensure both components were installed using the same prefix:
  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
  sudo cmake --install build
  ```
* After repairing the files, **log out and log back in**. The QML engine caches import failures for the duration of the session, so a Plasma reload or relog is mandatory.

---

## 4. KWallet Does Not Open
If the widget is unable to save or retrieve keys:
1. Open **System Settings → KDE Wallet** and verify that the wallet subsystem is enabled.
2. Open **Diagnostics → Wallet & Secrets** inside the widget configuration to check status.
3. If KWallet is locked or closed, the widget will not be able to decrypt your credentials. Unlock it and refresh the widget.
4. There is no plain-text backup folder. If KWallet remains disabled, secret-backed providers cannot be used.

---

## 5. API Returns 401 or 403 Errors
* **401 Unauthorized**: Re-enter your API key and verify that it has not expired or been revoked.
* **403 Forbidden**: Your API key lacks the necessary user role. This is common with **OpenAI**: you **must use an Admin key** to fetch usage or spending data. Standard project keys do not have permission to read account-wide billing endpoints.
* **AWS or Azure configuration issues**: Make sure your region, deployment name, and API version match the values configured in your cloud console exactly.

---

## 6. Provider is Connected but Usage is "Unknown"
This is expected behavior. Many provider APIs (like Anthropic, Gemini, Groq, and xAI) do not expose account token consumption or billing endpoints.
* The connected status confirms that the widget can reach the service.
* Token metrics remain marked as **Unknown** because they are not reported by the endpoint.
* Check the [Capability Matrix](../docs/provider-capabilities.md) to confirm what metrics your provider supports.

---

## 7. History is Empty
If your history charts do not show any metrics:
1. Ensure history logging is enabled under **Settings → History**.
2. Wait for at least one configured provider to complete a successful polling check.
3. Connection checks or connectivity-only providers do not record token metrics, and they will not populate the database.
4. Verify the database size on the History settings page or check the file manually:
   ```bash
   ls -lh ~/.local/share/plasma-ai-usage-monitor/usage_history.db
   ```

---

## 8. Browser Sync Labs Fails
* Ensure you are signed in to the service (Claude or ChatGPT) in your **Firefox browser**.
* Confirm you have selected the correct Firefox profile folder in **Settings → Subscriptions**.
* If you just logged in, close Firefox once so that it writes session cookies to the local disk database.
* Check the Browser Sync status inside **Diagnostics** for detailed diagnostics.

---

## 9. Antigravity Quota is Unavailable
* Ensure the **Antigravity 2.x** desktop application is running and signed in.
* If you recently updated Antigravity, restart it so that the widget can re-authenticate against the active localhost daemon.
* Use **Refresh / Test connection** in Settings to force the C++ backend to fetch the certificate layout.

---

## 10. How to Collect Logs
If you need to report a bug, follow the Plasma logs while reproducing the error:
```bash
journalctl --user -f | grep -i -E 'plasma|aiusage|qml'
```

Additionally, copy the support report from **Settings → Diagnostics → Copy support report**. This exports a redacted log detailing the install layers, database status, and source states with all sensitive tokens and API keys removed.
