#!/usr/bin/env bash
# ft_irc integration tester — extended coverage
# Covers: PASS/NICK/USER, JOIN/PRIVMSG, MODE (i,t,k,o,l) + removals, INVITE, TOPIC,
#         KICK, wrong PASS recovery, NICK collision, pre-registration command,
#         partial packet reassembly, topic retrieval (331/332), broadcast to all.
#
# Usage: ./test.sh [host] [port]

set -Euo pipefail
trap '' PIPE

HOST="${1:-localhost}"
PORT="${2:-1234}"

# Fresh channel every run to avoid leftover modes
CHAN="#t$(date +%H%M%S)$(printf '%04x' $RANDOM)"
KEY="s3cr3t"
LIMIT=2

# Colors
if command -v tput >/dev/null 2>&1 && [ -t 1 ]; then
  GREEN="$(tput setaf 2)"; RED="$(tput setaf 1)"; YELLOW="$(tput setaf 3)"
  BOLD="$(tput bold)"; RESET="$(tput sgr0)"
else
  GREEN=""; RED=""; YELLOW=""; BOLD=""; RESET=""
fi

PASS_CNT=0
FAIL_CNT=0
RUN_DIR="$(mktemp -d /tmp/irc_test.XXXXXX)"

# Choose nc (we write CRLF ourselves)
build_nc_cmd() {
  local -a cmd=(nc)
  if nc -h 2>&1 | grep -q -- ' -w '; then cmd+=(-w 3); fi
  echo "${cmd[@]}"
}
NC_BASE=($(build_nc_cmd))

# ---------------- client mgmt ----------------
start_client() {
  local name="$1"
  local fifo="$RUN_DIR/${name}.in"
  local raw="$RUN_DIR/${name}.raw"
  local log="$RUN_DIR/${name}.log"
  local pidf="$RUN_DIR/${name}.pid"
  local fdf="$RUN_DIR/${name}.fd"

  mkfifo "$fifo"
  : > "$raw"; : > "$log"

  ("${NC_BASE[@]}" "$HOST" "$PORT" < "$fifo" >> "$raw" 2>&1) &
  local nc_pid=$!
  echo "$nc_pid" > "$pidf"

  sleep 0.15
  if ! kill -0 "$nc_pid" 2>/dev/null; then
    echo "${RED}[FATAL]${RESET} ${name}: cannot connect to ${HOST}:${PORT}"
    exit 1
  fi

  # sanitize CR
  (while :; do
     tr -d '\r' < "$raw" > "$log.tmp" 2>/dev/null || true
     mv -f "$log.tmp" "$log" 2>/dev/null || true
     sleep 0.05
   done) &
  echo $! > "$RUN_DIR/${name}.sanpid"

  # open writer FD
  exec {fd}>"$fifo"
  echo "$fd" > "$fdf"
}

send_line() { # write a full IRC line with CRLF
  local name="$1"; shift
  local fdf="$RUN_DIR/${name}.fd"; local fd=""
  [[ -f "$fdf" ]] && fd="$(cat "$fdf")" || true
  [[ -z "$fd" ]] && { echo "${YELLOW}[WARN]${RESET} $name writer missing; skip: $*"; return 0; }
  printf '%s\r\n' "$*" >&$fd 2>/dev/null || {
    echo "${YELLOW}[WARN]${RESET} $name write failed: $*"
  }
}

send_raw() {  # write bytes as-is, no CRLF
  local name="$1"; shift
  local fdf="$RUN_DIR/${name}.fd"; local fd=""
  [[ -f "$fdf" ]] && fd="$(cat "$fdf")" || true
  [[ -z "$fd" ]] && { echo "${YELLOW}[WARN]${RESET} $name writer missing; skip raw"; return 0; }
  printf '%s' "$*" >&$fd 2>/dev/null || {
    echo "${YELLOW}[WARN]${RESET} $name raw write failed"
  }
}

close_client() {
  local name="$1"
  local pidf="$RUN_DIR/${name}.pid"
  local fdf="$RUN_DIR/${name}.fd"
  local sanpidf="$RUN_DIR/${name}.sanpid"
  local nc_pid="" fd="" sanpid=""

  [[ -f "$fdf" ]] && fd="$(cat "$fdf")" || true
  [[ -n "$fd" ]] && exec {fd}>&- 2>/dev/null || true

  [[ -f "$pidf" ]] && nc_pid="$(cat "$pidf")" || true
  sleep 0.10
  if [[ -n "$nc_pid" ]] && kill -0 "$nc_pid" 2>/dev/null; then kill -INT "$nc_pid" 2>/dev/null || true; fi
  sleep 0.10
  if [[ -n "$nc_pid" ]] && kill -0 "$nc_pid" 2>/dev/null; then kill -TERM "$nc_pid" 2>/dev/null || true; fi
  sleep 0.05
  if [[ -n "$nc_pid" ]] && kill -0 "$nc_pid" 2>/dev/null; then kill -KILL "$nc_pid" 2>/dev/null || true; fi

  [[ -f "$sanpidf" ]] && sanpid="$(cat "$sanpidf")" || true
  [[ -n "$sanpid" ]] && kill "$sanpid" 2>/dev/null || true
}

cleanup_all() {
  for n in alice bob eve mallory dupe pre carol; do close_client "$n" || true; done
  echo "${YELLOW}Logs: ${RUN_DIR}${RESET}"
}
trap cleanup_all EXIT

# ---------------- tiny test DSL ----------------
tail_snip() { tail -n 25 "$1" 2>/dev/null || true; }

wait_for() { # name pattern timeout_sec
  local name="$1" pat="$2" to="$3"
  local log="$RUN_DIR/${name}.log"
  local end=$((SECONDS + to))
  while (( SECONDS < end )); do
    grep -aE -m1 -q "$pat" "$log" 2>/dev/null && return 0
    sleep 0.05
  done
  return 1
}

pass() { printf "%b[PASS]%b %s\n" "$GREEN" "$RESET" "$1"; ((PASS_CNT++)); }
fail() { printf "%b[FAIL]%b %s\n" "$RED" "$RESET" "$1"; ((FAIL_CNT++)); }

assert_contains() { # name pattern timeout desc
  local name="$1" pat="$2" to="$3" desc="$4"
  if wait_for "$name" "$pat" "$to"; then
    pass "$desc"
  else
    fail "$desc"
    echo "      expected on [$name]: /$pat/ (timeout ${to}s) | log: ${RUN_DIR}/${name}.log"
    tail_snip "${RUN_DIR}/${name}.log"
  fi
}

send_and_expect_any() { # name line timeout desc pat1 [pat2 ...]
  local name="$1" line="$2" to="$3" desc="$4"; shift 4
  send_line "$name" "$line"
  local ok=1 pat
  for pat in "$@"; do
    if wait_for "$name" "$pat" "$to"; then ok=0; break; fi
  done
  if [[ $ok -eq 0 ]]; then
    pass "$desc"
  else
    fail "$desc"
    echo "      expected on [$name]: any of:"
    for pat in "$@"; do echo "        /$pat/"; done
    echo "      log: ${RUN_DIR}/${name}.log"
    tail_snip "${RUN_DIR}/${name}.log"
  fi
}

expect_modes_via_324() { # name chan timeout desc flags...
  local name="$1" chan="$2" to="$3" desc="$4"; shift 4
  send_line "$name" "MODE ${chan}"
  assert_contains "$name" "324[[:space:]]+${name}[[:space:]]+${chan}[[:space:]]+\\+" "$to" "${desc} (got 324)"
  local f
  for f in "$@"; do
    assert_contains "$name" "324[[:space:]]+${name}[[:space:]]+${chan}.*${f}" "$to" "  324 contains +${f}"
  done
}

assert_invite_delivered() {
  if wait_for eve ":alice INVITE eve[[:space:]]+:?#${CHAN#\#}" 4; then
    pass "eve got INVITE"
  elif wait_for alice "\\b341\\b[[:space:]]+alice[[:space:]]+eve[[:space:]]+${CHAN}" 4; then
    pass "eve got INVITE (via 341 to alice)"
  else
    fail "eve got INVITE"
    echo "      expected on [eve]: /:alice INVITE eve[[:space:]]+:?#${CHAN#\#}/"
    echo "      or on [alice]: /341 alice eve ${CHAN}/"
    echo "      eve log: ${RUN_DIR}/eve.log"
    tail_snip "${RUN_DIR}/eve.log"
    echo "      alice log: ${RUN_DIR}/alice.log"
    tail_snip "${RUN_DIR}/alice.log"
  fi
}

# ---------------- run ----------------
echo "${BOLD}Starting ft_irc integration tests on ${HOST}:${PORT}${RESET}"
echo "Logs in: ${RUN_DIR}"
echo "${YELLOW}Channel for this run: ${CHAN}${RESET}"

# === Core trio ===
start_client alice
start_client bob
start_client eve

# Handshake
send_and_expect_any alice "PASS hello123456" 5 "alice PASS accepted" \
  "Password accepted" "NOTICE \\* :Password accepted"
send_and_expect_any alice "NICK alice"       5 "alice NICK ok" \
  "Nick set" "NOTICE \\* :Nick set" "Now send USER" "Your nickname is now"
send_and_expect_any alice "USER alice 0 * :Alice" 5 "alice registered (001)" "\\b001\\b[[:space:]]+alice"

send_and_expect_any bob   "PASS hello123456" 5 "bob PASS accepted" \
  "Password accepted" "NOTICE \\* :Password accepted"
send_and_expect_any bob   "NICK bob"         5 "bob NICK ok" \
  "Nick set" "NOTICE \\* :Nick set" "Now send USER" "Your nickname is now"
send_and_expect_any bob   "USER bob 0 * :Bob" 5 "bob registered (001)" "\\b001\\b[[:space:]]+bob"

send_and_expect_any eve   "PASS hello123456" 5 "eve PASS accepted" \
  "Password accepted" "NOTICE \\* :Password accepted"
send_and_expect_any eve   "NICK eve"         5 "eve NICK ok" \
  "Nick set" "NOTICE \\* :Nick set" "Now send USER" "Your nickname is now"
send_and_expect_any eve   "USER eve 0 * :Eve" 5 "eve registered (001)" "\\b001\\b[[:space:]]+eve"

# Alice creates channel → op
send_line alice "JOIN ${CHAN}"
assert_contains alice ":alice JOIN ${CHAN}" 5 "alice JOIN (channel created, becomes op)"

# TOPIC retrieval when none
send_line alice "TOPIC ${CHAN}"
assert_contains alice "\\b331\\b[[:space:]]+alice[[:space:]]+${CHAN}[[:space:]]+:No topic is set|No topic is set" 3 "TOPIC query returns 331 when no topic"

# Modes + gating
send_line alice "MODE ${CHAN} +t"
expect_modes_via_324 alice "${CHAN}" 3 "modes after +t" "t"

send_line alice "MODE ${CHAN} +k ${KEY}"
expect_modes_via_324 alice "${CHAN}" 3 "modes after +k" "t" "k"

send_line alice "MODE ${CHAN} +l ${LIMIT}"
expect_modes_via_324 alice "${CHAN}" 3 "modes after +l" "t" "k" "l"

# Bob join without key (expect +k)
send_line bob "JOIN ${CHAN}"
assert_contains bob "\\b475\\b.*\\(\\+k\\)|Cannot join channel \\(\\+k\\)" 4 "bob blocked by +k (no key)"

# Bob join with key (<= limit)
send_line bob "JOIN ${CHAN} ${KEY}"
assert_contains alice ":bob JOIN ${CHAN}" 5 "bob joined with key (<= limit)"

# Eve join with key while full (expect +l)
send_line eve "JOIN ${CHAN} ${KEY}"
assert_contains eve "\\b471\\b|Cannot join channel \\(\\+l\\)" 4 "eve blocked by +l"

# Remove limit, then +i
send_line alice "MODE ${CHAN} -l"
expect_modes_via_324 alice "${CHAN}" 3 "modes after -l" "t" "k"

send_line alice "MODE ${CHAN} +i"
expect_modes_via_324 alice "${CHAN}" 3 "modes after +i" "t" "k" "i"

# Eve blocked by +i
send_line eve "JOIN ${CHAN} ${KEY}"
assert_contains eve "\\b473\\b.*\\(\\+i\\)|Cannot join channel \\(\\+i\\)" 4 "eve blocked by +i (invite-only)"

# Invite Eve
send_line alice "INVITE eve ${CHAN}"
assert_invite_delivered

send_line eve "JOIN ${CHAN} ${KEY}"
assert_contains alice ":eve JOIN ${CHAN}" 5 "eve joined after invite"

# TOPIC under +t: bob blocked, alice can set, 332 on query
send_line bob "TOPIC ${CHAN} :BobTopic"
assert_contains bob "You're not a channel operator|\\b482\\b" 4 "bob blocked by +t on TOPIC"

send_line alice "TOPIC ${CHAN} :OfficialTopic"
assert_contains alice ":alice TOPIC ${CHAN} :OfficialTopic" 4 "alice set topic"

send_line bob "TOPIC ${CHAN}"
assert_contains bob "\\b332\\b[[:space:]]+bob[[:space:]]+${CHAN}[[:space:]]+:OfficialTopic|:OfficialTopic" 3 "TOPIC query returns 332 with topic"

# Promote bob → can set topic; demote → blocked
send_line alice "MODE ${CHAN} +o bob"
send_line bob   "TOPIC ${CHAN} :BobIsOpNow"
assert_contains bob ":bob TOPIC ${CHAN} :BobIsOpNow" 4 "bob set topic as op"

send_line alice "MODE ${CHAN} -o bob"
send_line bob   "TOPIC ${CHAN} :BobAgain"
assert_contains bob "You're not a channel operator|\\b482\\b" 4 "bob blocked after -o"

# PRIVMSG broadcast to ALL (alice & eve must see it)
send_line bob "PRIVMSG ${CHAN} :hello everyone"
assert_contains alice ":bob PRIVMSG ${CHAN} :hello everyone" 4 "channel PRIVMSG seen by alice"
assert_contains eve   ":bob PRIVMSG ${CHAN} :hello everyone" 4 "channel PRIVMSG seen by eve"

# Direct PRIVMSG
send_line eve "PRIVMSG alice :hi alice"
assert_contains alice ":eve PRIVMSG alice :hi alice" 4 "direct PRIVMSG delivered"

# ==== NEW CASES ====

# KICK: alice kicks bob; bob cannot talk to channel anymore
send_line alice "KICK ${CHAN} bob :bye"
assert_contains bob   ":alice[[:space:]]+KICK[[:space:]]+${CHAN}[[:space:]]+bob[[:space:]]+:?bye|:alice KICK ${CHAN} bob" 4 "bob received KICK"
# bob attempts channel PRIVMSG → expect 'not on channel' (text or 442)
send_line bob "PRIVMSG ${CHAN} :still here?"
assert_contains bob "You're not on[[:space:]]+${CHAN}|\\b442\\b.*${CHAN}" 4 "bob cannot PRIVMSG after KICK"

# Mode removals: -i and -k, then new client joins with NO key
send_line alice "MODE ${CHAN} -i"
send_line alice "MODE ${CHAN} -k"
expect_modes_via_324 alice "${CHAN}" 3 "modes after removing -i -k" "t"  # (still +t)
# New client carol joins w/out key
start_client carol
send_and_expect_any carol "PASS hello123456" 5 "carol PASS accepted" "Password accepted" "NOTICE \\* :Password accepted"
send_and_expect_any carol "NICK carol"       5 "carol NICK ok"       "Nick set" "Now send USER" "Your nickname is now"
send_and_expect_any carol "USER carol 0 * :Carol" 5 "carol registered (001)" "\\b001\\b[[:space:]]+carol"
send_line carol "JOIN ${CHAN}"
assert_contains alice ":carol JOIN ${CHAN}" 5 "carol joined without key after -k -i"

# Wrong password then recovery (mallory)
start_client mallory
send_line mallory "PASS wrongpass"
assert_contains mallory "Invalid password|\\b464\\b" 4 "mallory wrong PASS rejected"
send_line mallory "PASS hello123456"
assert_contains mallory "Password accepted|\\b381\\b|NOTICE \\* :Password accepted" 4 "mallory PASS accepted after retry"
send_and_expect_any mallory "NICK mallory" 5 "mallory NICK ok" "Your nickname is now|Nick set"
send_and_expect_any mallory "USER mallory 0 * :Mallory" 5 "mallory registered (001)" "\\b001\\b[[:space:]]+mallory"

# NICK collision: dupe tries to take 'alice'
start_client dupe
send_and_expect_any dupe "PASS hello123456" 5 "dupe PASS accepted" "Password accepted|NOTICE \\* :Password accepted"
send_line dupe "NICK alice"
assert_contains dupe "Nickname is already in use|\\b433\\b" 4 "NICK collision rejected"

# Command before registration: JOIN without registering
start_client pre
send_line pre "JOIN ${CHAN}"
assert_contains pre "You are not registered|\\b451\\b" 4 "JOIN before registration rejected"

# Partial packet reassembly: eve sends fragmentary PRIVMSG to alice
send_raw eve "PRIV"
sleep 0.05; send_raw eve "MSG alice :chunked"
sleep 0.05; send_raw eve " hello"
sleep 0.05; send_raw eve $'\r\n'
assert_contains alice ":eve PRIVMSG alice :chunked hello" 4 "partial fragments reassembled into one PRIVMSG"

# Done
for n in alice bob eve mallory dupe pre carol; do close_client "$n"; done

echo
echo "${BOLD}Results:${RESET} ${GREEN}${PASS_CNT} passed${RESET}, ${RED}${FAIL_CNT} failed${RESET}"
[[ "$FAIL_CNT" -eq 0 ]] || { echo "${YELLOW}See logs in ${RUN_DIR}${RESET}"; exit 1; }
echo "All good! Logs kept in: ${RUN_DIR}"
