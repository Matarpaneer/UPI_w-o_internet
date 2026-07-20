#!/bin/bash
set -e

BASE_URL="http://127.0.0.1:8080/api"

echo "=== UPIMesh Drogon Backend Smoke Test ==="

echo -e "\n1. GET /server-key"
curl -s -X GET "$BASE_URL/server-key" | jq

echo -e "\n2. GET /accounts (Initial)"
curl -s -X GET "$BASE_URL/accounts" | jq

echo -e "\n3. POST /demo/send"
SEND_RES=$(curl -s -X POST "$BASE_URL/demo/send" \
  -H "Content-Type: application/json" \
  -d '{"senderVpa":"alice@demo","receiverVpa":"bob@demo","amount":100.50,"pin":"123456","ttl":5,"startDevice":"phone-alice"}')
echo "$SEND_RES" | jq

echo -e "\n4. GET /mesh/state"
curl -s -X GET "$BASE_URL/mesh/state" | jq

echo -e "\n5. POST /mesh/gossip"
curl -s -X POST "$BASE_URL/mesh/gossip" -d '' | jq

echo -e "\n6. POST /mesh/flush"
curl -s -X POST "$BASE_URL/mesh/flush" -d '' | jq

echo -e "\n7. POST /mesh/reset"
curl -s -X POST "$BASE_URL/mesh/reset" -d '' | jq

echo -e "\n8. GET /transactions"
curl -s -X GET "$BASE_URL/transactions" | jq

echo -e "\n9. GET /accounts (Final)"
curl -s -X GET "$BASE_URL/accounts" | jq

echo -e "\nSmoke test completed successfully!"
