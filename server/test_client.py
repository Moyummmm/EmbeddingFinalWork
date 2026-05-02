#!/usr/bin/env python3
"""Simple test client for the TCP registration server.

Frame format: [4-byte big-endian length][JSON payload]
"""
import socket
import struct
import json
import sys

HOST = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 9999


def send_msg(sock, obj):
    payload = json.dumps(obj).encode()
    frame = struct.pack(">I", len(payload)) + payload
    sock.sendall(frame)


def recv_msg(sock):
    head = _recv_all(sock, 4)
    plen = struct.unpack(">I", head)[0]
    data = _recv_all(sock, plen)
    return json.loads(data)


def _recv_all(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("closed")
        buf += chunk
    return buf


def new_conn():
    sock = socket.socket()
    sock.connect((HOST, PORT))
    return sock


def test(label, expr):
    ok = "PASS" if expr else "FAIL"
    print(f"  [{ok}] {label}")


# --- register node-a ---
print("=== register node-a ===")
s1 = new_conn()
send_msg(s1, {"type": "register", "ip": "192.168.1.100", "port": 10001, "name": "node-a"})
r = recv_msg(s1)
print(f"  type={r['type']} peers={len(r['peers'])}")
test("1 peer", len(r["peers"]) == 1)

# --- register node-b ---
print("=== register node-b ===")
s2 = new_conn()
send_msg(s2, {"type": "register", "ip": "192.168.1.101", "port": 10002, "name": "node-b"})
r = recv_msg(s2)
print(f"  type={r['type']} peers={len(r['peers'])}")
test("2 peers after b joins", len(r["peers"]) == 2)

# --- query from node-a ---
print("=== query ===")
send_msg(s1, {"type": "query"})
r = recv_msg(s1)
test("2 peers visible", len(r["peers"]) == 2)

# --- re-register (update) ---
print("=== re-register node-a ===")
send_msg(s1, {"type": "register", "ip": "192.168.1.100", "port": 10001, "name": "node-a-v2"})
r = recv_msg(s1)
names = [p["name"] for p in r["peers"]]
test("name updated to node-a-v2", "node-a-v2" in names)

# --- unregister node-b ---
print("=== unregister node-b ===")
send_msg(s2, {"type": "unregister", "ip": "192.168.1.101", "port": 10002})
r = recv_msg(s2)
test("unregister ack", r["type"] == "unregister_ack")

# --- query after unregister ---
print("=== query after unregister ===")
send_msg(s1, {"type": "query"})
r = recv_msg(s1)
test("only 1 peer left", len(r["peers"]) == 1)

# --- unregister node-a ---
print("=== unregister node-a ===")
send_msg(s1, {"type": "unregister", "ip": "192.168.1.100", "port": 10001})
r = recv_msg(s1)
test("unregister ack", r["type"] == "unregister_ack")

# --- query after all gone ---
print("=== query empty ===")
send_msg(s1, {"type": "query"})
r = recv_msg(s1)
test("0 peers", len(r["peers"]) == 0)

s1.close()
s2.close()
print("\nAll tests passed.")
