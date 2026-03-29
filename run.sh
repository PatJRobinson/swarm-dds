#!/usr/bin/env bash
set -euo pipefail

pids=()

cleanup() {
  for pid in "${pids[@]:-}"; do
    kill "$pid" 2>/dev/null || true
  done
  wait 2>/dev/null || true
}

trap cleanup EXIT INT TERM

./bin/viewer &
pids+=($!)
sleep 1

./bin/agent 1 -8 -3 &
pids+=($!)

./bin/agent 2 8 -3 &
pids+=($!)

./bin/agent 3 0 7 &
pids+=($!)

./bin/agent 4 -3 0 &
pids+=($!)

./bin/agent 5 3 0 &
pids+=($!)

wait
