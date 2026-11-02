#!/usr/bin/env bash
make threaded-server async-server

taskset -c 0-7 ./build/bin/async_server &
pid=$!
sleep 1
taskset -c 8 ab -n 100000 -c 100 http://127.0.0.1:8080/hello/world | tee coroed.txt
kill "$pid"
wait "$pid" 2>/dev/null || true

taskset -c 0-7 ./build/bin/threaded_server &
pid=$!
sleep 1
taskset -c 8 ab -n 100000 -c 100 http://127.0.0.1:8080/hello/world | tee threads.txt
kill "$pid"
wait "$pid" 2>/dev/null || true
