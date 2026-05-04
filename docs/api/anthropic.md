# OmniServer — Anthropic-compatible API

OmniServer's `/v1/messages` endpoint speaks the Anthropic
`messages` API (the one Claude Code, the official `anthropic` Python /
TypeScript SDKs, and Aider's "anthropic" mode talk to). Tool definitions
written in OpenAI's `tools[]` schema are translated to Anthropic
`input_schema` form on the way in, and `tool_use` content blocks are
generated on the way out.

## Quick start

```bash
./build/OmniServer --mock --port 8080

curl http://localhost:8080/v1/messages \
    -H 'Content-Type: application/json' \
    -H 'anthropic-version: 2023-06-01' \
    -d '{
        "model": "claude-omni-mock",
        "max_tokens": 256,
        "system": "You are a helpful assistant.",
        "messages": [
            {"role": "user", "content": "Hello!"}
        ]
    }'
```

## Endpoint

| Method | Path           | Notes                                       |
|--------|----------------|---------------------------------------------|
| POST   | `/v1/messages` | Streaming + non-streaming + tool_use blocks |

The endpoint accepts and ignores `anthropic-version` / `anthropic-beta`
headers; they're allowed in the CORS preflight so browser clients work
out of the box.

## Request / response shapes

### Non-streaming

Request:

```json
{
    "model": "claude-omni-mock",
    "max_tokens": 256,
    "system": "You are a helpful assistant.",
    "messages": [
        {"role": "user",
         "content": [{"type": "text", "text": "Hello!"}]}
    ]
}
```

Plain string content (`"content": "Hello!"`) is also accepted and treated
as a single text block, matching the official SDKs.

Response:

```json
{
    "id": "msg_1730000000",
    "type": "message",
    "role": "assistant",
    "model": "claude-omni-mock",
    "content": [{"type": "text", "text": "..."}],
    "stop_reason": "end_turn",
    "stop_sequence": null,
    "usage": {"input_tokens": 13, "output_tokens": 42}
}
```

### Streaming

Set `"stream": true`. The server emits Anthropic's full event sequence:

```
event: message_start
data: {"type":"message_start","message":{...}}

event: content_block_start
data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hello"}}

...

event: content_block_stop
data: {"type":"content_block_stop","index":0}

event: message_delta
data: {"type":"message_delta","delta":{"stop_reason":"end_turn","stop_sequence":null},"usage":{"output_tokens":42}}

event: message_stop
data: {"type":"message_stop"}
```

### Tools / tool_use

Pass `tools` Anthropic-style:

```json
{
    "model": "claude-omni-mock",
    "max_tokens": 256,
    "messages": [{"role": "user", "content": "what is the weather?"}],
    "tools": [{
        "name": "get_weather",
        "description": "Get current weather.",
        "input_schema": {
            "type": "object",
            "properties": {"q": {"type": "string"}},
            "required": ["q"]
        }
    }]
}
```

Response (`stop_reason: "tool_use"` and a `tool_use` content block):

```json
{
    "type": "message",
    "role": "assistant",
    "stop_reason": "tool_use",
    "content": [{
        "type": "tool_use",
        "id": "call-...",
        "name": "get_weather",
        "input": {"echo": "what is the weather?"}
    }],
    "usage": {"input_tokens": 5, "output_tokens": 21}
}
```

In streaming mode, `tool_use` blocks are emitted via
`content_block_start` (with `type: "tool_use"`) followed by
`input_json_delta` events, then `content_block_stop`.

### Returning tool results

When you've executed the tool, send the result back the same way the
official Anthropic SDK does — as a `tool_result` content block on a
`user`-role message:

```json
{
    "model": "claude-omni-mock",
    "max_tokens": 256,
    "messages": [
        {"role": "user", "content": "what is the weather?"},
        {"role": "assistant", "content": [
            {"type": "tool_use", "id": "call-...",
             "name": "get_weather", "input": {"q": "Cairo"}}
        ]},
        {"role": "user", "content": [
            {"type": "tool_result", "tool_use_id": "call-...",
             "content": "Sunny, 31°C"}
        ]}
    ]
}
```

## Authentication

Both `Authorization: Bearer <key>` and `x-api-key: <key>` headers are
accepted; pick whichever your client wants. Auth only kicks in when
`OmniServer` is started with `--api-key`.

## Client integrations

### Anthropic Python SDK

```python
from anthropic import Anthropic

client = Anthropic(
    base_url="http://localhost:8080",
    api_key="sk-omni-local",  # only needed if --api-key is set
)
msg = client.messages.create(
    model="claude-omni-mock",
    max_tokens=256,
    messages=[{"role": "user", "content": "hi"}],
)
print(msg.content[0].text)
```

### Claude Code (CLI)

```bash
export ANTHROPIC_API_URL="http://localhost:8080"
export ANTHROPIC_API_KEY="sk-omni-local"
claude
```

### Aider (Anthropic mode)

```bash
export ANTHROPIC_API_BASE="http://localhost:8080"
export ANTHROPIC_API_KEY="sk-omni-local"
aider --model anthropic/claude-omni-mock
```

## Limits in Stage A

- Image content blocks (`{"type": "image", "source": {...}}`) are parsed
  but the engine ignores them; vision lands in Stage B.
- The mock engine deterministically invokes the *first* tool with
  `{"echo": <last user message>}`.
- Real-engine tool calling isn't wired through OmniEngine yet — when
  not in `--mock` mode the engine returns text only. Tool calling on a
  real model lands together with Stage A2.
