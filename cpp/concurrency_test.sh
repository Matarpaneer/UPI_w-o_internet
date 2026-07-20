#!/bin/bash
set -e

BASE_URL="http://127.0.0.1:8080/api"
echo "=== Concurrency Test: 50 identical requests to /bridge/ingest ==="

# 1. Grab a valid packet from /demo/send
echo "Generating payload..."
SEND_RES=$(curl -s -X POST "$BASE_URL/demo/send" \
  -H "Content-Type: application/json" \
  -d '{"senderVpa":"alice@demo","receiverVpa":"bob@demo","amount":250.00,"pin":"123456","ttl":5,"startDevice":"phone-bridge"}')

PACKET_ID=$(echo "$SEND_RES" | jq -r '.packetId')
CIPHERTEXT=$(echo "$SEND_RES" | jq -r '.fullCiphertext')

# 2. Construct the JSON payload for /bridge/ingest
PAYLOAD=$(jq -n \
  --arg pid "$PACKET_ID" \
  --arg ctx "$CIPHERTEXT" \
  '{packetId: $pid, ttl: 5, createdAt: 1000, ciphertext: $ctx}')

# 3. Fire 50 concurrent requests in the background
echo "Firing 50 concurrent requests..."
for i in {1..50}; do
  curl -s -X POST "$BASE_URL/bridge/ingest" \
    -H "Content-Type: application/json" \
    -H "X-Bridge-Node-Id: test-bridge" \
    -H "X-Hop-Count: 0" \
    -d "$PAYLOAD" &
done

# Wait for all background curls to finish
wait

echo -e "\n\nAll requests completed. Let's check the Transactions table:"
curl -s -X GET "$BASE_URL/transactions" | jq
