#!/usr/bin/env bash
set -euo pipefail

# ===================== Config =====================
HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-65335}"
PASS="${PASS:-hunter2}"
SERVER_BIN="${SERVER_BIN:-./ircserv}"
AUTO_LAUNCH="${AUTO_LAUNCH:-1}"
KILL_OLD="${KILL_OLD:-1}"

TMPDIR="$(mktemp -d /tmp/irc_test.XXXXXX)"
SERVER_LOG="$TMPDIR/server.log"
echo "Logs in: $TMPDIR"
# ==================================================

declare -A IN OUT PID FDW

cleanup() {
  # kill server first
  if [[ "${PID[SERVER]:-}" != "" ]]; then
    kill "${PID[SERVER]}" 2>/dev/null || true
    wait "${PID[SERVER]}" 2>/dev/null || true
  fi
  # close writer FDs and kill clients
  for n in "${!FDW[@]}"; do
    eval "exec ${FDW[$n]}>&-"
  done
  for n in "${!PID[@]}"; do
    [[ "$n" == "SERVER" ]] && continue
    kill "${PID[$n]}" 2>/dev/null || true
  done
  # remove fifos
  for n in "${!IN[@]}"; do
    [[ -p "${IN[$n]}" ]] && rm -f "${IN[$n]}" || true
  done
  echo "Cleaning $TMPDIR"
}
trap cleanup EXIT

is_port_in_use() {
  local p="$1"
  if command -v ss >/dev/null 2>&1; then
    ss -ltn | awk '{print $4}' | grep -qE "[:.]${p}$"
  else
    netstat -an | grep -E "LISTEN|LISTENING" | grep -q "[\.\:]${p} "
  fi
}

choose_free_port() {
  if [[ -z "${PORT:-}" || "${PORT}" == "auto" ]]; then
    for p in 65335 6667 50000 50001 50002; do
      if ! is_port_in_use "$p"; then PORT="$p"; return; fi
    done
    for p in $(seq 40000 65535); do
      if ! is_port_in_use "$p"; then PORT="$p"; return; fi
    done
    echo "No free port found"; exit 1
  fi
}

check_port() { (exec 3<>"/dev/tcp/${HOST}/${PORT}") >/dev/null 2>&1; }

wait_for_port() {
  local timeout="${1:-10}"
  local end=$(( $(date +%s) + timeout ))
  while [[ $(date +%s) -le $end ]]; do
    if check_port; then return 0; fi
    sleep 0.05
  done
  return 1
}

launch_server() {
  echo "Starting server: ${SERVER_BIN} ${PORT} ${PASS}"
  "${SERVER_BIN}" "${PORT}" "${PASS}" >"$SERVER_LOG" 2>&1 &
  PID[SERVER]=$!
  if ! wait_for_port 10; then
    echo "Server did not open ${HOST}:${PORT}."
    echo "---- server log (tail) ----"
    tail -n 200 "$SERVER_LOG" || true
    exit 1
  fi
}

# ===== client management using persistent FIFO writer FDs =====
start_client() {
  local name="$1"
  IN["$name"]="$TMPDIR/${name}.in"
  OUT["$name"]="$TMPDIR/${name}.log"
  mkfifo "${IN[$name]}"
  : > "${OUT[$name]}"
  # Open a persistent writer FD to unblock FIFO reads
  # Store fd number in FDW[name]
  exec {fd}>"${IN[$name]}"
  FDW["$name"]=$fd
  # Now nc can open the FIFO for reading immediately
  ( nc "$HOST" "$PORT" <"${IN[$name]}" >"${OUT[$name]}" ) &
  PID["$name"]=$!
  sleep 0.15
}

send() {
  local name="$1"; shift
  local line="$*"
  local fd="${FDW[$name]}"
  # write via the persistent FD (prevents FIFO re-open races)
  printf "%s\r\n" "$line" >&$fd
}

sleep_until() {
  local file="$1" pat="$2" t="${3:-4}"
  local end=$(( $(date +%s) + t ))
  while [[ $(date +%s) -le $end ]]; do
    if grep -E -q "$pat" "$file"; then return 0; fi
    sleep 0.05
  done
  return 1
}

expect() {
  local name="$1" pat="$2" t="${3:-4}"
  if sleep_until "${OUT[$name]}" "$pat" "$t"; then
    echo "[PASS] $name ~ $pat"
  else
    echo "----- ${name} OUTPUT (tail) -----"
    tail -n 100 "${OUT[$name]}" || true
    echo "----- server.log (tail) -----"
    tail -n 100 "$SERVER_LOG" || true
    echo "[FAIL] $name ~ $pat"
    exit 1
  fi
}

section() { echo; echo "===== $* ====="; }

register() {
  local name="$1" nick="$2" user="$3" real="$4"
  send "$name" "PASS $PASS"
  expect "$name" "NOTICE .* :Password accepted" 4
  send "$name" "NICK $nick"
  expect "$name" "NOTICE $nick :Your nickname is now" 4
  send "$name" "USER $user 0 \* :$real"
  expect "$name" " 001 $nick :Welcome to ft_irc " 5
}

# =================== MAIN ===================

choose_free_port
if [[ "$AUTO_LAUNCH" == "1" ]]; then
  if is_port_in_use "$PORT"; then
    if [[ "$KILL_OLD" == "1" ]]; then
      echo "Port $PORT busy; killing old ircserv..."
      fuser -k "${PORT}"/tcp 2>/dev/null || true
      pkill -x ircserv 2>/dev/null || true
      sleep 0.2
    else
      echo "Port $PORT busy; choosing another..."
      PORT="auto"; choose_free_port
    fi
  fi
  echo "Using PORT=$PORT"
  launch_server
else
  echo "Using already-running server on ${HOST}:${PORT}"
  if ! wait_for_port 5; then
    echo "No server listening on ${HOST}:${PORT}."; exit 1
  fi
fi

# ============== TESTS (same assertions as before) ==============
section "Smoke: three clients online"
start_client A; start_client B; start_client C
# server welcome on connect
expect A "NOTICE \* :Welcome to ft_irc" 5
expect B "NOTICE \* :Welcome to ft_irc" 5
expect C "NOTICE \* :Welcome to ft_irc" 5

section "1) AUTH order enforced"
send A "NICK Alice"
expect A "451 NICK :You have not registered \(PASS first\)"
send A "USER a 0 \* :Alice A"
expect A "451 USER :You have not registered \(PASS first\)"
send A "PASS $PASS"
expect A "NOTICE \* :Password accepted" 4
send A "USER a 0 \* :Alice A"
expect A "451 USER :You have not registered \(NICK before USER\)"
send A "NICK Alice"
expect A "NOTICE Alice :Your nickname is now"
send A "USER a 0 \* :Alice A"
expect A " 001 Alice :Welcome to ft_irc "

section "Register Bob & Admin"
register B "Bob" "b" "Bobby"
register C "admin" "c" "SiteOperator"

section "2) Bot greets on new channel"
send A "JOIN #zoo"
expect A ":helperbot JOIN #zoo"
expect A "PRIVMSG #zoo :hi, I.?m helperbot|PRIVMSG #zoo :hi, I'm helperbot"

section "3) PRIVMSG trailing handling"
send A "PRIVMSG #zoo :hello there!"
expect A "PRIVMSG #zoo :hello there!"
send A "PRIVMSG #zoo hello there"
if sleep_until "${OUT[A]}" " 412 .* :No text to send .* ':'" 2; then
  echo "[PASS] strict PRIVMSG multi-word requires ':'"
else
  expect A "PRIVMSG #zoo :hello" 2
  echo "[WARN] permissive PRIVMSG (sends first word); to enforce strict, see cmdPRIVMSG note."
fi

section "4) MODE +o/-o broadcast format (HexChat compatibility)"
send C "PRIVMSG #zoo :!op Bob"
expect A ":admin MODE #zoo \+o Bob" 4
expect B ":admin MODE #zoo \+o Bob" 4
send C "PRIVMSG #zoo :!kick Bob :testing kick via bot"
expect A ":admin KICK #zoo Bob :testing kick via bot" 4

section "5) Rejoin: op should NOT persist"
send B "JOIN #zoo"
expect B " JOIN #zoo"
expect B " 353 Bob = #zoo :" 4
if grep -Eq "@Bob" "${OUT[B]}"; then
  echo "[FAIL] Bob appears as @Bob after rejoin (op persisted)"; exit 1
else
  echo "[PASS] Bob rejoined without op"
fi

section "6) Auto-promote an op if none left"
send C "MODE #zoo -o admin"
send C "MODE #zoo -o Alice"
send C "MODE #zoo -o Bob"
expect A ":ircserv MODE #zoo \+o " 6

section "7) +l below current members rejected"
send B "JOIN #zoo"
send C "JOIN #zoo"
send C "MODE #zoo +l 2"
expect C " 471 #zoo :Cannot set limit below current members"

section "8) Channel is deleted when empty"
send A "JOIN #gone"
expect A " JOIN #gone"
send B "PRIVMSG #gone :hey"
expect B " 442 .* #gone :You're not on that channel"
send A "PART #gone"
expect A " PART #gone"
send B "PRIVMSG #gone :hey again"
expect B " 403 .* #gone :No such channel"

section "9) Bot commands"
send A "PRIVMSG #zoo :!help"
expect A "PRIVMSG #zoo :!ping \| !echo <text> \| !topic <text> \| !op <nick> \| !kick <nick> \[reason\]"
send A "PRIVMSG #zoo :!ping"
expect A "PRIVMSG #zoo :pong"
send A "PRIVMSG #zoo :!echo hello-bot"
expect A "PRIVMSG #zoo :hello-bot"
send C "PRIVMSG #zoo :!topic test topic set via bot"
expect A ":admin TOPIC #zoo :test topic set via bot"

section "10) File transfer: happy path"
send A "FILESEND Bob 11 :demo.txt"
expect A " 739 Alice Bob [0-9]+ 11 :demo.txt"
expect B " 738 Alice [0-9]+ 11 :demo.txt"
TID="$(grep -Eo ' 738 Alice ([0-9]+) ' "${OUT[B]}" | awk '{print $3}' | tail -n1)"
[[ -n "$TID" ]] || { echo "[FAIL] could not parse transfer id"; exit 1; }
send B "FILEACCEPT $TID"
expect B " 742 \* $TID :ACCEPTED"
send A "FILEDATA $TID SGVsbG8gV29ybGQ="
expect B " 740 \* SGVsbG8gV29ybGQ="
send A "FILEDONE $TID"
expect A " 741 \* demo.txt :FILE DONE"
expect B " 741 \* demo.txt :FILE DONE"

section "11) File transfer: error flows"
send A "FILESEND Bob 5 :x.bin"
expect B " 738 Alice [0-9]+ 5 :x.bin"
TID2="$(grep -Eo ' 738 Alice ([0-9]+) ' "${OUT[B]}" | awk '{print $3}' | tail -n1)"
send A "FILEDATA $TID2 AAA="
expect A " 400 $TID2 :Transfer not accepted yet"
send B "FILECANCEL $TID2"
expect B " 743 \* $TID2 :Receiver cancelled"
send A "FILEDATA $TID2 AAA="
expect A " 400 $TID2 :Transfer not active"

echo
echo "🎉 All tests passed. Logs: $TMPDIR"
