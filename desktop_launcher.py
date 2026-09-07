"""Robust Windows desktop launcher for Catalyst Longevity Research.

The scientific application remains the existing Streamlit multipage project.
This launcher is designed for both PyInstaller one-file and one-folder builds.
It starts the local Streamlit server in headless mode, waits for the health
endpoint, then opens the user's default browser explicitly. Startup failures
are written to a persistent log and shown in a native Windows dialog.
"""
from __future__ import annotations

import os
import socket
import sys
import threading
import time
import traceback
import urllib.error
import urllib.request
import webbrowser
from datetime import datetime
from pathlib import Path


# PyInstaller's windowed bootloader sets the standard streams to None. Some
# libraries still expect file-like streams during import/startup.
if sys.stdin is None:
    sys.stdin = open(os.devnull, "r", encoding="utf-8")
if sys.stdout is None:
    sys.stdout = open(os.devnull, "w", encoding="utf-8")
if sys.stderr is None:
    sys.stderr = open(os.devnull, "w", encoding="utf-8")

# Keep the principal runtime dependencies visible to PyInstaller. The actual
# application imports them again when Streamlit executes app.py and its pages.
import openpyxl  # noqa: E402,F401
import pandas  # noqa: E402,F401
import pypdf  # noqa: E402,F401
import streamlit  # noqa: E402,F401
from streamlit.web import cli as streamlit_cli  # noqa: E402


APP_NAME = "Catalyst Longevity Research"
LOG_NAME = "launcher.log"


def app_data_dir() -> Path:
    """Return a persistent, user-writable application data directory."""
    base = os.getenv("LOCALAPPDATA") or os.getenv("APPDATA") or str(Path.home())
    path = Path(base) / APP_NAME
    path.mkdir(parents=True, exist_ok=True)
    return path


def log_path() -> Path:
    requested = os.getenv("CATALYST_DESKTOP_LOG", "").strip()
    if requested:
        return Path(requested).expanduser().resolve()
    return app_data_dir() / LOG_NAME


def write_log(message: str) -> None:
    """Write timestamped startup diagnostics without requiring a console."""
    try:
        path = log_path()
        path.parent.mkdir(parents=True, exist_ok=True)
        timestamp = datetime.now().isoformat(timespec="seconds")
        with path.open("a", encoding="utf-8") as handle:
            handle.write(f"[{timestamp}] {message}\n")
    except Exception:
        # Logging must never prevent the application from starting.
        pass


def native_message(title: str, message: str, error: bool = False) -> None:
    """Show a native Windows dialog; silently fall back on non-Windows hosts."""
    if os.name != "nt":
        return
    try:
        import ctypes

        flags = 0x10 if error else 0x40  # MB_ICONERROR / MB_ICONINFORMATION
        ctypes.windll.user32.MessageBoxW(None, message, title, flags)
    except Exception:
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
    """Use a requested diagnostic port when supplied, otherwise choose a free port."""
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


def is_ci_headless() -> bool:
    value = os.getenv("CATALYST_DESKTOP_HEADLESS", "").strip().lower()
    return value in {"1", "true", "yes", "on"}


def health_url(port: int) -> str:
    return f"http://127.0.0.1:{port}/_stcore/health"


def app_url(port: int) -> str:
    return f"http://127.0.0.1:{port}/"


def wait_for_server_and_open(port: int, timeout_s: int = 120) -> None:
    """Wait for Streamlit to become healthy, then open the browser explicitly."""
    deadline = time.monotonic() + timeout_s
    check_url = health_url(port)
    target_url = app_url(port)
    while time.monotonic() < deadline:
        try:
            with urllib.request.urlopen(check_url, timeout=2) as response:
                if int(getattr(response, "status", 200)) == 200:
                    write_log(f"Health check passed: {check_url}")
                    if not is_ci_headless():
                        opened = webbrowser.open_new_tab(target_url)
                        write_log(f"Browser open requested for {target_url}; result={opened}")
                        if not opened:
                            native_message(
                                APP_NAME,
                                "软件已启动，但系统没有自动打开浏览器。\n\n"
                                f"请手动打开：{target_url}\n\n"
                                f"启动日志：{log_path()}",
                            )
                    return
        except (urllib.error.URLError, TimeoutError, OSError):
            pass
        time.sleep(0.75)

    write_log(f"Health check timed out after {timeout_s}s: {check_url}")
    if not is_ci_headless():
        native_message(
            APP_NAME,
            "软件启动时间过长，尚未检测到本地服务。\n\n"
            "请等待片刻后再次双击，或把启动日志发给我排查。\n\n"
            f"日志位置：{log_path()}",
            error=True,
        )


def main() -> int:
    root = bundle_root()
    app_path = root / "app.py"

    write_log("=" * 72)
    write_log(f"Launcher started; executable={Path(sys.executable).resolve()}")
    write_log(f"Frozen={bool(getattr(sys, 'frozen', False))}; root={root}")
    write_log(f"Application entry point={app_path}")
    write_log(f"Windows={os.name == 'nt'}; cwd(before)={Path.cwd()}")

    if not app_path.exists():
        raise FileNotFoundError(f"Bundled application entry point not found: {app_path}")

    os.chdir(root)
    sys.path.insert(0, str(root))

    # Run Streamlit headless in the background process and open the browser
    # ourselves. This is more reliable after PyInstaller freezing than relying
    # on Streamlit's browser-launch behavior.
    os.environ.setdefault("STREAMLIT_BROWSER_GATHER_USAGE_STATS", "false")
    os.environ.setdefault("STREAMLIT_SERVER_FILE_WATCHER_TYPE", "none")
    os.environ.setdefault("STREAMLIT_GLOBAL_DEVELOPMENT_MODE", "false")

    port = resolve_port()
    write_log(f"Selected loopback port={port}")

    watcher = threading.Thread(
        target=wait_for_server_and_open,
        args=(port,),
        name="catalyst-browser-opener",
        daemon=True,
    )
    watcher.start()

    sys.argv = [
        "streamlit",
        "run",
        str(app_path),
        "--server.address=127.0.0.1",
        f"--server.port={port}",
        "--server.headless=true",
        "--server.fileWatcherType=none",
        "--browser.gatherUsageStats=false",
        "--global.developmentMode=false",
    ]
    write_log("Entering Streamlit CLI")
    return int(streamlit_cli.main() or 0)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except BaseException:
        details = traceback.format_exc()
        write_log("Launcher terminated with exception:\n" + details)
        if not is_ci_headless():
            native_message(
                APP_NAME,
                "Catalyst Longevity Research 启动失败。\n\n"
                "我已经把详细错误写入日志，请把该日志发给我即可继续定位。\n\n"
                f"日志位置：{log_path()}",
                error=True,
            )
        raise
