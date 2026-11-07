#!/usr/bin/env bash
set -euo pipefail

echo "Reloading plasmashell..."
if command -v systemctl >/dev/null 2>&1 && systemctl --user list-unit-files plasma-plasmashell.service >/dev/null 2>&1; then
  if command -v kquitapp6 >/dev/null 2>&1; then
    kquitapp6 plasmashell || true
  fi
  sleep 1
  if systemctl --user restart plasma-plasmashell.service; then
    PLASMA_PID="$(systemctl --user show --property MainPID --value plasma-plasmashell.service 2>/dev/null || true)"
    echo "plasmashell restarted via systemd (PID ${PLASMA_PID:-unknown})."
    echo "Wait a few seconds for the panel to appear, then right-click to add the widget."
    echo "If your latest C++ plugin changes are missing, reload alone was not enough; rebuild/install the plugin first, then reload again."
    exit 0
  fi
fi

if command -v kquitapp6 >/dev/null 2>&1; then
  kquitapp6 plasmashell || true
else
  pkill -x plasmashell || true
fi

# Give it a moment to fully exit
sleep 1

nohup plasmashell >/dev/null 2>&1 &
PLASMA_PID=$!
echo "plasmashell restarted (PID $PLASMA_PID)."
echo "Wait a few seconds for the panel to appear, then right-click to add the widget."
echo "If your latest C++ plugin changes are missing, reload alone was not enough; rebuild/install the plugin first, then reload again."
