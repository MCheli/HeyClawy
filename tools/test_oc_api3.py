import websocket, json
from load_secrets import load_secrets

cfg = load_secrets()
ws = websocket.WebSocket()
ws.connect(f"ws://{cfg['OC_HOST']}:{cfg['OC_PORT']}")
ws.settimeout(5)

msg = json.loads(ws.recv())

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
print("Full connect response:")
print(json.dumps(resp, indent=2)[:3000])
ws.close()
