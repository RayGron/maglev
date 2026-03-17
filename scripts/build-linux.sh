#!/usr/bin/env bash

set -euo pipefail

profile="${1:-debug}"
root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export CARGO_TARGET_DIR="${root_dir}/target/linux-x64"

case "${profile}" in
  debug)
    cargo build
    ;;
  release)
    cargo build --release
    ;;
  *)
    echo "Usage: $0 <debug|release>" >&2
    exit 1
    ;;
esac
