#!/usr/bin/env bash
set -euo pipefail

for sub in vendor/hmod-build vendor/OptionsMenu; do
  [ -n "$(ls -A "$sub" 2>/dev/null)" ] || git submodule update --init --recursive "$sub"
done

sh vendor/hmod-build/docker-make.sh "$@"

ls -la out/*.hmod 2>/dev/null
