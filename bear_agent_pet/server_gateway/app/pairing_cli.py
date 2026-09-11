from __future__ import annotations

import argparse
import datetime as dt
import json

from .pairing import issue_pairing_code, list_devices, revoke_device


def _format_time(timestamp: int | None) -> str:
    if not timestamp:
        return "-"
    return dt.datetime.fromtimestamp(timestamp, tz=dt.timezone.utc).astimezone().isoformat(timespec="seconds")


def main() -> int:
    parser = argparse.ArgumentParser(description="Tony desktop device pairing administration")
    sub = parser.add_subparsers(dest="command", required=True)

    issue = sub.add_parser("issue", help="issue a one-time pairing code")
    issue.add_argument("--ttl", type=int, default=600, help="validity in seconds (60-3600)")

    sub.add_parser("list", help="list paired devices")

    revoke = sub.add_parser("revoke", help="revoke one paired device")
    revoke.add_argument("device_id")

    args = parser.parse_args()
    if args.command == "issue":
        code, expires_at = issue_pairing_code(args.ttl)
        print(f"PAIRING_CODE={code}")
        print(f"EXPIRES_AT={_format_time(expires_at)}")
        return 0

    if args.command == "list":
        print(json.dumps(list_devices(), ensure_ascii=False, indent=2))
        return 0

    if args.command == "revoke":
        if not revoke_device(args.device_id):
            print(f"Device not found or already revoked: {args.device_id}")
            return 1
        print(f"REVOKED={args.device_id}")
        return 0

    return 2


if __name__ == "__main__":
    raise SystemExit(main())
