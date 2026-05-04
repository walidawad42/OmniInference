# OmniServer — OpenAI-compatible API

OmniServer exposes an HTTP API that's wire-compatible with OpenAI's
`/v1/chat/completions` and friends. Any tool that talks to the OpenAI API
through a configurable `OPENAI_API_BASE` / `base_url` will work with
OmniServer; the only thing that changes is the URL and (optionally) the
bearer key.

## Quick start

```bash
# Build
cmake -S . -B build
cmake --build build -j

# Run in deterministic mock mode (no model needed, useful for plumbing
# integrations end-to-end before putting a real GGUF in place)
./build/OmniServer --mock --port 8080
```

```bash
curl http://localhost:8080/v1/chat/completions \
    -H 'Content-Type: application/json' \
    -d '{
        "model": "omni-mock",
        "messages": [
            {"role": "system",  "content": "You are a helpful assistant."},
            {"role": "user",    "content": "Hello!"}
        ]
    }'
```

## Endpoints

| Method | Path                          | Notes                                   |
|--------|-------------------------------|-----------------------------------------|
| GET    | `/health`                     | Liveness + mock-mode flag               |
| GET    | `/v1/models`                  | Lists the loaded model id               |
| POST   | `/v1/chat/completions`        | Chat — streaming + tools                |
| POST   | `/v1/completions`             | Legacy text completion                  |
| POST   | `/v1/embeddings`              | Returns an "embedding model not loaded" envelope until you wire one through `--embedding-model` |
| POST   | `/v1/models/load`             | Load a GGUF at runtime                  |
| POST   | `/tool/call`                  | Invoke a registered tool directly       |
| GET    | `/quantization/status`        | TurboQuant Plus status                  |

All paths return CORS headers and accept an `OPTIONS` preflight from any
origin (`*` by default; override with `--cors-origin`).

## Request / response shapes

### Chat completion (non-streaming)

Request body:

```json
{
    "model": "omni-mock",
    "messages": [
        {"role": "system",    "content": "You are a helpful assistant."},
        {"role": "user",      "content": "What's the weather in Cairo?"}
    ],
    "temperature": 0.7,
    "top_p": 0.95,
    "max_tokens": 512
}
```

Response (truncated):

```json
{
    "id": "chatcmpl-1730000000",
    "object": "chat.completion",
    "model": "omni-mock",
    "choices": [{
        "index": 0,
        "message": {"role": "assistant", "content": "..."},
        "finish_reason": "stop"
    }],
    "usage": {"prompt_tokens": 13, "completion_tokens": 42, "total_tokens": 55}
}
```

### Chat completion (streaming)

Set `"stream": true` in the request. The server replies with
`Content-Type: text/event-stream` and emits `chat.completion.chunk` objects
inside `data: ` SSE frames, terminating with `data: [DONE]`.

```bash
curl -N http://localhost:8080/v1/chat/completions \
    -H 'Content-Type: application/json' \
    -d '{"model":"omni-mock","stream":true,
         "messages":[{"role":"user","content":"hi"}]}'
```

### Tools / function calling

Pass `tools` like the OpenAI API does. The mock engine always invokes the
*first* tool with `{"echo": <last user message>}` so integration tests can
assert the round trip.

```json
{
    "model": "omni-mock",
    "messages": [{"role": "user", "content": "what is the weather"}],
    "tools": [{
        "type": "function",
        "function": {
            "name": "get_weather",
            "description": "Get current weather for a city.",
            "parameters": {
                "type": "object",
                "properties": {"q": {"type": "string"}},
                "required": ["q"]
            }
        }
    }]
}
```

Response (`finish_reason` is `tool_calls` and the message carries
`tool_calls`):

```json
{
    "choices": [{
        "finish_reason": "tool_calls",
        "message": {
            "role": "assistant",
            "content": "",
            "tool_calls": [{
                "id": "call-...",
                "type": "function",
                "function": {
                    "name": "get_weather",
                    "arguments": "{\"echo\":\"what is the weather\"}"
                }
            }]
        }
    }]
}
```

## Authentication

Pass `--api-key <key>` to require a bearer header on every request:

```bash
./build/OmniServer --mock --api-key sk-omni-local
curl http://localhost:8080/v1/chat/completions \
    -H 'Authorization: Bearer sk-omni-local' \
    -H 'Content-Type: application/json' \
    -d '{...}'
```

`x-api-key` is also accepted (Anthropic-style) so a single key works for
both endpoints.

## Client integrations

### Aider

```bash
export OPENAI_API_BASE="http://localhost:8080/v1"
export OPENAI_API_KEY="sk-omni-local"   # optional, only if --api-key
aider --model omni-mock
```

### Continue.dev / Cline (OpenAI-compatible mode)

```jsonc
// .continue/config.json (Continue.dev) or settings.json (Cline)
{
    "models": [{
        "title": "OmniInference (local)",
        "provider": "openai",
        "model": "omni-mock",
        "apiBase": "http://localhost:8080/v1",
        "apiKey": "sk-omni-local"
    }]
}
```

### LangChain (Python)

```python
from langchain_openai import ChatOpenAI

llm = ChatOpenAI(
    base_url="http://localhost:8080/v1",
    api_key="sk-omni-local",
    model="omni-mock",
)
```

### Plain `openai` Python client

```python
from openai import OpenAI

client = OpenAI(
    base_url="http://localhost:8080/v1",
    api_key="sk-omni-local",
)
print(client.chat.completions.create(
    model="omni-mock",
    messages=[{"role": "user", "content": "hi"}],
).choices[0].message.content)
```

## Limits in Stage A

- Vision (`image_url` content parts) is parsed but the engine returns
  text-only responses; the image is dropped silently. Real vision lands
  in Stage B.
- Audio inputs are not parsed at all.
- `/v1/embeddings` always returns the `no_embedding_model` envelope.
- `--mock` uses a deterministic canned response. To exercise real
  inference, drop `--mock` and load a model via `POST /v1/models/load` (or
  pre-load in code). Real-engine streaming surfaces only text deltas — the
  engine doesn't yet have a tool-calling protocol.
