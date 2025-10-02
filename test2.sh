#!/usr/bin/env bash
set -euo pipefail

# ===================== Config =====================
HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-65335}"
PASS="${PASS:-hunter2}"
SERVER_BIN="${SERVER_BIN:-./ircserv}"
AUTO_LAUNCH="${AUTO_LAUNCH:-1}"   # 1=launch server; 0=use existing
KILL_OLD="${KILL_OLD:-1}"         # 1=kill old ircserv bound to PORT if needed
TIMEOUT_DEFAULT=8
# ==================================================

# ---- require bash >= 4.2 (named coprocesses) ----
if [[ -z "${BASH_VERSINFO:-}" || ${BASH_VERSINFO[0]} -lt 4 || ( ${BASH_VERSINFO[0]} -eq 4 && ${BASH_VERSINFO[1]} -lt 2 ) ]]; then
  echo "This script needs bash >= 4.2. Run with: env bash ./test2.sh"
  exit 1
fi

# ---- Colors ----
RESET="$(printf '\033[0m')"
BOLD="$(printf '\033[1m')"
GREEN="$(printf '\033[32m')"
RED="$(printf '\033[31m')"
YELLOW="$(printf '\033[33m')"
CYAN="$(printf '\033[36m')"
DIM="$(printf '\033[2m')"

# ---- Paths / logs ----
TMPDIR="$(mktemp -d /tmp/irc_test.XXXXXX)"
SERVER_LOG="$TMPDIR/server.log"
echo "Logs in: $TMPDIR"

# ---- Section counters ----
SECTIONS_TOTAL=0
SECTIONS_PASS=0
SECTIONS_FAIL=0
SEC_NAME=""
SEC_FAILED=0

# ========== cleanup ==========
cleanup() {
  # stop readers first
  for p in ${READERA:-} ${READERB:-} ${READERC:-}; do [[ -n "${p:-}" ]] && kill "$p" 2>/dev/null || true; done
  # close write fds
  for fd in ${A_WFD:-} ${B_WFD:-} ${C_WFD:-}; do [[ -n "${fd:-}" ]] && exec {fd}>&- 2>/dev/null || true; done
  # close read fds
  for fd in ${A_RFD:-} ${B_RFD:-} ${C_RFD:-}; do [[ -n "${fd:-}" ]] && exec {fd}<&- 2>/dev/null || true; done
  # kill coprocess PIDs
  for p in ${APROC_PID:-} ${BPROC_PID:-} ${CPROC_PID:-}; do [[ -n "${p:-}" ]] && kill "$p" 2>/dev/null || true; done
  # kill server last
  if [[ -n "${SERVER_PID:-}" ]]; then
    kill "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
  echo "Cleaning $TMPDIR"
}
trap cleanup EXIT

# ---------- helpers ----------
is_port_in_use() {
  local p="$1"
  if command -v ss >/dev/null 2>&1; then
    ss -ltn | awk '{print $4}' | grep -qE "[:.]${p}$"
  else
    netstat -an 2>/dev/null | grep -E "LISTEN|LISTENING" | grep -q "[\.\:]${p} "
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
  SERVER_PID=$!
  if ! wait_for_port 10; then
    echo "Server did not open ${HOST}:${PORT}."
    echo "---- server log (tail) ----"
    tail -n 200 "$SERVER_LOG" || true
    exit 1
  fi
}

sleep_until() {
  local file="$1" pat="$2" t="${3:-$TIMEOUT_DEFAULT}"
  local end=$(( $(date +%s) + t ))
  while [[ $(date +%s) -le $end ]]; do
    if grep -E -q "$pat" "$file"; then return 0; fi
    sleep 0.05
  done
  return 1
}

pass()  { echo -e "${GREEN}[PASS]${RESET} $*"; }
fail()  { echo -e "${RED}[FAIL]${RESET}  $*"; SEC_FAILED=1; }
warn()  { echo -e "${YELLOW}[WARN]${RESET}  $*"; }

expect() {
  local who="$1" file="$2" pat="$3" t="${4:-$TIMEOUT_DEFAULT}"
  if sleep_until "$file" "$pat" "$t"; then
    pass "$who ~ $pat"
  else
    echo -e "${DIM}----- ${who} OUTPUT (tail) -----${RESET}"
    tail -n 120 "$file" || true
    echo -e "${DIM}----- server.log (tail) -----${RESET}"
    tail -n 120 "$SERVER_LOG" || true
    fail "$who ~ $pat"
  fi
}

section_begin() {
  SEC_NAME="$1"
  SEC_FAILED=0
  SECTIONS_TOTAL=$((SECTIONS_TOTAL+1))
  echo
  echo -e "${BOLD}===== $SEC_NAME =====${RESET}"
}

section_end() {
  if [[ $SEC_FAILED -eq 0 ]]; then
    SECTIONS_PASS=$((SECTIONS_PASS+1))
    echo -e "${CYAN}Section result:${RESET} ${GREEN}PASS${RESET} – $SEC_NAME"
  else
    SECTIONS_FAIL=$((SECTIONS_FAIL+1))
    echo -e "${CYAN}Section result:${RESET} ${RED}FAIL${RESET} – $SEC_NAME"
  fi
}

# ---------- start clients using NAMED coproc (no warnings) ----------
start_A() {
  A_LOG="$TMPDIR/A.log"; : >"$A_LOG"
  coproc APROC { nc "$HOST" "$PORT"; }
  exec {A_RFD}<&${APROC[0]}
  exec {A_WFD}>&${APROC[1]}
  APROC_PID=$APROC_PID
  cat <&${A_RFD} >>"$A_LOG" & READERA=$!
}
start_B() {
  B_LOG="$TMPDIR/B.log"; : >"$B_LOG"
  coproc BPROC { nc "$HOST" "$PORT"; }
  exec {B_RFD}<&${BPROC[0]}
  exec {B_WFD}>&${BPROC[1]}
  BPROC_PID=$BPROC_PID
  cat <&${B_RFD} >>"$B_LOG" & READERB=$!
}
start_C() {
  C_LOG="$TMPDIR/C.log"; : >"$C_LOG"
  coproc CPROC { nc "$HOST" "$PORT"; }
  exec {C_RFD}<&${CPROC[0]}
  exec {C_WFD}>&${CPROC[1]}
  CPROC_PID=$CPROC_PID
  cat <&${C_RFD} >>"$C_LOG" & READERC=$!
}

sendA(){ printf "%s\r\n" "$*" >&${A_WFD}; }
sendB(){ printf "%s\r\n" "$*" >&${B_WFD}; }
sendC(){ printf "%s\r\n" "$*" >&${C_WFD}; }

registerA(){ sendA "PASS $PASS"; expect "A" "$A_LOG" "NOTICE .* :Password accepted" 8; sendA "NICK $1"; expect "A" "$A_LOG" "NOTICE $1 :Your nickname is now" 8; sendA "USER $2 0 \* :$3"; expect "A" "$A_LOG" " 001 $1 :Welcome to ft_irc " 8; }
registerB(){ sendB "PASS $PASS"; expect "B" "$B_LOG" "NOTICE .* :Password accepted" 8; sendB "NICK $1"; expect "B" "$B_LOG" "NOTICE $1 :Your nickname is now" 8; sendB "USER $2 0 \* :$3"; expect "B" "$B_LOG" " 001 $1 :Welcome to ft_irc " 8; }
registerC(){ sendC "PASS $PASS"; expect "C" "$C_LOG" "NOTICE .* :Password accepted" 8; sendC "NICK $1"; expect "C" "$C_LOG" "NOTICE $1 :Your nickname is now" 8; sendC "USER $2 0 \* :$3"; expect "C" "$C_LOG" " 001 $1 :Welcome to ft_irc " 8; }

# =================== MAIN ===================

choose_free_port
echo "Using PORT=$PORT"

if [[ "$AUTO_LAUNCH" == "1" ]]; then
  if is_port_in_use "$PORT"; then
    if [[ "$KILL_OLD" == "1" ]]; then
      echo "Port $PORT busy; killing old ircserv..."
      fuser -k "${PORT}"/tcp 2>/dev/null || true
      pkill -x ircserv 2>/dev/null || true
      sleep 0.25
    else
      echo "Port $PORT busy; choosing another..."
      PORT="auto"; choose_free_port; echo "Using PORT=$PORT"
    fi
  fi
  launch_server
else
  echo "Using already-running server on ${HOST}:${PORT}"
  if ! wait_for_port 5; then echo "No server listening on ${HOST}:${PORT}."; exit 1; fi
fi

# ========== Tests ==========

section_begin "Smoke: three clients online"
start_A; start_B; start_C
expect "A" "$A_LOG" "NOTICE \* :Welcome to ft_irc" 8
expect "B" "$B_LOG" "NOTICE \* :Welcome to ft_irc" 8
expect "C" "$C_LOG" "NOTICE \* :Welcome to ft_irc" 8
section_end

section_begin "1) AUTH order enforced"
sendA "NICK Alice";         expect "A" "$A_LOG" " 451 .* NICK :You have not registered \(PASS first\)"
sendA "USER a 0 \* :Alice"; expect "A" "$A_LOG" " 451 .* USER :You have not registered \(PASS first\)"
sendA "PASS $PASS";         expect "A" "$A_LOG" "NOTICE \* :Password accepted" 6
sendA "USER a 0 \* :Alice"; expect "A" "$A_LOG" " 451 .* USER :You have not registered \(NICK before USER\)"
sendA "NICK Alice";         expect "A" "$A_LOG" "NOTICE Alice :Your nickname is now"
sendA "USER a 0 \* :Alice"; expect "A" "$A_LOG" " 001 Alice :Welcome to ft_irc "
section_end

section_begin "Register Bob & Admin"
registerB "Bob"   "b" "Bobby"
registerC "admin" "c" "SiteOperator"
section_end

section_begin "2) Bot greets on new channel (timing tolerant)"
sendA "JOIN #zoo"
# bot JOIN/greet often broadcast before first member; warn-only
if sleep_until "$A_LOG" ":helperbot JOIN #zoo" 3; then
  pass "A ~ :helperbot JOIN #zoo"
else
  warn "helperbot JOIN not observed (likely sent before first member joined)"
fi
if sleep_until "$A_LOG" "PRIVMSG #zoo :hi, I.?m helperbot|PRIVMSG #zoo :hi, I'm helperbot" 2; then
  pass "A ~ bot greeting observed"
else
  warn "bot greeting not observed (likely timing)"
fi
# Normal JOIN/NAMES from server should still appear:
expect "A" "$A_LOG" " 353 Alice = #zoo :" 6
expect "A" "$A_LOG" " 366 Alice #zoo :End of /NAMES list."
section_end

section_begin "3) PRIVMSG trailing handling (check receiver)"
# Put Bob into #zoo so he can receive messages from Alice
sendB "JOIN #zoo"
expect "B" "$B_LOG" " JOIN #zoo"
sendA "PRIVMSG #zoo :hello there!"
expect "B" "$B_LOG" "PRIVMSG #zoo :hello there!"
# Without ':' your server sends only the first word to receivers (permissive) OR rejects with 412 (strict)
sendA "PRIVMSG #zoo hello there"
if sleep_until "$A_LOG" " 412 .* :No text to send" 3; then
  pass "strict 412 for multi-word w/o ':'"
else
  expect "B" "$B_LOG" "PRIVMSG #zoo :hello" 3
  warn "permissive behavior (first word sent); stricter 412 is optional"
fi
section_end

section_begin "4) MODE +o/-o broadcast format (HexChat compatibility)"
# Make admin an operator first, then let admin use the bot to op/kick others
sendA "MODE #zoo +o admin"
expect "A" "$A_LOG" ":Alice MODE #zoo \+o admin" 6
sendC "PRIVMSG #zoo :!op Bob"
expect "A" "$A_LOG" ":admin MODE #zoo \+o Bob" 6
expect "B" "$B_LOG" ":admin MODE #zoo \+o Bob" 6
sendC "PRIVMSG #zoo :!kick Bob :testing kick via bot"
expect "A" "$A_LOG" ":admin KICK #zoo Bob :testing kick via bot" 6
section_end

section_begin "5) Rejoin: operator should NOT persist"
sendB "JOIN #zoo"
expect "B" "$B_LOG" " JOIN #zoo"
expect "B" "$B_LOG" " 353 Bob = #zoo :" 6
if grep -Eq "@Bob" "$B_LOG"; then
  fail "Bob appears as @Bob after rejoin (op persisted)"
else
  pass "Bob rejoined without op"
fi
section_end

section_begin "6) Auto-promote an op if none left (optional)"
# If you implement auto-reop, broadcast should appear; otherwise warn.
sendC "MODE #zoo -o admin"
sendA "MODE #zoo -o Alice"
sendB "MODE #zoo -o Bob"
if sleep_until "$A_LOG" ":ircserv MODE #zoo \+o " 5; then
  pass "auto-reop observed"
else
  warn "no auto-reop (not implemented) — this is optional/nice-to-have"
fi
section_end

section_begin "7) +l below current members rejected (optional)"
# Ensure >=3 members then try to set a limit below member count
sendB "JOIN #zoo"
sendC "JOIN #zoo"
sendC "MODE #zoo +l 2"
if sleep_until "$C_LOG" " 471 #zoo :Cannot set limit below current members" 3; then
  pass "471 rejection observed"
else
  warn "no 471; server accepted +l 2 with >=3 users (feature not implemented yet)"
fi
section_end

section_begin "8) Channel is deleted when empty"
sendA "JOIN #gone";                   expect "A" "$A_LOG" " JOIN #gone"
sendB "PRIVMSG #gone :hey";          expect "B" "$B_LOG" " 442 .* #gone :You're not on that channel"
sendA "PART #gone";                   # sender won't receive PART echo; that's normal
# After A leaves, either:
#  - channel deleted => Bob PRIVMSG returns 403 (preferred), or
#  - channel kept empty => Bob PRIVMSG returns 442 (not preferred)
if sleep_until "$B_LOG" " 403 .* #gone :No such channel" 3; then
  pass "channel deleted when empty (403)"
else
  if sleep_until "$B_LOG" " 442 .* #gone :You're not on that channel" 1; then
    warn "channel still exists when empty (442); consider deleting empty channels"
  else
    fail "neither 403 nor 442 observed after PART"
  fi
fi
section_end

section_begin "9) Bot commands"
sendA "PRIVMSG #zoo :!help";            expect "A" "$A_LOG" "PRIVMSG #zoo :!ping \| !echo <text> \| !topic <text> \| !op <nick> \| !kick <nick> \[reason\]"
sendA "PRIVMSG #zoo :!ping";            expect "A" "$A_LOG" "PRIVMSG #zoo :pong"
sendA "PRIVMSG #zoo :!echo hello-bot";  expect "A" "$A_LOG" "PRIVMSG #zoo :hello-bot"
sendC "PRIVMSG #zoo :!topic test topic set via bot"
expect "A" "$A_LOG" ":admin TOPIC #zoo :test topic set via bot"
section_end

section_begin "10) File transfer: happy path"
sendA "FILESEND Bob 11 :demo.txt"
expect "A" "$A_LOG" " 739 Alice Bob [0-9]+ 11 :demo.txt"
expect "B" "$B_LOG" " 738 Alice [0-9]+ 11 :demo.txt"
TID="$(grep -Eo ' 738 Alice ([0-9]+) ' "$B_LOG" | awk '{print $3}' | tail -n1)"
[[ -n "$TID" ]] || fail "could not parse transfer id"
sendB "FILEACCEPT $TID"
expect "B" "$B_LOG" " 742 \* $TID :ACCEPTED"
sendA "FILEDATA $TID SGVsbG8gV29ybGQ="
expect "B" "$B_LOG" " 740 \* SGVsbG8gV29ybGQ="
sendA "FILEDONE $TID"
expect "A" "$A_LOG" " 741 \* demo.txt :FILE DONE"
expect "B" "$B_LOG" " 741 \* demo.txt :FILE DONE"
section_end

section_begin "11) File transfer: error flows"
sendA "FILESEND Bob 5 :x.bin"
expect "B" "$B_LOG" " 738 Alice [0-9]+ 5 :x.bin"
TID2="$(grep -Eo ' 738 Alice ([0-9]+) ' "$B_LOG" | awk '{print $3}' | tail -n1)"
[[ -n "$TID2" ]] || fail "could not parse transfer id (error path)"
sendA "FILEDATA $TID2 AAA="
expect "A" "$A_LOG" " 400 .* $TID2 :Transfer not accepted yet"
sendB "FILECANCEL $TID2"
expect "B" "$B_LOG" " 743 \* $TID2 :Receiver cancelled"
sendA "FILEDATA $TID2 AAA="
expect "A" "$A_LOG" " 400 .* $TID2 :Transfer not active"
section_end

# ========== Summary ==========
echo
echo -e "${BOLD}==================== SUMMARY ====================${RESET}"
echo -e "Sections: ${BOLD}${SECTIONS_TOTAL}${RESET}   ${GREEN}Passed:${RESET} ${BOLD}${SECTIONS_PASS}${RESET}   ${RED}Failed:${RESET} ${BOLD}${SECTIONS_FAIL}${RESET}"
if [[ $SECTIONS_FAIL -eq 0 ]]; then
  echo -e "${GREEN}All sections passed!${RESET}"
else
  echo -e "${RED}Some sections failed — check the logs above and in:${RESET} ${BOLD}$TMPDIR${RESET}"
fi
echo
echo -e "Client logs:  ${CYAN}$TMPDIR/A.log${RESET}, ${CYAN}$TMPDIR/B.log${RESET}, ${CYAN}$TMPDIR/C.log${RESET}"
echo -e "Server log:   ${CYAN}$SERVER_LOG${RESET}"
