# Tony message routing (v1.0.3)

Tony now routes chat input before invoking the server model.

- `local_fixed`: short identity, greeting, status and social replies.
- `local_action`: short replies bound to Tony actions such as Paw, Nod, HeadTilt, Hop, Spin, Sniff and Dance.
- `server_agent`: technical, analytical, coding, tool or open-ended requests.
- `/agent <message>` or `agent: <message>`: explicitly bypasses local replies.

Local replies use the live `TonyBehaviorEngine::Snapshot`, so state-sensitive answers such as cold/status reflect the current pet state. Every route is written to the operator log as `chat_route`.
