#!/bin/bash
BUILD_DIR=$1
shift

# Start the server in the background
"$BUILD_DIR/drogonserver" > "$BUILD_DIR/server_test.log" 2>&1 &
SERVER_PID=$!

# Wait for the server to bind to port 8080
sleep 2

# Execute the actual test command
"$@"
TEST_EXIT_CODE=$?

# Gracefully shutdown the server
kill -INT $SERVER_PID
wait $SERVER_PID 2>/dev/null || true

exit $TEST_EXIT_CODE
