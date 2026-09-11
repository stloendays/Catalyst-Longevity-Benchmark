# Tony device pairing

Tony gateway v0.7 supports one-time pairing codes and per-device bearer tokens. The gateway should stay bound to `127.0.0.1:18790`; expose it only through a TLS reverse proxy when remote WSS access is enabled.

## Issue a one-time code

On the server:

```bash
cd /home/ubuntu/.local/share/bear-agent/app
/home/ubuntu/.local/share/bear-agent/venv/bin/python -m app.pairing_cli issue --ttl 600
```

The command prints a code such as `ABCD-EFGH-JKLM`. It can be used once and expires automatically.

In the Windows client, right-click Tony and choose `连接 / 配对新设备…`, enter the WSS endpoint and the one-time code. The client receives a random per-device token and stores it with Windows DPAPI, so another Windows account cannot simply copy the saved token and use it.

## List or revoke devices

```bash
/home/ubuntu/.local/share/bear-agent/venv/bin/python -m app.pairing_cli list
/home/ubuntu/.local/share/bear-agent/venv/bin/python -m app.pairing_cli revoke DEVICE_ID
```

The server stores only SHA-256 hashes of device bearer tokens. The plaintext token is returned once to the paired client and is not written to the pairing store.

## Public deployment rule

Before exposing `/agent/ws` through HTTPS/WSS, set `BEAR_AGENT_REQUIRE_AUTH=true` in `/home/ubuntu/.local/share/bear-agent/bear-agent.env` and restart `bear-agent-gateway.service`. Keep port `18790` loopback-only. Terminate TLS on Caddy/Nginx at port 443 and proxy `/pair` plus `/agent/ws` to `127.0.0.1:18790`.

Do not distribute the server SSH private key with TonyDesktopPet.exe.
