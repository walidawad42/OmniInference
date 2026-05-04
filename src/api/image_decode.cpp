#include "api/image_decode.h"

#include <algorithm>
#include <array>
#include <cstring>

// stb_image: define the implementation in exactly one .cpp.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_STATIC
#include <stb_image_resize2.h>

namespace omni::api {

namespace {

// --- Base64 decode ---------------------------------------------------------
constexpr int kInvalid = -1;
constexpr int kPad     = -2;

int DecodeChar(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;  // url-safe variant maps '-' to 62
    if (c == '/' || c == '_') return 63;  // url-safe variant maps '_' to 63
    if (c == '=')             return kPad;
    return kInvalid;
}

// --- Helpers ---------------------------------------------------------------
void SetError(std::string* out, const std::string& msg) {
    if (out) *out = msg;
}

// Split a `data:` URI into (media-type, payload). Returns false if the URI
// shape is malformed.
struct DataUri {
    std::string media_type;
    bool        is_base64 = false;
    std::string payload;  // raw, still encoded
};

bool ParseDataUri(const std::string& uri, DataUri& out) {
    if (uri.compare(0, 5, "data:") != 0) return false;
    auto comma = uri.find(',');
    if (comma == std::string::npos) return false;

    std::string head = uri.substr(5, comma - 5);
    out.payload      = uri.substr(comma + 1);

    auto semi = head.find(';');
    if (semi == std::string::npos) {
        out.media_type = head;
        out.is_base64  = false;
    } else {
        out.media_type = head.substr(0, semi);
        out.is_base64  = head.find("base64", semi) != std::string::npos;
    }
    if (out.media_type.empty()) {
        out.media_type = "image/png";
    }
    return true;
}

// Strip whitespace + decode percent-escapes. Best-effort; only used for the
// `data:<mime>,<payload>` (non-base64) branch which is rare in practice.
std::vector<uint8_t> UrlDecodeBytes(const std::string& s) {
    std::vector<uint8_t> out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '%' && i + 2 < s.size()) {
            auto hex = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                return -1;
            };
            int hi = hex(s[i + 1]);
            int lo = hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<uint8_t>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        if (c == '+') {
            out.push_back(' ');
        } else {
            out.push_back(static_cast<uint8_t>(c));
        }
    }
    return out;
}

}  // namespace

bool Base64Decode(const std::string& s, std::vector<uint8_t>& out) {
    out.clear();
    out.reserve((s.size() / 4) * 3);

    int  buf       = 0;
    int  buf_bits  = 0;
    int  pad_count = 0;

    for (char raw : s) {
        // Skip whitespace + line-wrapping characters.
        if (raw == '\n' || raw == '\r' || raw == ' ' || raw == '\t') continue;
        int v = DecodeChar(raw);
        if (v == kInvalid) return false;
        if (v == kPad) {
            ++pad_count;
            buf       <<= 6;
            buf_bits   += 6;
            if (buf_bits >= 8) {
                buf_bits -= 8;
            }
            continue;
        }
        if (pad_count > 0) return false;  // padding before more data is illegal
        buf       = (buf << 6) | v;
        buf_bits += 6;
        if (buf_bits >= 8) {
            buf_bits -= 8;
            out.push_back(static_cast<uint8_t>((buf >> buf_bits) & 0xFF));
        }
    }
    return true;
}

std::optional<DecodedImage> DecodeImageBytes(const uint8_t* data,
                                             size_t         len,
                                             std::string*   error_out) {
    if (data == nullptr || len == 0) {
        SetError(error_out, "empty image payload");
        return std::nullopt;
    }
    int w = 0, h = 0, src_channels = 0;
    // Force 3 channels (RGB). stb still tells us the source's actual channel
    // count via `src_channels` so we can keep it as metadata.
    unsigned char* pixels = stbi_load_from_memory(data, static_cast<int>(len),
                                                  &w, &h, &src_channels, 3);
    if (pixels == nullptr) {
        SetError(error_out,
                 std::string("stb_image decode failed: ") + stbi_failure_reason());
        return std::nullopt;
    }
    DecodedImage img;
    img.width        = w;
    img.height       = h;
    img.channels     = 3;
    img.source_bytes = len;
    img.pixels.assign(pixels, pixels + (static_cast<size_t>(w) * h * 3));
    stbi_image_free(pixels);
    return img;
}

std::optional<DecodedImage> DecodeImageUrl(const std::string& image_url,
                                           std::string*       error_out) {
    if (image_url.empty()) {
        SetError(error_out, "empty image_url");
        return std::nullopt;
    }
    if (image_url.compare(0, 5, "data:") == 0) {
        DataUri uri;
        if (!ParseDataUri(image_url, uri)) {
            SetError(error_out, "malformed data: URI");
            return std::nullopt;
        }
        std::vector<uint8_t> bytes;
        if (uri.is_base64) {
            if (!Base64Decode(uri.payload, bytes)) {
                SetError(error_out, "base64 decode failed");
                return std::nullopt;
            }
        } else {
            bytes = UrlDecodeBytes(uri.payload);
        }
        auto decoded = DecodeImageBytes(bytes.data(), bytes.size(), error_out);
        if (!decoded) return std::nullopt;
        decoded->source_kind       = "data-uri";
        decoded->source_media_type = uri.media_type;
        return decoded;
    }
    if (image_url.compare(0, 7, "http://") == 0 ||
        image_url.compare(0, 8, "https://") == 0) {
        SetError(error_out,
                 "remote URL fetch is disabled in Stage B; pass the image "
                 "as a data: URI instead");
        return std::nullopt;
    }
    SetError(error_out, "unsupported image_url scheme");
    return std::nullopt;
}

int DecodeImagesIn(ChatRequest& req) {
    int decoded_count = 0;
    for (auto& msg : req.messages) {
        for (auto& part : msg.content) {
            if (part.kind != ContentPart::Kind::kImageUrl) continue;
            // Already decoded — skip (idempotent).
            if (part.decoded_image && !part.decoded_image->empty()) {
                ++decoded_count;
                continue;
            }
            std::string err;
            auto decoded = DecodeImageUrl(part.image_url, &err);
            if (decoded) {
                if (!part.image_media_type.empty()) {
                    decoded->source_media_type = part.image_media_type;
                }
                part.decoded_image = std::make_shared<DecodedImage>(std::move(*decoded));
                part.image_decode_error.clear();
                ++decoded_count;
            } else {
                part.image_decode_error = err.empty() ? "decode failed" : err;
            }
        }
    }
    return decoded_count;
}

DecodedImage ResizeImage(const DecodedImage& src, int target_w, int target_h) {
    DecodedImage out;
    out.width             = target_w;
    out.height            = target_h;
    out.channels          = src.channels;
    out.source_media_type = src.source_media_type;
    out.source_kind       = src.source_kind;
    out.source_bytes      = src.source_bytes;
    out.pixels.assign(static_cast<size_t>(target_w) * target_h * src.channels, 0);

    if (src.empty() || target_w <= 0 || target_h <= 0) return out;

    stbir_pixel_layout layout = (src.channels == 4) ? STBIR_RGBA : STBIR_RGB;
    stbir_resize_uint8_linear(
        src.pixels.data(),  src.width,  src.height,  /*src_stride*/ 0,
        out.pixels.data(),  target_w,   target_h,    /*dst_stride*/ 0,
        layout);
    return out;
}

}  // namespace omni::api
