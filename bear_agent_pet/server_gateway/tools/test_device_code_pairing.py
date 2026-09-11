from __future__ import annotations

import json
import os
import tempfile
from pathlib import Path


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        store = Path(tmp) / "pairing.json"
        os.environ["BEAR_PAIRING_STORE"] = str(store)

        from app.pairing import (
            approve_device_pairing_request,
            claim_device_pairing_request,
            create_device_pairing_request,
            list_devices,
            token_valid,
        )

        request_id, code, expires_at = create_device_pairing_request("Test Windows PC", ttl_seconds=600)
        assert len(code) == 9 and code[4] == "-", code
        assert expires_at > 0

        status, token, device_id = claim_device_pairing_request(request_id)
        assert (status, token, device_id) == ("pending", None, None)

        approved = approve_device_pairing_request(code)
        assert approved["device_name"] == "Test Windows PC"
        assert approved["already_paired"] is False

        status, token, device_id = claim_device_pairing_request(request_id)
        assert status == "approved"
        assert token and device_id
        assert token_valid(token)

        # A lost HTTP response must be safe to retry without creating another device.
        status2, token2, device_id2 = claim_device_pairing_request(request_id)
        assert (status2, token2, device_id2) == ("approved", token, device_id)
        devices = list_devices()
        assert len(devices) == 1, devices
        assert devices[0]["paired_via"] == "device_code", devices

        persisted = json.loads(store.read_text(encoding="utf-8"))
        serialized = json.dumps(persisted)
        assert token not in serialized, "plaintext bearer token must never be persisted"
        assert request_id not in serialized, "plaintext request id must never be persisted"

    print("TONY_DEVICE_CODE_PAIRING_TEST=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
