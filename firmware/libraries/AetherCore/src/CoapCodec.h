#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace aether {
struct CoapMessage {
  uint8_t type = 0, code = 0, tokenLength = 0;
  uint16_t mid = 0;
  uint8_t token[8] = {};
  char path[96] = {};
  const uint8_t* payload = nullptr;
  size_t payloadLength = 0;
  bool unsupportedOption = false;
};
inline bool coapField(uint8_t nibble, const uint8_t* data, size_t size, size_t& at, uint32_t& value) {
  if (nibble < 13) { value = nibble; return true; }
  const size_t bytes = nibble == 13 ? 1 : nibble == 14 ? 2 : 0;
  if (!bytes || at + bytes > size) return false;
  value = bytes == 1 ? data[at] + 13U : (data[at] << 8U | data[at + 1]) + 269U;
  at += bytes;
  return true;
}
inline bool parseCoap(const uint8_t* data, size_t size, CoapMessage& out) {
  out = CoapMessage{};
  if (size < 4 || size > 2048 || data[0] >> 6 != 1) return false;
  out.type = (data[0] >> 4) & 3; out.tokenLength = data[0] & 15;
  if (out.tokenLength > 8 || size < 4U + out.tokenLength) return false;
  out.code = data[1]; out.mid = data[2] << 8 | data[3];
  memcpy(out.token, data + 4, out.tokenLength);
  size_t at = 4 + out.tokenLength, pathSize = 0;
  uint32_t option = 0;
  while (at < size) {
    const uint8_t head = data[at++];
    if (head == 255) {
      if (at == size) return false;
      out.payload = data + at; out.payloadLength = size - at; return true;
    }
    uint32_t delta, length;
    if (!coapField(head >> 4, data, size, at, delta) || !coapField(head & 15, data, size, at, length)) return false;
    option += delta;
    if (option > 65535 || length > size - at) return false;
    if (option == 11) {
      if (pathSize + 1 + length >= sizeof(out.path)) return false;
      out.path[pathSize++] = '/';
      for (size_t i = 0; i < length; ++i) {
        if (data[at + i] < 0x21 || data[at + i] > 0x7e) return false;
        out.path[pathSize++] = data[at + i];
      }
      out.path[pathSize] = 0;
    } else if ((option & 1) && option != 3 && option != 7 && option != 15 && option != 17) out.unsupportedOption = true;
    at += length;
  }
  return true;
}
inline size_t coapResponse(const CoapMessage& req, uint8_t code, const char* body,
                           bool json, uint8_t* output, size_t capacity) {
  const size_t length = strlen(body), needed = 4 + req.tokenLength + (length ? 3 + length : 0);
  if (needed > capacity) return 0;
  output[0] = (req.type == 0 ? 0x60 : 0x50) | req.tokenLength;
  output[1] = code; output[2] = req.mid >> 8; output[3] = req.mid;
  memcpy(output + 4, req.token, req.tokenLength);
  size_t at = 4 + req.tokenLength;
  if (length) {
    output[at++] = json ? 0xc1 : 0xc0;
    if (json) output[at++] = 50;
    output[at++] = 255; memcpy(output + at, body, length); at += length;
  }
  return at;
}
inline size_t coapReport(uint16_t mid, const char* body, uint8_t* output, size_t capacity) {
  const uint8_t prefix[] = {0x42, 2, uint8_t(mid >> 8), uint8_t(mid), uint8_t(mid >> 8), uint8_t(mid),
                            0xb6, 'a','e','t','h','e','r', 0x05, 'n','o','d','e','B', 0x11, 50, 255};
  const size_t length = strlen(body);
  if (sizeof(prefix) + length > capacity) return 0;
  memcpy(output, prefix, sizeof(prefix)); memcpy(output + sizeof(prefix), body, length);
  return sizeof(prefix) + length;
}
}  // namespace aether
