#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

from cryptography.fernet import Fernet


def main() -> int:
    parser = argparse.ArgumentParser(description="Decrypt one Tony operator-log batch using the server-local Fernet key.")
    parser.add_argument("encrypted_file", type=Path)
    parser.add_argument(
        "--key",
        type=Path,
        default=Path(os.getenv("BEAR_OPERATOR_LOG_KEY_PATH", "~/.local/share/bear-agent/operator-log.key")).expanduser(),
    )
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    plaintext = Fernet(args.key.read_bytes().strip()).decrypt(args.encrypted_file.read_bytes().strip())
    if args.output:
        args.output.write_bytes(plaintext)
    else:
        sys.stdout.buffer.write(plaintext)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
