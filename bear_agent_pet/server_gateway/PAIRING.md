# Tony device pairing

Tony Desktop Pet uses approval-gated device-code pairing for a simple first-run experience. The gateway stays bound to `127.0.0.1:18790` and is exposed remotely only through the TLS reverse proxy.

## Default first-run flow

An unpaired Tony Desktop Pet automatically requests a short-lived connection code from the public gateway and displays it on the desktop, for example:

```text
ABCD-EFGH
```

The person using the Windows PC does not need to know the WSS URL, SSH configuration, or bearer token. They only send the displayed connection code to the Tony Agent/server owner.

On the server, approve that code with:

```bash
cd /home/ubuntu/.local/share/bear-agent/app
/home/ubuntu/.local/share/bear-agent/venv/bin/python -m app.pairing_cli approve ABCD-EFGH
```

Tony polls the approval state automatically. After approval, the PC receives a per-device bearer token, stores it using Windows DPAPI, connects to `/agent/ws`, and remembers the computer for future launches. The displayed code expires after about 10 minutes and is not itself a bearer token.

The public pairing protocol is:

- `POST /pair/request` — creates a short-lived desktop connection request and returns the visible code plus an unguessable client request ID.
- `POST /pair/status` — the desktop polls with the hidden request ID until the server owner approves the visible code.
- `/agent/ws` — remains protected by the resulting per-device bearer token.

The server pairing store contains hashes rather than plaintext bearer tokens or plaintext request IDs. Public clients cannot approve their own displayed codes.

## Admin and recovery commands

List pending requests or paired devices:

```bash
/home/ubuntu/.local/share/bear-agent/venv/bin/python -m app.pairing_cli pending
/home/ubuntu/.local/share/bear-agent/venv/bin/python -m app.pairing_cli list
```

Revoke one paired computer:

```bash
/home/ubuntu/.local/share/bear-agent/venv/bin/python -m app.pairing_cli revoke DEVICE_ID
```

A legacy one-time recovery code can still be issued if needed:

```bash
/home/ubuntu/.local/share/bear-agent/venv/bin/python -m app.pairing_cli issue --ttl 600
```

Tony also retains the configured reusable friend-code path as an advanced owner recovery mechanism. It is no longer the default first-run UX.

## Public deployment rule

Keep `BEAR_AGENT_REQUIRE_AUTH=true` in `/home/ubuntu/.local/share/bear-agent/bear-agent.env`. Keep port `18790` loopback-only. Terminate TLS on Caddy/Nginx at port 443 and proxy `/pair`, `/pair/request`, `/pair/status`, and `/agent/ws` to `127.0.0.1:18790`.

Never distribute the server SSH private key with TonyDesktopPet.exe and never expose the long-term device bearer token as the visible connection code.
