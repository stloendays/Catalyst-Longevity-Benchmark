"""Windows desktop launcher for Catalyst Intelligence Workspace.

This module is intentionally small: the scientific application remains the
existing Streamlit multipage project.  When frozen with PyInstaller, bundled
project files are extracted next to this launcher in ``sys._MEIPASS`` and the
Streamlit server is started locally on a free loopback port.
"""
from __future__ import annotations

import os
import socket
import sys
from pathlib import Path

# These imports make the principal runtime dependencies explicit to PyInstaller.
# The application itself imports them from app.py/pages at Streamlit runtime.
import openpyxl  # noqa: F401
import pandas  # noqa: F401
import pypdf  # noqa: F401
import streamlit  # noqa: F401
from streamlit.web import cli as streamlit_cli


def bundle_root() -> Path:
    """Return the repository root in source mode or PyInstaller extraction root."""
    frozen_root = getattr(sys, "_MEIPASS", None)
    if frozen_root:
        return Path(frozen_root).resolve()
    return Path(__file__).resolve().parent


def free_loopback_port() -> int:
    """Reserve an available local TCP port for the Streamlit server."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def main() -> int:
    root = bundle_root()
    app_path = root / "app.py"
    if not app_path.exists():
        raise FileNotFoundError(f"Bundled application entry point not found: {app_path}")

    os.chdir(root)
    sys.path.insert(0, str(root))

    # Keep the desktop build local, reproducible, and quiet about telemetry.
    os.environ.setdefault("STREAMLIT_BROWSER_GATHER_USAGE_STATS", "false")
    os.environ.setdefault("STREAMLIT_SERVER_FILE_WATCHER_TYPE", "none")
    os.environ.setdefault("STREAMLIT_GLOBAL_DEVELOPMENT_MODE", "false")

    port = free_loopback_port()
    sys.argv = [
        "streamlit",
        "run",
        str(app_path),
        "--server.address=127.0.0.1",
        f"--server.port={port}",
        "--server.headless=false",
        "--server.fileWatcherType=none",
        "--browser.gatherUsageStats=false",
        "--global.developmentMode=false",
    ]
    return int(streamlit_cli.main() or 0)


if __name__ == "__main__":
    raise SystemExit(main())
