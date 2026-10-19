"""Expose CMake's tag-derived version to GitHub Actions without resolving it again."""

import json
import os
from pathlib import Path
import sys


metadata = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
if output := os.environ.get("GITHUB_OUTPUT"):
    with open(output, "a", encoding="utf-8") as stream:
        stream.write(f"version={metadata['version']}\ntag={metadata['tag']}\n")
print(f"Lighter {metadata['version']} ({metadata['tag']}, {metadata['commit'][:7]})")
