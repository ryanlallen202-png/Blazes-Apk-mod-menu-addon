#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace diddy::combined {
inline float SafeDt(float dt) { return std::isfinite(dt) ? std::clamp(dt, 0.0f, 0.05f) : 0.0f; }
inline bool RollSpeed(float original, float &result) {
    if (!std::isfinite(original) || original <= 0 || original > 40) return false;
    result = std::min(original * 2, 40.0f);
    return true;
}
struct TriggerGate {
    bool previous = false;
    double next = 0;
    bool Take(bool aiming, bool trigger, double now) {
        const bool edge = trigger && !previous;
        previous = trigger;
        if (!aiming || !edge || !std::isfinite(now) || now < next) return false;
        next = now + 0.5;
        return true;
    }
    void Reset() { previous = true; next = 0; } // require release after room changes
};
// Bounded UTF-16 conversion for signs and object labels. Do not interpret rich-text
// markup, copy unbounded room strings, or read past a truncated surrogate pair.
inline std::string PlainText(const char16_t *chars, int length) {
    std::string out;
    if (!chars || length <= 0) return out;
    const int count = std::min(length, 180);
    out.reserve(180);
    for (int i = 0; i < count; ++i) {
        uint32_t c = chars[i];
        if (c >= 0xd800 && c <= 0xdbff) {
            if (i + 1 < count && chars[i + 1] >= 0xdc00 && chars[i + 1] <= 0xdfff)
                c = 0x10000 + ((c - 0xd800) << 10) + (chars[++i] - 0xdc00);
            else c = 0xfffd;
        } else if (c >= 0xdc00 && c <= 0xdfff) c = 0xfffd;
        if (c < 32 || c == 127 || c == '<' || c == '>') c = ' ';
        const size_t width = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
        if (out.size() + width > 180) break;
        if (width == 1) out.push_back(char(c));
        else {
            if (width == 4) out.push_back(char(0xf0 | (c >> 18)));
            if (width >= 3) out.push_back(char((width == 3 ? 0xe0 : 0x80) | ((c >> 12) & (width == 3 ? 15 : 63))));
            out.push_back(char((width == 2 ? 0xc0 : 0x80) | ((c >> 6) & (width == 2 ? 31 : 63))));
            out.push_back(char(0x80 | (c & 63)));
        }
    }
    return out;
}
} // namespace diddy::combined
