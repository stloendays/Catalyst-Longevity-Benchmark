"""Windows desktop launcher for Catalyst Intelligence Workspace.

The scientific application remains the existing Streamlit multipage project.
When frozen with PyInstaller, bundled project files are extracted under
``sys._MEIPASS`` and the Streamlit server is started on loopback.
"""
from __future__ import annotations

import os
import socket
import sys
import tempfile
import traceback
from datetime import datetime
from pathlib import Path

# Make the principal runtime dependencies explicit to PyInstaller. The app and
# page modules import them again at Streamlit runtime.
import openpyxl  # noqa: F401
import pandas  # noqa: F401
import pypdf  # noqa: F401
import streamlit  # noqa: F401
from streamlit.web import cli as streamlit_cli


def log_path() -> Path:
    requested = os.getenv("CATALYST_DESKTOP_LOG", "").strip()
    if requested:
        return Path(requested).expanduser().resolve()
    return Path(tempfile.gettempdir()) / "Catalyst_Intelligence_Workspace.log"


def write_log(message: str) -> None:
    """Write lightweight startup diagnostics without requiring a console."""
    try:
        path = log_path()
        path.parent.mkdir(parents=True, exist_ok=True)
        timestamp = datetime.now().isoformat(timespec="seconds")
        with path.open("a", encoding="utf-8") as handle:
            handle.write(f"[{timestamp}] {message}\n")
    except Exception:
        # Logging must never prevent the application from starting.
        pass


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


def resolve_port() -> int:
    """Use a requested test port when supplied, otherwise choose a free port."""
    requested = os.getenv("CATALYST_DESKTOP_PORT", "").strip()
    if not requested:
        return free_loopback_port()
    try:
        port = int(requested)
    except ValueError as exc:
        raise ValueError("CATALYST_DESKTOP_PORT must be an integer") from exc
    if not 1 <= port <= 65535:
        raise ValueError("CATALYST_DESKTOP_PORT must be between 1 and 65535")
    return port


def resolve_headless() -> bool:
    """Keep normal desktop launches interactive while allowing CI headless mode."""
    value = os.getenv("CATALYST_DESKTOP_HEADLESS", "").strip().lower()
    return value in {"1", "true", "yes", "on"}


def main() -> int:
    root = bundle_root()
    app_path = root / "app.py"
    write_log(f"Launcher root: {root}")
    write_log(f"Application entry point: {app_path}")
    if not app_path.exists():
        raise FileNotFoundError(f"Bundled application entry point not found: {app_path}")

    os.chdir(root)
    sys.path.insert(0, str(root))

    # Keep the desktop build local, reproducible, and quiet about telemetry.
    os.environ.setdefault("STREAMLIT_BROWSER_GATHER_USAGE_STATS", "false")
    os.environ.setdefault("STREAMLIT_SERVER_FILE_WATCHER_TYPE", "none")
    os.environ.setdefault("STREAMLIT_GLOBAL_DEVELOPMENT_MODE", "false")

    port = resolve_port()
    headless = resolve_headless()
    write_log(f"Starting Streamlit on 127.0.0.1:{port}; headless={headless}")
    sys.argv = [
        "streamlit",
        "run",
        str(app_path),
        "--server.address=127.0.0.1",
        f"--server.port={port}",
        f"--server.headless={'true' if headless else 'false'}",
        "--server.fileWatcherType=none",
        "--browser.gatherUsageStats=false",
        "--global.developmentMode=false",
    ]
    return int(streamlit_cli.main() or 0)


if __name__ == "__main__":
    try:
        write_log("Catalyst Intelligence Workspace launcher started")
        raise SystemExit(main())
    except BaseException:
        write_log("Launcher terminated with exception:\n" + traceback.format_exc())
        raise
