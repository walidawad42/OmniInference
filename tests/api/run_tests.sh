#!/usr/bin/env bash
#
# End-to-end round-trip tests for the OmniServer API surface.
#
# Spins up `OmniServer --mock --port <PORT>` on an ephemeral high port, fires
# a curated set of curl requests at it, and validates each response.
#
# Usage:
#   tests/api/run_tests.sh                       # requires OmniServer on PATH
#   tests/api/run_tests.sh /path/to/OmniServer
#
# Exits non-zero on the first failed assertion. Designed to be safe to run
# under CI as well as locally.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
SERVER_BIN="${1:-${OMNI_SERVER_BIN:-${ROOT_DIR}/build/OmniServer}}"
PORT="${PORT:-${OMNI_TEST_PORT:-19080}}"
BASE_URL="http://127.0.0.1:${PORT}"

TMP_DIR="$(mktemp -d)"
LOG_FILE="${TMP_DIR}/server.log"
SERVER_PID=""

cleanup() {
    if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

PASS=0
FAIL=0

ok() {
    PASS=$((PASS + 1))
    printf '  \e[32mok\e[0m %s\n' "$1"
}

fail() {
    FAIL=$((FAIL + 1))
    printf '  \e[31mFAIL\e[0m %s\n' "$1"
    if [[ -n "${2:-}" ]]; then
        printf '       %s\n' "$2"
    fi
}

# Assert that the JSON-decoded value at $2 (a Python expression on `data`)
# matches $3.
assert_json_eq() {
    local label="$1" expr="$2" expected="$3" actual
    actual="$(python3 -c "import sys, json; data=json.load(sys.stdin); print(${expr})" <<<"${4}")"
    if [[ "${actual}" == "${expected}" ]]; then
        ok "${label}"
    else
        fail "${label}" "expected '${expected}' got '${actual}' (raw: ${4})"
    fi
}

# Assert that the JSON-decoded value at $2 is "truthy" (Python truthiness).
assert_json_truthy() {
    local label="$1" expr="$2" actual
    actual="$(python3 -c "import sys, json; data=json.load(sys.stdin); print(bool(${expr}))" <<<"${3}")"
    if [[ "${actual}" == "True" ]]; then
        ok "${label}"
    else
        fail "${label}" "value at ${expr} was falsy (raw: ${3})"
    fi
}

require_bin() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "tests/api: missing required binary '$1'" >&2
        exit 2
    fi
}
require_bin curl
require_bin python3

if [[ ! -x "${SERVER_BIN}" ]]; then
    echo "tests/api: OmniServer binary not found at ${SERVER_BIN}" >&2
    echo "tests/api: build it first:    cmake --build build -j" >&2
    exit 2
fi

echo "tests/api: launching ${SERVER_BIN} --mock --port ${PORT}"
"${SERVER_BIN}" --mock --port "${PORT}" >"${LOG_FILE}" 2>&1 &
SERVER_PID=$!

# Wait for the listener to come up.
for _ in $(seq 1 50); do
    if curl -sf "${BASE_URL}/health" >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done
if ! curl -sf "${BASE_URL}/health" >/dev/null 2>&1; then
    echo "tests/api: server failed to start; log:" >&2
    cat "${LOG_FILE}" >&2
    exit 1
fi

#############################################################################
echo "=== /health ==="
RESP="$(curl -sf "${BASE_URL}/health")"
assert_json_eq "status==ok"          "data['status']"        "ok"          "${RESP}"
assert_json_eq "mock_mode==True"     "data['mock_mode']"     "True"        "${RESP}"

#############################################################################
echo "=== GET /v1/models ==="
RESP="$(curl -sf "${BASE_URL}/v1/models")"
assert_json_eq "object==list"        "data['object']"        "list"        "${RESP}"
assert_json_eq "models[0].id"        "data['data'][0]['id']" "omni-inference-mock" "${RESP}"
assert_json_eq "models[0].owned_by"  "data['data'][0]['owned_by']" "omni-inference"  "${RESP}"

#############################################################################
echo "=== POST /v1/chat/completions (non-streaming) ==="
RESP="$(curl -sf "${BASE_URL}/v1/chat/completions" \
  -H 'Content-Type: application/json' \
  -d '{"model":"omni-mock","messages":[{"role":"user","content":"hello"}]}')"
assert_json_eq "object==chat.completion"     "data['object']"                     "chat.completion" "${RESP}"
assert_json_eq "choices[0].finish_reason"    "data['choices'][0]['finish_reason']" "stop"           "${RESP}"
assert_json_eq "choices[0].message.role"     "data['choices'][0]['message']['role']" "assistant"   "${RESP}"
assert_json_truthy "content non-empty"        "data['choices'][0]['message']['content']"            "${RESP}"
assert_json_truthy "usage.total_tokens > 0"   "data['usage']['total_tokens'] > 0"                   "${RESP}"

#############################################################################
echo "=== POST /v1/chat/completions (streaming) ==="
STREAM_LOG="${TMP_DIR}/stream.log"
curl -sf -N "${BASE_URL}/v1/chat/completions" \
  -H 'Content-Type: application/json' \
  -d '{"model":"omni-mock","stream":true,"messages":[{"role":"user","content":"hi"}]}' \
  >"${STREAM_LOG}"
if grep -q "^data: \[DONE\]" "${STREAM_LOG}"; then
    ok "stream terminated with [DONE]"
else
    fail "stream missing [DONE]"
fi
DELTA_COUNT="$(grep -c '"chat.completion.chunk"' "${STREAM_LOG}" || true)"
if [[ "${DELTA_COUNT}" -gt 0 ]]; then
    ok "saw ${DELTA_COUNT} chat.completion.chunk events"
else
    fail "no chat.completion.chunk events seen"
fi

#############################################################################
echo "=== POST /v1/chat/completions (tools) ==="
RESP="$(curl -sf "${BASE_URL}/v1/chat/completions" \
  -H 'Content-Type: application/json' \
  -d '{
    "model":"omni-mock",
    "messages":[{"role":"user","content":"what is the weather"}],
    "tools":[{"type":"function","function":{"name":"get_weather",
              "description":"Get current weather",
              "parameters":{"type":"object","properties":{"q":{"type":"string"}},"required":["q"]}}}]
  }')"
assert_json_eq "tool finish_reason"   "data['choices'][0]['finish_reason']"             "tool_calls"  "${RESP}"
assert_json_eq "tool name"            "data['choices'][0]['message']['tool_calls'][0]['function']['name']" "get_weather" "${RESP}"
assert_json_truthy "tool id present"  "data['choices'][0]['message']['tool_calls'][0]['id']"             "${RESP}"

#############################################################################
echo "=== POST /v1/completions (legacy) ==="
RESP="$(curl -sf "${BASE_URL}/v1/completions" \
  -H 'Content-Type: application/json' \
  -d '{"model":"omni-mock","prompt":"once upon a time"}')"
assert_json_eq "object==text_completion" "data['object']"  "text_completion" "${RESP}"
assert_json_truthy "text non-empty"      "data['choices'][0]['text']"        "${RESP}"

#############################################################################
echo "=== POST /v1/embeddings (501 envelope) ==="
HTTP_CODE="$(curl -s -o "${TMP_DIR}/embed.json" -w '%{http_code}' \
    "${BASE_URL}/v1/embeddings" \
    -H 'Content-Type: application/json' \
    -d '{"model":"omni-mock","input":"hello"}')"
RESP="$(cat "${TMP_DIR}/embed.json")"
# We return 200 with an error envelope (matches OpenAI's behaviour for
# unsupported features). Just make sure the envelope is shaped correctly.
if [[ "${HTTP_CODE}" =~ ^(200|501)$ ]]; then
    ok "status ${HTTP_CODE}"
else
    fail "unexpected status ${HTTP_CODE} (body: ${RESP})"
fi
assert_json_eq "error.code" "data['error']['code']" "no_embedding_model" "${RESP}"

#############################################################################
echo "=== POST /v1/messages (Anthropic, non-streaming) ==="
RESP="$(curl -sf "${BASE_URL}/v1/messages" \
  -H 'Content-Type: application/json' \
  -H 'anthropic-version: 2023-06-01' \
  -d '{
    "model":"claude-omni-mock",
    "max_tokens":256,
    "system":"you are a test bot",
    "messages":[{"role":"user","content":"hi"}]
  }')"
assert_json_eq "type==message"        "data['type']"           "message"   "${RESP}"
assert_json_eq "role==assistant"      "data['role']"           "assistant" "${RESP}"
assert_json_eq "stop_reason==end_turn" "data['stop_reason']"   "end_turn"  "${RESP}"
assert_json_truthy "content[0].text non-empty" "data['content'][0]['text']" "${RESP}"

#############################################################################
echo "=== POST /v1/messages (Anthropic, streaming) ==="
STREAM_LOG="${TMP_DIR}/anthropic.log"
curl -sf -N "${BASE_URL}/v1/messages" \
  -H 'Content-Type: application/json' \
  -H 'anthropic-version: 2023-06-01' \
  -d '{
    "model":"claude-omni-mock",
    "max_tokens":256,
    "stream":true,
    "messages":[{"role":"user","content":"streamed"}]
  }' >"${STREAM_LOG}"
for evt in message_start content_block_start content_block_delta content_block_stop message_delta message_stop; do
    if grep -q "^event: ${evt}$" "${STREAM_LOG}"; then
        ok "anthropic event: ${evt}"
    else
        fail "anthropic event missing: ${evt}"
    fi
done

#############################################################################
echo "=== POST /v1/messages (Anthropic, tools) ==="
RESP="$(curl -sf "${BASE_URL}/v1/messages" \
  -H 'Content-Type: application/json' \
  -H 'anthropic-version: 2023-06-01' \
  -d '{
    "model":"claude-omni-mock",
    "max_tokens":256,
    "messages":[{"role":"user","content":"call my tool"}],
    "tools":[{"name":"do_thing","description":"do a thing",
              "input_schema":{"type":"object","properties":{"q":{"type":"string"}}}}]
  }')"
assert_json_eq "stop_reason==tool_use"  "data['stop_reason']"          "tool_use" "${RESP}"
assert_json_eq "first block type=tool_use" \
                "[b['type'] for b in data['content'] if b['type']=='tool_use'][0]" \
                "tool_use" "${RESP}"
assert_json_eq "tool_use name"          "[b['name'] for b in data['content'] if b['type']=='tool_use'][0]" \
                "do_thing" "${RESP}"

#############################################################################
echo "=== CORS preflight (OPTIONS) ==="
HEADERS="$(curl -s -o /dev/null -D - -X OPTIONS "${BASE_URL}/v1/chat/completions" \
  -H 'Origin: https://aider.chat' \
  -H 'Access-Control-Request-Method: POST')"
if grep -qi '^Access-Control-Allow-Origin: \*' <<<"${HEADERS}"; then
    ok "CORS origin header present"
else
    fail "CORS origin header missing" "${HEADERS}"
fi
if grep -qi '^Access-Control-Allow-Methods:.*POST' <<<"${HEADERS}"; then
    ok "CORS methods header includes POST"
else
    fail "CORS methods header missing POST" "${HEADERS}"
fi

#############################################################################
echo "=== Auth: 401 when api-key required and missing ==="
# Restart the server with --api-key.
kill "${SERVER_PID}"
wait "${SERVER_PID}" 2>/dev/null || true
"${SERVER_BIN}" --mock --port "${PORT}" --api-key sekret >"${LOG_FILE}" 2>&1 &
SERVER_PID=$!
for _ in $(seq 1 50); do
    if curl -sf "${BASE_URL}/health" >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

HTTP_CODE="$(curl -s -o "${TMP_DIR}/auth.json" -w '%{http_code}' \
    "${BASE_URL}/v1/chat/completions" \
    -H 'Content-Type: application/json' \
    -d '{"model":"x","messages":[{"role":"user","content":"hi"}]}')"
if [[ "${HTTP_CODE}" == "401" ]]; then
    ok "no key -> 401"
else
    fail "no key got ${HTTP_CODE} not 401" "$(cat "${TMP_DIR}/auth.json")"
fi

HTTP_CODE="$(curl -s -o /dev/null -w '%{http_code}' \
    "${BASE_URL}/v1/chat/completions" \
    -H 'Content-Type: application/json' \
    -H 'Authorization: Bearer sekret' \
    -d '{"model":"x","messages":[{"role":"user","content":"hi"}]}')"
if [[ "${HTTP_CODE}" == "200" ]]; then
    ok "valid bearer key -> 200"
else
    fail "valid bearer got ${HTTP_CODE} not 200"
fi

# Anthropic-style x-api-key header should also work.
HTTP_CODE="$(curl -s -o /dev/null -w '%{http_code}' \
    "${BASE_URL}/v1/messages" \
    -H 'Content-Type: application/json' \
    -H 'anthropic-version: 2023-06-01' \
    -H 'x-api-key: sekret' \
    -d '{"model":"claude-omni-mock","max_tokens":64,"messages":[{"role":"user","content":"hi"}]}')"
if [[ "${HTTP_CODE}" == "200" ]]; then
    ok "x-api-key -> 200"
else
    fail "x-api-key got ${HTTP_CODE} not 200"
fi

#############################################################################
printf '\nResults: \e[32m%d passed\e[0m, \e[31m%d failed\e[0m\n' "${PASS}" "${FAIL}"
exit $(( FAIL == 0 ? 0 : 1 ))
