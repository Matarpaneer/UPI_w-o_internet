import requests
import subprocess
import time
import json
import sys
import os

BASE_URL = "http://localhost:8080/api"

print("--- 1. Fetching Server Public Key ---")
pk_resp = requests.get(f"{BASE_URL}/server-key").json()
pubkey = pk_resp['publicKey']
print(f"Got public key: {pubkey[:30]}...")

def craft_and_ingest(time_offset_ms, name):
    print(f"\n--- 2. Direct External Ingest: {name} ---")
    signed_at = int(time.time() * 1000) + time_offset_ms
    
    craft_packet_path = "./craft_packet" if os.path.exists("./craft_packet") else "./build/craft_packet"
    ctx = subprocess.check_output([craft_packet_path, pubkey, str(signed_at)]).decode('utf-8').strip()
    
    payload = {
        "packetId": f"packet-{name}",
        "ttl": 5,
        "createdAt": signed_at,
        "ciphertext": ctx
    }
    
    headers = {
        "Content-Type": "application/json",
        "X-Bridge-Node-Id": "direct-bridge",
        "X-Hop-Count": "1"
    }
    
    resp = requests.post(f"{BASE_URL}/bridge/ingest", json=payload, headers=headers)
    print(f"Status: {resp.status_code}")
    print(f"Response: {resp.json()}")

# Valid (now)
craft_and_ingest(0, "VALID")
# Stale (> 24h past)
craft_and_ingest(-25 * 3600 * 1000, "STALE_24H")
# Future (> 5m future)
craft_and_ingest(6 * 60 * 1000, "FUTURE_5M")

print("\n--- 3. Malformed Requests ---")
# Missing field demo/send
resp1 = requests.post(f"{BASE_URL}/demo/send", json={"senderVpa": "alice@demo"}) # missing amount, etc
print(f"Malformed demo/send status: {resp1.status_code}")
if resp1.status_code != 200:
    print(f"Body: {resp1.text}")

# Invalid JSON bridge/ingest
resp2 = requests.post(f"{BASE_URL}/bridge/ingest", data="{invalid_json:", headers={"Content-Type": "application/json"})
print(f"Invalid JSON ingest status: {resp2.status_code}")
if resp2.status_code != 200:
    print(f"Body: {resp2.text}")
