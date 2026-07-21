import threading
import requests
import json
import time

url_ingest = "http://localhost:8080/api/bridge/ingest"
url_accounts = "http://localhost:8080/api/accounts"
url_demo = "http://localhost:8080/api/demo/send"

print("Generating 50 DISTINCT mesh packet payloads...")
payloads = []
# Send 10 rupees 50 times (Alice has 5000 so she won't bounce due to insufficient funds)
for _ in range(50):
    demo_res = requests.post(url_demo, json={
        "senderVpa": "alice@demo",
        "receiverVpa": "bob@demo",
        "amount": 10.00,
        "pin": "123456",
        "ttl": 5,
        "startDevice": "phone-bridge"
    }).json()

    payload = {
        "packetId": demo_res['packetId'],
        "ttl": 5,
        "createdAt": 1000,
        "ciphertext": demo_res['fullCiphertext']
    }
    payloads.append(json.dumps(payload))

headers = {
    "Content-Type": "application/json",
    "X-Bridge-Node-Id": "test-bridge",
    "X-Hop-Count": "0"
}

errors = []
inconsistent_reads = []
read_count = 0
settled_count = 0
dropped_count = 0
lock = threading.Lock()

def run_ingest(payload_str):
    global settled_count, dropped_count
    try:
        resp = requests.post(url_ingest, headers=headers, data=payload_str)
        outcome = resp.json().get('outcome', 'ERROR')
        with lock:
            if outcome == "SETTLED":
                settled_count += 1
            elif outcome == "DUPLICATE_DROPPED":
                dropped_count += 1
    except Exception as e:
        with lock:
            errors.append(str(e))

def run_read():
    global read_count
    try:
        # Keep reading in a tight loop to maximize contention
        for _ in range(10):
            resp = requests.get(url_accounts)
            if resp.status_code == 200:
                accounts = resp.json()
                total = sum(acc['balance'] for acc in accounts)
                with lock:
                    read_count += 1
                    # Total should ALWAYS be exactly 9000, no matter how many writes are inflight
                    if total != 9000.0:
                        inconsistent_reads.append(f"Inconsistent total: {total}, dump: {accounts}")
    except Exception as e:
        with lock:
            errors.append(str(e))

threads = []
# Start readers
print("Spawning 20 reader threads (each looping 10x)...")
for _ in range(20):
    t = threading.Thread(target=run_read)
    threads.append(t)
    t.start()

# Start writers
print("Spawning 50 concurrent writer threads with UNIQUE payloads...")
for p in payloads:
    t = threading.Thread(target=run_ingest, args=(p,))
    threads.append(t)
    t.start()

for t in threads:
    t.join()

print("\n=== Results ===")
print(f"Total Reads Executed: {read_count}")
print(f"Read Inconsistencies Detected: {len(inconsistent_reads)}")
for r in inconsistent_reads:
    print(r)
print(f"Total Errors (Crash/Network): {len(errors)}")
print(f"Ingest SETTLED count: {settled_count}")
print(f"Ingest DUPLICATE_DROPPED count: {dropped_count}")
