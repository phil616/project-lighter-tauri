"""Reject tracked build output; inspect the Git index, not local build folders."""

import os
from pathlib import Path
import subprocess
import sys


OUTPUT_SUFFIXES = (
    ".exe", ".exe.dist", ".dll", ".pdb", ".lib", ".a", ".o", ".obj",
    ".exp", ".ilk", ".idb", ".so", ".dylib", ".pch", ".gch", ".res",
    ".msi", ".msix", ".msixbundle", ".appx", ".appimage", ".dmg", ".pkg",
    ".deb", ".rpm", ".zip", ".7z", ".rar", ".tar", ".tar.gz", ".tar.xz",
    ".tar.zst", ".tgz",
)
OUTPUT_DIRECTORIES = {
    "_packages", "node_modules", "target", "cmakefiles", "dist", "dist-ssr",
    "release-windows", "release-linux", "release-macos", "stage", "out",
    ".venv", "venv", "__pycache__",
}


def is_build_output(path):
    parts = path.lower().split("/")
    return (
        path.lower().endswith(OUTPUT_SUFFIXES)
        or ".so." in parts[-1]
        or any(part in OUTPUT_DIRECTORIES for part in parts[:-1])
        or (len(parts) > 1 and parts[0].startswith(("build", "cmake-build-")))
    )


def main():
    root = Path(__file__).resolve().parent.parent
    result = subprocess.run(
        ["git", "ls-files", "-z"], cwd=root, check=True, stdout=subprocess.PIPE
    )
    paths = [os.fsdecode(path) for path in result.stdout.split(b"\0") if path]
    rejected = [path for path in paths if is_build_output(path)]
    if rejected:
        print("Generated build output is tracked in Git; keep it in CI artifacts:", file=sys.stderr)
        for path in rejected:
            print(f"  {path}", file=sys.stderr)
        return 1
    print(f"Source-only repository check passed ({len(paths)} tracked files).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
