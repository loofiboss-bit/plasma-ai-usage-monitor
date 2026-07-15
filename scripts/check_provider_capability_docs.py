#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
subprocess.run([sys.executable, str(root / "scripts/generate_provider_capabilities.py"), "--check"],
               cwd=root, check=True)
for guide in ("gemini", "litellm", "cerebras", "fireworks", "perplexity"):
    path = root / "docs/provider-setup" / f"{guide}.md"
    if not path.is_file() or "Scheduled traffic" not in path.read_text(encoding="utf-8"):
        raise SystemExit(f"Provider setup guide missing or incomplete: {path.relative_to(root)}")
