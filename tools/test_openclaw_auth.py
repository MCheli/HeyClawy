#!/usr/bin/env python3
"""Test OpenClaw device identity authentication with persistent keypair and auto-pairing."""

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization
import base64, json, websocket, time, hashlib, os, sys

KEYFILE = os.path.join(os.path.dirname(__file__), "heyclawy_device_key.bin")

# Load config from secrets.txt
sys.path.insert(0, os.path.dirname(__file__))
from load_secrets import load_secrets
cfg = load_secrets()
HOST = cfg['OC_HOST']
PORT = int(cfg['OC_PORT'])
TOKEN = cfg['OC_TOKEN']
WS_URL = f"ws://{HOST}:{PORT}"


def load_or_generate_key():
    if os.path.exists(KEYFILE):
        with open(KEYFILE, "rb") as f:
            priv_bytes = f.read()
        pk = Ed25519PrivateKey.from_private_bytes(priv_bytes)
        print("Loaded existing keypair")
    else:
        pk = Ed25519PrivateKey.generate()
        priv_bytes = pk.private_bytes(
            serialization.Encoding.Raw,
            serialization.PrivateFormat.Raw,
            serialization.NoEncryption(),
        )
        with open(KEYFILE, "wb") as f:
            f.write(priv_bytes)
        print("Generated new keypair, saved to", KEYFILE)
        # Also export as hex for ESP32 secrets.h
        print(f"Private key hex: {priv_bytes.hex()}")
    return pk


def get_device_info(pk):
    pub = pk.public_key()
    pub_bytes = pub.public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    pub_b64 = base64.urlsafe_b64encode(pub_bytes).decode().rstrip("=")
    dev_id = hashlib.sha256(pub_bytes).hexdigest()
    return dev_id, pub_b64, pub_bytes


def wait_for_response(ws, expected_id, timeout=10):
    """Read messages until we get a response with the expected ID."""
    start = time.time()
    while time.time() - start < timeout:
        msg = ws.recv()
        data = json.loads(msg)
        if data.get("type") == "res" and data.get("id") == expected_id:
            return data
    return None


def approve_pairing(request_id):
    """Connect as control-ui and approve a pairing request."""
    print(f"Approving pairing request: {request_id}")
    ws = websocket.create_connection(WS_URL, timeout=15)
    ws.recv()  # challenge
    ctrl_connect = {
        "type": "req", "id": "1", "method": "connect",
        "params": {
            "minProtocol": 3, "maxProtocol": 3,
            "client": {"id": "openclaw-control-ui", "version": "0.2.0", "platform": "win32", "mode": "ui"},
            "role": "operator",
            "scopes": ["operator.admin", "operator.pairing", "operator.read", "operator.write"],
            "auth": {"token": TOKEN},
        },
    }
    ws.send(json.dumps(ctrl_connect))
    r = wait_for_response(ws, "1")
    if not r or not r.get("ok"):
        err_msg = r.get("error", {}).get("message", "unknown") if r else "timeout"
        print("Control-UI connect failed:", err_msg)
        ws.close()
        return False

    ws.send(json.dumps({
        "type": "req", "id": "2", "method": "device.pair.approve",
        "params": {"requestId": request_id},
    }))
    r = wait_for_response(ws, "2")
    if not r:
        print("Approve timed out")
        ws.close()
        return False
    ok = r.get("ok")
    if ok:
        print("Pairing approved!")
    else:
        print("Approve failed:", json.dumps(r.get("error", {}), indent=2))
    ws.close()
    return ok


def connect_with_device(pk, dev_id, pub_b64):
    """Connect with device identity. Returns (ws, ok) tuple."""
    ws = websocket.create_connection(WS_URL, timeout=15)
    challenge = json.loads(ws.recv())
    nonce = challenge["payload"]["nonce"]

    signed_at = int(time.time() * 1000)
    scopes_str = "operator.read,operator.write"
    payload = f"v2|{dev_id}|gateway-client|cli|operator|{scopes_str}|{signed_at}|{TOKEN}|{nonce}"
    sig = pk.sign(payload.encode())
    sig_b64 = base64.urlsafe_b64encode(sig).decode().rstrip("=")

    connect_req = {
        "type": "req", "id": "1", "method": "connect",
        "params": {
            "minProtocol": 3, "maxProtocol": 3,
            "client": {"id": "gateway-client", "version": "0.2.0", "platform": "esp32s3", "mode": "cli"},
            "role": "operator",
            "scopes": ["operator.read", "operator.write"],
            "auth": {"token": TOKEN},
            "device": {
                "id": dev_id, "publicKey": pub_b64,
                "signature": sig_b64, "signedAt": signed_at, "nonce": nonce,
            },
        },
    }
    ws.send(json.dumps(connect_req))
    r = json.loads(ws.recv())
    return ws, r


def test_chat(ws):
    """Send a test chat message and print the response."""
    chat = {
        "type": "req", "id": "5", "method": "chat.send",
        "params": {
            "sessionKey": "default",
            "message": "Hello from HeyClawy! Say hi briefly.",
            "idempotencyKey": f"t-{int(time.time())}",
        },
    }
    ws.send(json.dumps(chat))
    for _ in range(40):
        try:
            msg = ws.recv()
            data = json.loads(msg)
            t = data.get("type", "")
            e = data.get("event", "")
            if t == "event" and e == "chat":
                p = data.get("payload", {})
                st = p.get("state", "")
                c = p.get("message", {}).get("content", [])
                txt = "".join(x.get("text", "") for x in c)
                print(f"  [{st}] {txt[:200]}")
                if st in ("final", "error"):
                    break
            elif t == "res":
                ok = data.get("ok")
                err = data.get("error", {}).get("message", "")
                print(f"  Res: ok={ok}" + (f" err={err}" if err else ""))
                if not ok:
                    break
        except Exception as ex:
            print(f"  Error: {ex}")
            break


def main():
    pk = load_or_generate_key()
    dev_id, pub_b64, pub_bytes = get_device_info(pk)
    print(f"Device ID: {dev_id}")

    # Also print private key hex for ESP32 integration
    priv_bytes = pk.private_bytes(
        serialization.Encoding.Raw, serialization.PrivateFormat.Raw, serialization.NoEncryption()
    )
    print(f"Private key hex (for secrets.h): {priv_bytes.hex()}")

    # Attempt 1: connect with device identity
    ws, r = connect_with_device(pk, dev_id, pub_b64)
    if not r.get("ok"):
        err = r.get("error", {})
        code = err.get("code", "")
        print(f"Connect failed: {code} - {err.get('message', '')}")

        if code == "NOT_PAIRED":
            req_id = err.get("details", {}).get("requestId", "")
            ws.close()
            if approve_pairing(req_id):
                time.sleep(1)
                ws, r = connect_with_device(pk, dev_id, pub_b64)
                if not r.get("ok"):
                    print("Still failed after pairing:", r.get("error", {}))
                    ws.close()
                    return
            else:
                print("Could not approve pairing")
                return
        else:
            ws.close()
            return

    print("Connected with device identity!")
    print("Testing chat.send...")
    test_chat(ws)
    ws.close()
    print("Done!")


if __name__ == "__main__":
    main()
