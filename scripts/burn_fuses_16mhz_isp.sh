#!/usr/bin/env bash
set -euo pipefail

PROGRAMMER="${1:-usbasp}"
PORT="${2:-}"

PORT_ARGS=()
if [[ -n "$PORT" ]]; then
  PORT_ARGS=(--port "$PORT")
fi

arduino-cli burn-bootloader \
  --fqbn attiny:avr:ATtinyX5:cpu=attiny85,clock=internal16 \
  --programmer "$PROGRAMMER" \
  "${PORT_ARGS[@]}"
