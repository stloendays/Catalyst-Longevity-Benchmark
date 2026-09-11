# Tony Local Bridge Protocol v1

Tony's Local Bridge is the deliberately narrow interface between the remote Agent and the Windows desktop. It gives the Agent useful desktop capabilities without granting arbitrary command execution.

## Security model

The server may request only semantic tools from the allowlist. The Windows client validates the tool and its arguments again before doing anything. Local tools can be disabled globally from Tony's right-click menu.

Sensitive actions are one-shot confirmed on the Windows machine. A remote response, prompt, model output or tool result cannot silently approve its own action.

The v1 bridge does **not** expose shell, PowerShell, cmd.exe, registry editing, process execution, file deletion, arbitrary file writing, credential access or automatic screenshot upload.

## Capability negotiation

After WebSocket connection, the desktop sends:

```json
{
  "type": "client_hello",
  "protocol_version": "1",
  "client": "TonyDesktopPet",
  "client_version": "0.7.0",
  "device_name": "TONY-PC",
  "platform": "windows",
  "capabilities": [
    "read_clipboard",
    "write_clipboard",
    "capture_screen",
    "open_url",
    "open_file",
    "show_notification"
  ]
}
```

The Gateway intersects those capabilities with its own allowlist and replies with `client_hello_ack`. The server must not request a capability that was not negotiated.

## Tool request

```json
{
  "type": "tool_request",
  "request_id": "random-id",
  "tool": "capture_screen",
  "args": {},
  "reason": "Save a screenshot requested by the user"
}
```

The client enters `tool_running`, asks for local confirmation where required, executes the semantic operation, and returns exactly one result for that request ID.

## Tool result

Success:

```json
{
  "type": "tool_result",
  "request_id": "random-id",
  "tool": "capture_screen",
  "ok": true,
  "result": {
    "path": "C:\\Users\\Tony\\Pictures\\Tony\\Tony_20260911_181500_000.png",
    "width": 1920,
    "height": 1080,
    "uploaded": false
  }
}
```

Denied or failed:

```json
{
  "type": "tool_result",
  "request_id": "random-id",
  "tool": "read_clipboard",
  "ok": false,
  "result": {},
  "error": "User denied clipboard access."
}
```

## v1 allowlist

| Tool | Client confirmation | Data returned to server | Notes |
|---|---|---|---|
| `read_clipboard` | Yes | text, length, truncation flag | Text capped at 12,000 characters. Tool output is treated as untrusted data before model use. |
| `write_clipboard` | Yes | character count | Requires explicit content. |
| `capture_screen` | Yes | local path and dimensions | Saves into the local Pictures/Tony directory. Image bytes are not uploaded in v1. |
| `open_url` | Yes | opened URL | Only `http` and `https`. |
| `open_file` | Yes | local path and size | File must already exist and be a regular file. |
| `show_notification` | No | shown status | Low-risk UI-only operation through Tony's tray icon. |

## Request planning

The v1 Gateway uses conservative deterministic intent recognition for local actions. A local tool is emitted only when the user's message explicitly asks for that operation, such as `take a screenshot`, `read my clipboard` or `open https://...`.

If no explicit local-tool intent is recognized, the request follows Tony's existing Qwen/OpenClaw routing. Requests such as `run PowerShell`, `delete this file` or `execute this command` are **not** translated into local execution.

## Prompt-injection boundary

Clipboard and later file/screenshot contents are external data. Before they are passed into a model, the Gateway wraps them with an instruction that the returned content is untrusted data and must not be followed as instructions. This is defense in depth; the desktop allowlist and local confirmation remain the primary controls.

## Planned v2

The next Local Bridge revision can add explicit file/screenshot upload as a separate permissioned capability. It should use size limits, content-type checks, per-request user consent, temporary upload IDs and server-side retention limits rather than exposing arbitrary filesystem access.
