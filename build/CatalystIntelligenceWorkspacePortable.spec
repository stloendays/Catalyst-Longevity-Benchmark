# -*- mode: python ; coding: utf-8 -*-
"""PyInstaller one-folder recipe for the recommended Windows portable build.

The one-folder build is intentionally preferred over the one-file build for
end-user testing: it avoids large temporary extraction at every launch and is
less likely to be delayed or blocked by endpoint protection software.
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
    [],
    exclude_binaries=True,
    name="Catalyst_Intelligence_Workspace",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)

coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=False,
    upx_exclude=[],
    name="Catalyst_Longevity_Portable",
)
