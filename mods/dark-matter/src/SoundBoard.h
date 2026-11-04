#pragma once

// Adapted from the user's SoundBoard.h for this repository's SDK. Keep decoding
// independent of Unity so it can be tested without a headset. No external SDK wrappers.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace diddy::soundboard {
inline constexpr const char *kVoiceObjectName = "PhotonVRVoice";

struct WavInfo {
    int channels = 0, sampleRate = 0;
    uint16_t format = 0, bits = 0;
    size_t dataOffset = 0, dataBytes = 0;
};
inline uint16_t Read16(const std::vector<uint8_t> &bytes, size_t offset) {
    return uint16_t(bytes[offset]) | (uint16_t(bytes[offset + 1]) << 8);
}
inline uint32_t Read32(const std::vector<uint8_t> &bytes, size_t offset) {
    return uint32_t(bytes[offset]) | (uint32_t(bytes[offset + 1]) << 8) |
           (uint32_t(bytes[offset + 2]) << 16) | (uint32_t(bytes[offset + 3]) << 24);
}
inline bool ParseWav(const std::vector<uint8_t> &bytes, WavInfo &info) {
    info = {};
    if (bytes.size() < 44 || std::memcmp(bytes.data(), "RIFF", 4) ||
        std::memcmp(bytes.data() + 8, "WAVE", 4)) return false;
    const size_t riffBytes = Read32(bytes, 4);
    if (riffBytes < 36 || riffBytes > bytes.size() - 8) return false;
    const size_t end = riffBytes + 8;
    bool haveFormat = false, haveData = false;
    uint16_t alignment = 0;
    for (size_t offset = 12; offset < end;) {
        if (end - offset < 8) return false;
        const size_t count = Read32(bytes, offset + 4), body = offset + 8;
        if (count > end - body) return false;
        if (!std::memcmp(bytes.data() + offset, "fmt ", 4)) {
            if (haveFormat || count < 16) return false;
            info.format = Read16(bytes, body);
            info.channels = Read16(bytes, body + 2);
            const uint32_t rate = Read32(bytes, body + 4);
            if (rate < 8000 || rate > 48000) return false;
            info.sampleRate = int(rate);
            alignment = Read16(bytes, body + 12);
            info.bits = Read16(bytes, body + 14);
            haveFormat = true;
        } else if (!std::memcmp(bytes.data() + offset, "data", 4)) {
            if (haveData) return false;
            info.dataOffset = body;
            info.dataBytes = count;
            haveData = true;
        }
        offset = body + count;
        if (count & 1u) {
            if (offset == end) return false;
            ++offset;
        }
    }
    if (!haveFormat || !haveData || (info.channels != 1 && info.channels != 2)) return false;
    const bool supported = (info.format == 1 && (info.bits == 8 || info.bits == 16)) ||
                           (info.format == 3 && info.bits == 32);
    if (!supported) return false;
    const size_t frameBytes = size_t(info.channels) * (info.bits / 8);
    return alignment == frameBytes && info.dataBytes >= frameBytes && info.dataBytes % frameBytes == 0;
}
inline bool DecodeWav(const std::vector<uint8_t> &bytes, WavInfo &info,
                      std::vector<float> &samples, size_t maxSamples) {
    samples.clear();
    if (!ParseWav(bytes, info)) return false;
    const size_t width = info.bits / 8, count = info.dataBytes / width;
    if (count > maxSamples) return false;
    samples.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const size_t offset = info.dataOffset + i * width;
        float value;
        if (info.format == 3) {
            const uint32_t raw = Read32(bytes, offset);
            static_assert(sizeof(value) == sizeof(raw));
            std::memcpy(&value, &raw, sizeof(value));
            // NaN is not made finite by clamp and must not enter Unity/voice buffers.
            if (!std::isfinite(value)) { samples.clear(); return false; }
            value = std::clamp(value, -1.0f, 1.0f);
        } else if (info.bits == 8) {
            value = (int(bytes[offset]) - 128) / 128.0f;
        } else {
            const uint16_t raw = Read16(bytes, offset);
            const int sample = raw < 32768 ? int(raw) : int(raw) - 65536;
            value = sample / 32768.0f;
        }
        samples[i] = value;
    }
    return true;
}
} // namespace diddy::soundboard
