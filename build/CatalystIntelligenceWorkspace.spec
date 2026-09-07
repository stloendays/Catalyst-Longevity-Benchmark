# -*- mode: python ; coding: utf-8 -*-
"""PyInstaller recipe for the Windows portable single-file test release."""

from PyInstaller.utils.hooks import collect_all

block_cipher = None


datas = [
    ("app.py", "."),
    ("pages", "pages"),
    ("src", "src"),
    ("examples", "examples"),
    ("data", "data"),
    ("protocols", "protocols"),
    ("schemas", "schemas"),
    (".streamlit", ".streamlit"),
]
binaries = []
hiddenimports = []

# Streamlit needs its frontend/static assets at runtime.  Materials Project is
# imported lazily by the application, so collect it explicitly for the frozen
# build rather than relying on static import discovery alone.
for package_name in ("streamlit", "mp_api", "pymatgen"):
    package_datas, package_binaries, package_hiddenimports = collect_all(package_name)
    datas += package_datas
    binaries += package_binaries
    hiddenimports += package_hiddenimports


a = Analysis(
    ["desktop_launcher.py"],
    pathex=["."],
    binaries=binaries,
    datas=datas,
    hiddenimports=hiddenimports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
)
pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name="Catalyst_Intelligence_Workspace",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
