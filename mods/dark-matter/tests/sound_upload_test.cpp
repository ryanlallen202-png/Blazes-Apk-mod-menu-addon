// Executes the production SoundLoadTick with a bounded, fault-injecting audio API.
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <vector>
struct Il2CppObject {};
static Il2CppObject clip;
static bool failRoots = false, failArray = false, failCreate = false, failUpload = false, throwUpload = false;
static int roots = 0, creates = 0, destroys = 0, stopped = 0, stages = 0;
struct Ref {
    Il2CppObject *p = nullptr;
    void clear() { if (p) --roots; p = nullptr; }
    void set(Il2CppObject *o) { clear(); if (o && !failRoots) { p = o; ++roots; } }
    Il2CppObject *get() const { return p; }
};
static Ref g_soundClip;
static bool Alive(Il2CppObject *o) { return o != nullptr; }
static bool g_voiceOverrideActive = false, g_soundLocalStarted = false;
static double g_time = 10, g_voiceRestoreAt = 0, g_soundDuration = 0;
static std::atomic<int> g_soundDecodeState{0};
static std::mutex g_soundMutex;
static int g_soundDecodedChannels = 0, g_soundDecodedRate = 0, g_soundLoadChannels = 0, g_soundLoadFrames = 0, g_soundLastProgress = -1;
static size_t g_soundDecodedFrames = 0, g_soundLoadOffset = 0;
static constexpr size_t kMaxSoundSamples = 8000000;
static std::vector<float> g_soundPcm, uploaded;
static std::vector<bool> written;
static std::vector<int> offsets;
static std::vector<uint8_t> buffer;
static size_t arraySamples = 0;
enum class SoundStartPhase { Idle, Source, Local, Voice };
static SoundStartPhase g_soundStartPhase = SoundStartPhase::Idle;
struct Method { int id; bool IsValid() const { return true; } };
[[maybe_unused]] static bool AudioCallable(const Method &) { return true; }
static void *ArrayNew(void *, uintptr_t n) {
    if (failArray) return nullptr;
    arraySamples = n; buffer.assign(32 + n * sizeof(float), 0);
    return buffer.data();
}
static struct { Method audioClipCreate{1}, audioClipSetData{2}, destroy{3}; void *floatClass = &clip;
    void *(*arrayNew)(void *, uintptr_t) = ArrayNew; } G;
namespace ym { static void log(const char *, ...) {} }
static void Notify(const char *, ...) {}
static const char *CreateMonoString(const char *s) { return s; }
static void CheckVoiceTransmission() {}
static void RestoreVoiceInput() { g_voiceOverrideActive = false; }
static void SoundStartTick() { ++stages; g_soundStartPhase = SoundStartPhase::Idle; }
static void StopSound() {
    ++stopped; g_soundClip.clear(); g_soundPcm.clear(); g_soundDecodeState.store(0);
    g_soundStartPhase = SoundStartPhase::Idle;
}
static bool Upload(Method, Il2CppObject *target, Il2CppObject *array, int frameOffset) {
    assert(target == &clip && array == reinterpret_cast<Il2CppObject *>(buffer.data()));
    assert(roots == 2); // both the clip and temporary float[] must be retained
    if (throwUpload) throw std::runtime_error("managed upload failure");
    if (failUpload) return false;
    assert(frameOffset >= 0 && arraySamples % g_soundLoadChannels == 0);
    const size_t begin = size_t(frameOffset) * g_soundLoadChannels;
    assert(begin + arraySamples <= uploaded.size());
    offsets.push_back(frameOffset);
    for (size_t i = 0; i < arraySamples; ++i) {
        assert(!written[begin + i]); written[begin + i] = true;
        std::memcpy(&uploaded[begin + i], buffer.data() + 32 + i * sizeof(float), sizeof(float));
    }
    return true;
}
template<class Result, class... Args>
static Result AudioCall(Method method, Il2CppObject *target, Args... args) {
    if constexpr (std::is_same_v<Result, Il2CppObject *>) {
        assert(method.id == 1 && !target); ++creates; return failCreate ? nullptr : &clip;
    } else if constexpr (std::is_same_v<Result, bool>) return Upload(method, target, args...);
    else { assert(method.id == 3 && !target); ++destroys; }
}
#include "sound_upload.inc"
static void Reset(int channels = 2, int frames = 5003) {
    g_soundClip.clear(); assert(roots == 0);
    failRoots = failArray = failCreate = failUpload = throwUpload = false;
    creates = destroys = stopped = stages = 0;
    offsets.clear(); buffer.clear(); g_soundStartPhase = SoundStartPhase::Idle;
    g_soundDecodedChannels = channels; g_soundDecodedRate = 48000; g_soundDecodedFrames = frames;
    g_soundPcm.resize(size_t(channels) * frames);
    for (size_t i = 0; i < g_soundPcm.size(); ++i) g_soundPcm[i] = float(i) / float(g_soundPcm.size());
    uploaded.assign(g_soundPcm.size(), -1); written.assign(g_soundPcm.size(), false);
    g_soundDecodeState.store(2);
}
int main() {
    for (int channels : {1,2}) for (int frames : {1,4096,4097,10003}) {
        Reset(channels, frames); const auto expected = g_soundPcm;
        SoundLoadTick(); assert(creates == 1 && offsets.empty() && roots == 1);
        for (int i = 0; i < (frames + 4095) / 4096; ++i) SoundLoadTick();
        assert(uploaded == expected && std::all_of(written.begin(), written.end(), [](bool b) { return b; }));
        for (size_t i = 0; i < offsets.size(); ++i) assert(offsets[i] == int(i * 4096));
        assert(g_soundPcm.empty() && g_soundDecodeState.load() == 0);
        assert(g_soundStartPhase == SoundStartPhase::Source && stages == 0);
        SoundLoadTick(); assert(stages == 1);
    }
    for (int state : {0,1,3}) {
        Reset(); g_soundDecodeState.store(state); SoundLoadTick();
        assert(creates == 0 && offsets.empty());
    }
    Reset(3); SoundLoadTick(); assert(stopped == 1 && creates == 0);
    Reset(2,0); SoundLoadTick(); assert(stopped == 1 && creates == 0);
    Reset(); failCreate = true; SoundLoadTick(); assert(stopped == 1 && !roots);
    Reset(); SoundLoadTick(); failArray = true; SoundLoadTick(); assert(stopped == 1 && !roots);
    Reset(); SoundLoadTick(); failRoots = true; SoundLoadTick(); assert(stopped == 1 && !roots && offsets.empty());
    Reset(); SoundLoadTick(); failUpload = true; SoundLoadTick(); assert(stopped == 1 && !roots);
    Reset(); SoundLoadTick(); throwUpload = true;
    bool caught = false; try { SoundLoadTick(); } catch (...) { caught = true; }
    assert(caught && roots == 1); StopSound(); assert(!roots);
    // A failed clip GC root must stop, not allocate another native clip next frame.
    Reset(); failRoots = true; SoundLoadTick(); SoundLoadTick();
    assert(creates == 1 && stopped == 1 && destroys == 1 && roots == 0);
    g_soundClip.clear();
}
