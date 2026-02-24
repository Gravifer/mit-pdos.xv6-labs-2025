#!/usr/bin/env bash
set -euo pipefail

# Boot xv6 in QEMU, run a shell command, quit QEMU, and extract key output.
# Usage:
#   scripts/xv6_run_and_extract.sh [xv6_command] [log_file] [--addr2line]
# Example:
#   scripts/xv6_run_and_extract.sh bttest /tmp/xv6-bttest.log

RESOLVE_ADDR2LINE=0
POSITIONAL=()
for arg in "$@"; do
  case "$arg" in
    --addr2line)
      RESOLVE_ADDR2LINE=1
      ;;
    *)
      POSITIONAL+=("$arg")
      ;;
  esac
done

XV6_CMD="${POSITIONAL[0]:-bttest}"
LOG_FILE="${POSITIONAL[1]:-/tmp/xv6-session.log}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export XV6_CMD

if ! command -v expect >/dev/null 2>&1; then
  echo "error: 'expect' is required (install package: expect)" >&2
  exit 1
fi

cd "$ROOT_DIR"

if ! expect <<'EOF' >"$LOG_FILE" 2>&1
  set timeout 180
  set cmd $env(XV6_CMD)

  spawn -noecho make qemu

  expect {
    -re {init: starting sh} {}
    timeout { puts "timed out waiting for xv6 shell startup"; exit 1 }
    eof { puts "qemu exited before shell startup"; exit 1 }
  }

  expect {
    -re {\$ $} {}
    timeout { puts "timed out waiting for xv6 prompt"; exit 1 }
    eof { puts "qemu exited before prompt"; exit 1 }
  }

  send -- "$cmd\r"

  expect {
    -re {\$ $} {}
    timeout { puts "timed out waiting for prompt after command"; exit 1 }
    eof { puts "qemu exited before command completed"; exit 1 }
  }

  send -- "\001x"
  expect eof
EOF
then
  echo "error: xv6 automation failed; last log lines:" >&2
  tail -n 60 "$LOG_FILE" >&2 || true
  exit 1
fi

echo "saved full session log: $LOG_FILE"
echo
echo "---- extracted: backtrace block ----"
awk '
  /backtrace:/{
    print;
    n=0;
    while (n < 20 && getline > 0) {
      if ($0 ~ /^\$/) break;
      print;
      n++;
    }
    exit;
  }
' "$LOG_FILE"

echo
echo "---- extracted: address lines (normalized) ----"
mapfile -t ADDRS < <(
  grep -Eo '0x0x[0-9a-fA-F]+|0x[0-9a-fA-F]+' "$LOG_FILE" 2>/dev/null \
    | sed 's/^0x0x/0x/' \
    | awk '!seen[$0]++'
)

if [[ ${#ADDRS[@]} -eq 0 ]]; then
  echo "(no addresses found)"
else
  printf '%s\n' "${ADDRS[@]}"
fi

echo
echo "Tip: resolve addresses with:"
echo "  riscv64-unknown-elf-addr2line -e kernel/kernel -f -a <addr1> <addr2> ..."

if [[ "$RESOLVE_ADDR2LINE" -eq 1 ]]; then
  echo
  echo "---- addr2line: saved return addresses ----"
  if ! command -v riscv64-unknown-elf-addr2line >/dev/null 2>&1; then
    echo "error: riscv64-unknown-elf-addr2line not found" >&2
    exit 1
  fi
  if [[ ! -f "kernel/kernel" ]]; then
    echo "error: kernel/kernel not found (build first with make kernel/kernel)" >&2
    exit 1
  fi
  if [[ ${#ADDRS[@]} -eq 0 ]]; then
    echo "(skipped: no addresses to resolve)"
    exit 0
  fi

  riscv64-unknown-elf-addr2line -e kernel/kernel -f -a "${ADDRS[@]}"

  echo
  echo "---- addr2line: callsites (ra-4) ----"
  CALLSITES=()
  for addr in "${ADDRS[@]}"; do
    if [[ "$addr" =~ ^0x[0-9a-fA-F]+$ ]]; then
      hex=${addr#0x}
      val=$((16#$hex))
      if (( val >= 4 )); then
        CALLSITES+=("$(printf '0x%016x' "$((val - 4))")")
      fi
    fi
  done

  if [[ ${#CALLSITES[@]} -eq 0 ]]; then
    echo "(no callsite addresses computed)"
  else
    riscv64-unknown-elf-addr2line -e kernel/kernel -f -a "${CALLSITES[@]}"
  fi
fi
