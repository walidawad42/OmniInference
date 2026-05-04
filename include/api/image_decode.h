#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace omni::api {

// A decoded raster image, packed RGB or RGBA, row-major. Source ints are
// 8-bit-per-channel in 0..255. Vision encoders (CLIP-ViT, SigLIP, etc.)
// always normalise from these channels; we leave the conversion to float
// to whichever backend consumes the DecodedImage.
struct DecodedImage {
    std::vector<uint8_t> pixels;
    int                  width    = 0;
    int                  height   = 0;
    int                  channels = 3;       // 3 = RGB, 4 = RGBA
    std::string          source_media_type;  // "image/png", "image/jpeg", ...
    std::string          source_kind;        // "data-uri", "url", "raw"
    size_t               source_bytes = 0;   // size of the encoded payload

    bool empty() const { return pixels.empty(); }
};

// Decode whatever lives behind an `image_url`. Currently supported:
//
//   - `data:<mime>;base64,<base64-payload>`
//   - `data:<mime>,<urlencoded-payload>`           (raw, not base64)
//
// HTTP/HTTPS URLs are NOT fetched in Stage B; that needs an opt-in HTTP
// fetcher with TLS + a sane allow-list and is a Stage B2 concern. When a
// URL is passed in, this returns `std::nullopt` and writes an explanatory
// string into `error_out` (if non-null). The mock engine surfaces this so
// callers can see the URL fetch was skipped intentionally.
std::optional<DecodedImage> DecodeImageUrl(const std::string& image_url,
                                           std::string*       error_out = nullptr);

// Decode raw bytes already in memory (PNG / JPEG / BMP / GIF / TGA via
// stb_image). Strictly internal; callers normally go through
// `DecodeImageUrl`.
std::optional<DecodedImage> DecodeImageBytes(const uint8_t* data,
                                             size_t         len,
                                             std::string*   error_out = nullptr);

// Resize using stb_image_resize2's bilinear path. Always emits the same
// channel count as the source.
DecodedImage ResizeImage(const DecodedImage& src, int target_w, int target_h);

// Decode `s` as base64 (RFC 4648). Returns false on malformed input.
// Exposed so tests + Anthropic schema can decode/encode independently.
bool Base64Decode(const std::string& s, std::vector<uint8_t>& out);

}  // namespace omni::api

// Pulled in here, after DecodedImage / Base64Decode are declared, so the
// chat_request.h forward decl of `struct DecodedImage` doesn't lose access
// to the full type when callers want to actually inspect pixels.
#include "api/chat_request.h"

namespace omni::api {

// Walk every ContentPart in `req` and decode any kImageUrl part into the
// part's `decoded_image`. Decode failures populate `image_decode_error` on
// the part but do NOT abort the whole request — the engine can decide
// whether a missing image is fatal. Returns the number of successfully
// decoded images.
int DecodeImagesIn(struct ChatRequest& req);

}  // namespace omni::api
