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

## Vision (Stage B)

OpenAI-style `image_url` content parts are now decoded server-side via
stb_image. The flow:

```json
{
    "model": "omni-mock",
    "messages": [{
        "role": "user",
        "content": [
            {"type": "text", "text": "describe this"},
            {"type": "image_url",
             "image_url": {"url": "data:image/png;base64,iVBORw0KG..."}}
        ]
    }]
}
```

OmniServer:

1. Parses the request and walks every `image_url` content part.
2. Decodes the `data:` URI (base64 → raw bytes) and runs it through
   stb_image (PNG / JPEG / BMP / GIF / TGA). The result is a packed-RGB
   raster + width / height / source media type, attached back onto the
   content part.
3. Hands the request to the engine. Mock mode acknowledges every image
   in the assistant's reply (`(with 1 image: image/png 1x1)`); a real
   vision-capable engine consumes the pixels.

Decode failures (malformed base64, corrupted PNG, unsupported media
type) do **not** 4xx the request. Instead, the failed part is left
without `decoded_image` populated and the mock surfaces the error
inline (`(with 1 image: image/png (decode failed: stb_image decode
failed: ...))`). This matches OpenAI's behaviour where the model still
responds even when a vision input is unreadable.

`http://` / `https://` URLs are intentionally **not** fetched yet —
that needs a TLS-capable allow-listed HTTP client and a separate
opt-in. In the meantime, base64 your images on the client side.

## Limits in Stage A

- Audio inputs are not parsed at all.
- `/v1/embeddings` always returns the `no_embedding_model` envelope.
- `--mock` uses a deterministic canned response. To exercise real
  inference, drop `--mock` and load a model via `POST /v1/models/load` (or
  pre-load in code). Real-engine streaming surfaces only text deltas — the
  engine doesn't yet have a tool-calling protocol.

## Limits in Stage B

- Real vision-language inference is not wired into OmniEngine yet — the
  decode pipeline lands here, but actually pushing the pixels through a
  CLIP / SigLIP encoder + projecting them into the LLM's embedding space
  is Stage B2. Tracking which backend (a second modern llama.cpp on CPU
  vs. an older but cc 3.0–compatible LLaVA-1.5/1.6 path) is the right
  fit for the K5100M is a Stage B2 design call; until then, the mock
  echoes back what it received so vision plumbing is testable.
