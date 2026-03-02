#!/usr/bin/env bash
# Test Suite for the Web Server

SCRIPT="./server"
PASS=0
FAIL=0

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

terminate_server() {
    pkill -INT -P $1 server 2>/dev/null
    kill -INT $1 2>/dev/null
    
    for i in {1..10}; do
        kill -0 $1 2>/dev/null || break
        sleep 0.2
    done

    pkill -9 server 2>/dev/null
    wait $1 2>/dev/null
}

assert_success() {
 if [ $? -eq 0 ]; then
 echo -e "${GREEN}✓ PASS${NC}: $1"
 ((PASS++))
 else
 echo -e "${RED}✗ FAIL${NC}: $1"
 ((FAIL++))
 fi
}

assert_fail() {
 if [ $? -ne 0 ]; then
 echo -e "${GREEN}✓ PASS${NC}: $1"
 ((PASS++))
 else
 echo -e "${RED}✗ FAIL${NC}: $1"
 ((FAIL++))
 fi
}

test_initialization() {
 echo "=== Test: Initialization ==="

 (cd .. && timeout 1s "$SCRIPT" 2>&1) | grep -q "Error"
 
 assert_fail "Server initializes correctly"
}

test_get() {
 echo "=== Test: GET requests for various file type And Content-Type ==="

 cd .. && "$SCRIPT" > "/dev/null" 2>&1 &
 PID=$!

 curl -k -I "https://localhost:8080/miau.png" 2>&1 | grep -q "200 OK" && curl -k -I "https://localhost:8080/miau.png" 2>&1 | grep -q "image/png"
 assert_success "GET and Content-Type works for PNG"

 curl -k -I "https://localhost:8080/lusiadas.pdf" 2>&1 | grep -q "200 OK" && curl -k -I "https://localhost:8080/lusiadas.pdf" 2>&1 | grep -q "application/pdf"
 assert_success "GET and Content-Type works for PDF"
 curl -k -I "https://localhost:8080/" 2>&1 | grep -q "200 OK" && curl -k -I "https://localhost:8080/" 2>&1 | grep -q "text/html"
 assert_success "GET and Content-Type works for HTML"
 curl -k -I "https://localhost:8080/errors/internalservererror.js" 2>&1 | grep -q "200 OK" && curl -k -I "https://localhost:8080/errors/internalservererror.js" 2>&1 | grep -q "application/javascript"
 assert_success "GET and Content-Type works for JS"
 curl -k -I "https://localhost:8080/errors/internalservererror.css" 2>&1 | grep -q "200 OK" && curl -k -I "https://localhost:8080/errors/internalservererror.css" 2>&1 | grep -q "text/css"
 assert_success "GET and Content-Type works for CSS"

 terminate_server $PID
}

test_status() {
 echo "=== Test: Verify correct HTTP status codes ==="

 cd .. && "$SCRIPT" > "/dev/null" 2>&1 &
 PID=$!

 curl -k -I "https://localhost:8080/" 2>&1 | grep -q "200 OK"
 assert_success "Status 200"
 curl -k -I "https://localhost:8080/status404" 2>&1 | grep -q "404 Not Found"
 assert_success "Status 404"
 curl -k -I "https://localhost:8080/admin" 2>&1 | grep -q "401 Unauthorized"
 assert_success "Status 401"
 
 echo "test" > "../www/forbidden_test.txt"
 chmod 000 "../www/forbidden_test.txt"
 curl -k -I "https://localhost:8080/forbidden_test.txt" 2>&1 | grep -q "403 Forbidden"
 assert_success "Status 403"
 rm -f "../www/forbidden_test.txt"
 
 mv "../www/errors/notfound.html" "../www/notfound.html"
 curl -k -I "https://localhost:8080/status404" 2>&1 | grep -q "500 Internal Server Error"
 mv "../www/notfound.html" "../www/errors/notfound.html"
 assert_success "Status 500"

 if command -v ab &> /dev/null; then
   ab -n 1000 -c 250 -k https://localhost:8080/lusiadas.pdf > /dev/null 2>&1 &
   AB_PID=$!
   sleep 0.3
   for i in {1..5}; do
     curl -k -I "https://localhost:8080/" 2>&1 | grep -q "503 Service Unavailable" && break
     sleep 0.1
   done
   
   assert_success "Status 503"
   wait $AB_PID 2>/dev/null
 else
   echo "⚠ SKIPPING: Status 503 (ab not available for load testing)"
 fi

 terminate_server $PID
}

test_directory_index() {
 echo "=== Test: Directory index serving ==="

 cd .. && "$SCRIPT" > "/dev/null" 2>&1 &
 PID=$!

 TEST_DIR="../www/directory_test"
 mkdir -p "$TEST_DIR"
 echo "Sensitive data!" > "$TEST_DIR/secret_config.txt"
 echo "Just a picture" > "$TEST_DIR/my_photo.jpg"

 curl -k -s "https://localhost:8080/directory_test/" | grep -q "secret_config.txt"
 assert_fail "Directory listing"

 rm -rf "$TEST_DIR"

 terminate_server $PID
}

test_apache_bench() {
 echo "=== Test: Concurrent requests ==="

 cd .. && "$SCRIPT" > "/dev/null" 2>&1 &
 PID=$!

 if command -v ab &> /dev/null; then
  echo "Testing concurrent requests! (This may take a while...)"
  ab -n 10000 -c 100 -k "https://localhost:8080/" | grep -q "Failed requests:        0"
  assert_success "No Concurrent Requests Dropped"

 else
  echo "⚠ SKIPPING: Please install apachebench!"
 fi

 curl -k -s "https://localhost:8080/api/stats" | grep -q "\"status_200\":10000"
 assert_success "Statistics accuracy under concurrent load"

 terminate_server $PID
}

test_parallel_clients() {
 echo "=== Test: Multiple parallel curl clients ==="

 cd .. && "$SCRIPT" > "/dev/null" 2>&1 &
 PID=$!
 sleep 1 

 NUM_CLIENTS=20
 TEMP_DIR=$(mktemp -d)
 PIDS=()
 
 for i in $(seq 1 $NUM_CLIENTS); do
   (
     if curl -k -I "https://localhost:8080/" 2>/dev/null | grep -q "200"; then
       echo "success" > "$TEMP_DIR/result_$i"
     fi
   ) &
   PIDS+=($!)
 done

 for pid in "${PIDS[@]}"; do
   wait "$pid" 2>/dev/null
 done
 
 SUCCESS_COUNT=$(grep -l "success" "$TEMP_DIR"/result_* 2>/dev/null | wc -l)
 
 rm -rf "$TEMP_DIR"
 
 [ $SUCCESS_COUNT -eq $NUM_CLIENTS ]
 assert_success "Parallel clients ($SUCCESS_COUNT/$NUM_CLIENTS succeeded)"

 terminate_server $PID
}

test_helgrind() {
 echo "=== Test: Helgrind ==="

 if command -v valgrind &> /dev/null; then
  HELGRIND_OUTPUT=$(cd .. && timeout --signal=INT 2s valgrind --tool=helgrind "$SCRIPT" 2>&1)
  echo "$HELGRIND_OUTPUT" | grep -q "LEAK SUMMARY:"
  assert_fail "Helgrind"
 else
  echo "Please install valgrind for testing/debugging purposes!"
 fi
}

test_log_integrity() {
 echo "=== Test: Log file integrity (no interleaved entries) ===" # This test was generated with the help of Gemini

 cd .. && rm -f access.log server.log 2>/dev/null
 
 "$SCRIPT" > "/dev/null" 2>&1 &
 PID=$!
 sleep 1

 NUM_REQUESTS=50
 PIDS=()
 for i in $(seq 1 $NUM_REQUESTS); do
   curl -k -s "https://localhost:8080/index.html" > /dev/null 2>&1 &
   PIDS+=($!)
 done

 for pid in "${PIDS[@]}"; do
   wait "$pid" 2>/dev/null
 done

 sleep 1
 terminate_server $PID

 if [ -f ./access.log ]; then
   COMPLETE_LINES=$(grep -E '^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3} - - \[[0-9]{2}/[A-Za-z]{3}/[0-9]{4}:[0-9]{2}:[0-9]{2}:[0-9]{2} [+-][0-9]{4}\] "GET .* HTTP/[0-9]\.[0-9]" [0-9]{3} [0-9]+$' ./access.log 2>/dev/null | wc -l)
   TOTAL_LINES=$(grep -v -E "(Server starting|Server closing)" ./access.log 2>/dev/null | wc -l)

   [ "$COMPLETE_LINES" -eq "$TOTAL_LINES" ] && [ "$TOTAL_LINES" -gt 0 ]
   assert_success "Log entries are not interleaved ($COMPLETE_LINES/$TOTAL_LINES complete)"
 else
   echo "⚠ SKIPPING: access.log not found"
 fi
}

test_cache_consistency() {
 echo "=== Test: Cache consistency across threads ===" # This test was generated with the help of Gemini

 cd .. && "$SCRIPT" > "/dev/null" 2>&1 &
 PID=$!
 sleep 1

 NUM_CLIENTS=30
 TEMP_DIR=$(mktemp -d)
 PIDS=()
 
 for i in $(seq 1 $NUM_CLIENTS); do
   (
     CHECKSUM=$(curl -k -s "https://localhost:8080/lusiadas.pdf" 2>/dev/null | md5sum | cut -d' ' -f1)
     echo "$CHECKSUM" > "$TEMP_DIR/checksum_$i"
   ) &
   PIDS+=($!)
 done

 for pid in "${PIDS[@]}"; do
   wait "$pid" 2>/dev/null
 done

 UNIQUE_CHECKSUMS=$(cat "$TEMP_DIR"/checksum_* 2>/dev/null | sort -u | wc -l)
 TOTAL_CHECKSUMS=$(cat "$TEMP_DIR"/checksum_* 2>/dev/null | wc -l)
 
 rm -rf "$TEMP_DIR"
 terminate_server $PID

 [ "$UNIQUE_CHECKSUMS" -eq 1 ] && [ "$TOTAL_CHECKSUMS" -eq "$NUM_CLIENTS" ]
 assert_success "Cache consistent across threads ($TOTAL_CHECKSUMS requests, $UNIQUE_CHECKSUMS unique content)"
}

test_valgrind() {
 echo "=== Test: Valgrind ==="

 if command -v valgrind &> /dev/null; then
  HELGRIND_OUTPUT=$(cd .. && timeout --signal=INT 2s valgrind "$SCRIPT" 2>&1)
  echo "$HELGRIND_OUTPUT" | grep -q "LEAK SUMMARY:"
  assert_fail "Valgrind"
 else
  echo "Please install valgrind for testing/debugging purposes!"
 fi
}

test_valgrind() {
 echo "=== Test: Valgrind ==="

 if command -v valgrind &> /dev/null; then
  HELGRIND_OUTPUT=$(cd .. && timeout --signal=INT 2s valgrind "$SCRIPT" 2>&1)
  echo "$HELGRIND_OUTPUT" | grep -q "LEAK SUMMARY:"
  assert_fail "Valgrind"
 else
  echo "Please install valgrind for testing/debugging purposes!"
 fi
}

test_graceful_shutdown() {
 echo "=== Test: Graceful Shutdown ==="
 
 TEMP_LOG=$(mktemp)
 "$SCRIPT" > "$TEMP_LOG" 2>&1 &
 PID=$!
 sleep 1
 
 kill -INT $PID 2>/dev/null
 wait $PID 2>/dev/null
 
 grep -q "Server successfully closed" "$TEMP_LOG"
 assert_success "Graceful shutdown"
 
 rm -f "$TEMP_LOG"
 cd tests
}

test_zombie_proc() {
 echo "=== Test: Zombie Processes ==="
 
 cd .. && "$SCRIPT" > "/dev/null" 2>&1 &
 PID=$!
 sleep 1

 terminate_server $PID
 sleep 1

 ZOMBIE_COUNT=$(ps --ppid $PID -o stat= 2>/dev/null | grep 'Z' | wc -l)
 [ "$ZOMBIE_COUNT" -eq 0 ]
 assert_success "No Zombie processes ($ZOMBIE_COUNT found)"
}

test_continuous_load() {
 echo "=== Test: Continuous Load (5+ minutes) ==="
 
 if ! command -v ab &> /dev/null; then
   echo "⚠ SKIPPING: Apache Bench not available"
   return
 fi
 
 echo "⚠ WARNING: This test will run for 5 minutes with continuous load."
 read -p "Do you want to continue? (y/N): " -n 1 -r
 echo ""
 
 if [[ ! $REPLY =~ ^[Yy]$ ]]; then
   echo "⚠ SKIPPING: Continuous load test"
   return
 fi
 
 echo "Starting continuous load test at $(date)"
 echo "Running Apache Bench for 5 minutes with 50 concurrent connections..."
 
 cd .. && "$SCRIPT" > "/dev/null" 2>&1 &
 PID=$!
 sleep 2

 START_TIME=$(date +%s)
 TOTAL_REQUESTS=0
 TOTAL_FAILED=0
 
 # Run Apache Bench in a loop for 5 minutes
 while [ $(($(date +%s) - START_TIME)) -lt 300 ]; do
   ab -n 10000 -c 50 -k "https://localhost:8080/index.html" > test.log 2>&1
   
   REQUESTS=$(grep "Complete requests:" test.log | awk '{print $3}')
   FAILED=$(grep "Failed requests:" test.log | awk '{print $3}')
   
   TOTAL_REQUESTS=$((TOTAL_REQUESTS + REQUESTS))
   TOTAL_FAILED=$((TOTAL_FAILED + FAILED))
 done
 
 END_TIME=$(date +%s)
 DURATION=$((END_TIME - START_TIME))
 
 echo "Continuous load test completed at $(date)"
 echo "Actual duration: ${DURATION} seconds"
 echo "Total requests: ${TOTAL_REQUESTS}"
 echo "Failed requests: ${TOTAL_FAILED}"
 
 terminate_server $PID
 rm -f test.log
 
 [ "$TOTAL_FAILED" -eq 0 ]
 assert_success "Continuous load (${TOTAL_REQUESTS} requests, ${TOTAL_FAILED} failed, ${DURATION}s)"

}

# First let's ensure server was not already running
pkill -9 server 2>/dev/null
sleep 1 
test_initialization
test_get
test_status
test_directory_index
test_apache_bench
test_parallel_clients
test_log_integrity
test_cache_consistency
test_helgrind
test_valgrind
test_graceful_shutdown
test_zombie_proc
test_continuous_load

echo "========================================="
echo "Results: $PASS passed, $FAIL failed"
echo "========================================="
[ $FAIL -eq 0 ] && exit 0 || exit 1
