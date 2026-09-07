"""Friendly launcher for Catalyst Longevity Analyzer."""
from __future__ import annotations

import importlib.util
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent


def main() -> int:
    if importlib.util.find_spec("streamlit") is None:
        print("尚未安装网页界面依赖。")
        print("请先运行：python -m pip install -r requirements-ui.txt")
        return 1

    print("正在启动 Catalyst Longevity Analyzer...")
    print("浏览器通常会自动打开；如果没有，请使用终端显示的本地网址。")
    return subprocess.call(
        [sys.executable, "-m", "streamlit", "run", str(ROOT / "app.py")],
        cwd=ROOT,
    )


if __name__ == "__main__":
    raise SystemExit(main())
