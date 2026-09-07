# -*- mode: python ; coding: utf-8 -*-
"""PyInstaller single-file recipe kept as a compatibility/fallback build.

The portable one-folder build is the recommended end-user package. This
single-file build remains available for convenience, with UPX disabled to
reduce antivirus false positives and startup friction.
"""

from pathlib import Path

from PyInstaller.utils.hooks import collect_all


ROOT = Path(SPECPATH).resolve().parent


def source(path: str) -> str:
    return str(ROOT / path)


datas = [
    (source("app.py"), "."),
    (source("pages"), "pages"),
    (source("src"), "src"),
    (source("examples"), "examples"),
    (source("data"), "data"),
    (source("protocols"), "protocols"),
    (source("schemas"), "schemas"),
    (source(".streamlit"), ".streamlit"),
]
binaries = []
hiddenimports = []

for package_name in ("streamlit", "mp_api", "pymatgen"):
    package_datas, package_binaries, package_hiddenimports = collect_all(package_name)
    datas += package_datas
    binaries += package_binaries
    hiddenimports += package_hiddenimports


a = Analysis(
    [source("desktop_launcher.py")],
    pathex=[str(ROOT)],
    binaries=binaries,
    datas=datas,
    hiddenimports=hiddenimports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name="Catalyst_Intelligence_Workspace",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
