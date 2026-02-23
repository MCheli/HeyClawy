import websocket, json, hashlib, struct
from load_secrets import load_secrets

cfg = load_secrets()
ws = websocket.WebSocket()
ws.connect(f"ws://{cfg['OC_HOST']}:{cfg['OC_PORT']}")
ws.settimeout(5)

msg = json.loads(ws.recv())

# Simple connect with token - check policy
connect_req = {
    "type": "req", "id": "1", "method": "connect",
    "params": {
        "minProtocol": 3, "maxProtocol": 3,
        "client": {"id": "test", "version": "1.0", "platform": "test", "mode": "cli"},
        "role": "operator",
        "scopes": ["operator.read", "operator.write"],
        "auth": {"token": cfg['OC_TOKEN']}
    }
}
ws.send(json.dumps(connect_req))
resp = json.loads(ws.recv())
policy = resp.get("payload", {}).get("policy", {})
snapshot = resp.get("payload", {}).get("snapshot", {})
print("POLICY:", json.dumps(policy, indent=2)[:1000])
print("SNAPSHOT keys:", list(snapshot.keys()) if isinstance(snapshot, dict) else str(snapshot)[:200])

# try health (might work without scope)
ws.send(json.dumps({"type": "req", "id": "h1", "method": "health", "params": {}}))
for _ in range(10):
    msg = json.loads(ws.recv())
    if msg.get("type") == "res" and msg.get("id") == "h1":
        print("health response:", json.dumps(msg)[:500])
        break

# try status
ws.send(json.dumps({"type": "req", "id": "s1", "method": "status", "params": {}}))
for _ in range(10):
    msg = json.loads(ws.recv())
    if msg.get("type") == "res" and msg.get("id") == "s1":
        print("status response:", json.dumps(msg)[:500])
        break

# try chat.history
ws.send(json.dumps({"type": "req", "id": "ch1", "method": "chat.history", "params": {"sessionKey": "default", "limit": 1}}))
for _ in range(10):
    msg = json.loads(ws.recv())
    if msg.get("type") == "res" and msg.get("id") == "ch1":
        ok = msg.get("ok")
        err = msg.get("error", {})
        print(f"chat.history: ok={ok} err={err}")
        break

ws.close()
