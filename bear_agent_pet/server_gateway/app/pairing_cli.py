from __future__ import annotations

import argparse
import datetime as dt
import json
import os
from pathlib import Path

from .pairing import (
    approve_device_pairing_request,
    issue_pairing_code,
    list_device_pairing_requests,
    list_devices,
    revoke_device,
)


def _format_time(timestamp: int | None) -> str:
    if not timestamp:
        return "-"
    return dt.datetime.fromtimestamp(timestamp, tz=dt.timezone.utc).astimezone().isoformat(timespec="seconds")


def _store_path() -> Path:
    return Path(
        os.getenv(
            "BEAR_PAIRING_STORE",
            "/home/ubuntu/.local/share/bear-agent/pairing.json",
        )
    )


def _repair_store_ownership() -> None:
    """Keep the pairing store readable by the gateway after an admin runs this CLI as root."""
    path = _store_path()
    if not path.exists():
        return
    try:
        path.chmod(0o600)
        if hasattr(os, "geteuid") and os.geteuid() == 0:
            parent = path.parent.stat()
            os.chown(path, parent.st_uid, parent.st_gid)
    except OSError:
        # The pairing operation itself succeeded; ownership repair is best effort on
        # non-standard installations where the caller cannot chown the store.
        pass


def main() -> int:
    parser = argparse.ArgumentParser(description="Tony desktop device pairing administration")
    sub = parser.add_subparsers(dest="command", required=True)

    issue = sub.add_parser("issue", help="issue a legacy one-time pairing code")
    issue.add_argument("--ttl", type=int, default=600, help="validity in seconds (60-3600)")

    approve = sub.add_parser("approve", help="approve a connection code displayed by Tony Desktop Pet")
    approve.add_argument("code", help="8-character code shown automatically on the desktop")

    sub.add_parser("pending", help="list unexpired desktop pairing requests")
    sub.add_parser("list", help="list paired devices")

    revoke = sub.add_parser("revoke", help="revoke one paired device")
    revoke.add_argument("device_id")

    args = parser.parse_args()
    if args.command == "issue":
        code, expires_at = issue_pairing_code(args.ttl)
        _repair_store_ownership()
        print(f"PAIRING_CODE={code}")
        print(f"EXPIRES_AT={_format_time(expires_at)}")
        return 0

    if args.command == "approve":
        try:
            row = approve_device_pairing_request(args.code)
        except ValueError as exc:
            print(f"APPROVE_FAILED={exc}")
            return 1
        _repair_store_ownership()
        paired = bool(row["already_paired"])
        print("APPROVED=true")
        print(f"DEVICE_NAME={row['device_name']}")
        print(f"EXPIRES_AT={_format_time(row['expires_at'])}")
        print(f"ALREADY_PAIRED={'true' if paired else 'false'}")
        print(f"WAITING_FOR_DEVICE_CLAIM={'false' if paired else 'true'}")
        if not paired:
            print("NEXT_STATE=approved; waiting for Tony to poll /pair/status and claim its device token")
        return 0

    if args.command == "pending":
        print(json.dumps(list_device_pairing_requests(), ensure_ascii=False, indent=2))
        return 0

    if args.command == "list":
        print(json.dumps(list_devices(), ensure_ascii=False, indent=2))
        return 0

    if args.command == "revoke":
        if not revoke_device(args.device_id):
            print(f"Device not found or already revoked: {args.device_id}")
            return 1
        _repair_store_ownership()
        print(f"REVOKED={args.device_id}")
        return 0

    return 2


if __name__ == "__main__":
    raise SystemExit(main())
