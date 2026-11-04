// Host simulation of Photon/Unity. voice_under_test.inc is extracted from menu.cpp
// by test_soundboard.py: the routing/restoration implementation is not duplicated.
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

struct Il2CppObject { bool alive = true; };
enum Op { None, GetTransmit, GetRecording, GetEnabled, GetSource, GetClip, GetLoop,
          SetSource, SetClip, SetLoop, Start, Stop, SetRecording, SetEnabled, GetTransmitting, GetHost, AddSource,
          LocalVolume, LocalSpatial, LocalLoop, LocalClip, LocalPlay };
struct MethodBase { Op op; bool available = true; bool IsValid() const { return available; } };
template<class T> struct Method : MethodBase {
    Method(Op op = None) : MethodBase{op, true} {}
};
template<class T> struct Field {
    T *value = nullptr;
    bool IsValid() const { return value != nullptr; }
    Field &operator[](Il2CppObject *) { return *this; }
    T operator()() const { assert(value); return *value; }
};
static bool failRoots = false;
struct Ref {
    Il2CppObject *object = nullptr;
    void set(Il2CppObject *value) { object = failRoots ? nullptr : value; }
    void clear() { object = nullptr; }
    Il2CppObject *get() const { return object; }
};
struct Bindings {
    Method<bool> voiceGetTransmitEnabled{GetTransmit}, voiceGetIsRecording{GetRecording},
        voiceGetRecordingEnabled{GetEnabled}, voiceGetLoopAudioClip{GetLoop}, voiceGetIsCurrentlyTransmitting{GetTransmitting};
    Method<int> voiceGetSourceType{GetSource};
    Method<Il2CppObject *> voiceGetAudioClip{GetClip};
    Method<void> voiceSetSourceType{SetSource}, voiceSetAudioClip{SetClip}, voiceSetLoopAudioClip{SetLoop},
        voiceStartRecording{Start}, voiceStopRecording{Stop}, voiceSetIsRecording{SetRecording},
        voiceSetRecordingEnabled{SetEnabled};
    Method<Il2CppObject *> compGameObject{GetHost}, goAddComponent{AddSource};
    Method<void> asSetVolume{LocalVolume}, asSetSpatial{LocalSpatial}, asSetLoop{LocalLoop},
        asSetClip{LocalClip}, asPlay{LocalPlay};
    void *tAudioSource = (void *)1;
    Field<int> voiceSourceTypeField;
    Field<Il2CppObject *> voiceClipField;
    Field<bool> voiceLoopField, voiceTransmitField, voiceRecordingField, voiceRecordingEnabledField;
} G;
static Il2CppObject recorder, oldClip, newClip, headTransform, headObject, audioSource;
struct LocalState {
    bool playing = false, loop = true;
    float volume = 1, spatial = 1;
    Il2CppObject *clip = nullptr;
} local;
enum class SoundStartPhase { Idle, Source, Local, Voice };
static SoundStartPhase g_soundStartPhase = SoundStartPhase::Idle;
static Ref g_head, g_soundSource, g_soundClip;
static bool g_soundRoomPlayback = true, g_soundLocalStarted = false;
static double g_soundDuration = 2, g_soundNextAllowed = 0;
static float g_soundVolume = 0.75f;
static std::string g_soundPlayingPath = "fixture.wav";
struct State {
    bool transmit = true, recording = true, enabled = true, loop = true;
    bool present = true, noOpStartOnce = false;
    int source = 0;
    Il2CppObject *clip = &oldClip;
} state;
static std::vector<Op> calls;
static std::map<Op, int> failures;
static std::vector<std::string> logs;
static bool g_voiceOverrideActive = false, g_voiceWasRecording = false, g_voicePreviousLoop = true;
static int g_voicePreviousSourceType = 0, g_voiceRestoreAttempts = 0;
static double g_time = 10, g_voiceRestoreAt = 0, g_voiceCheckAt = 0;
static Ref g_voiceRecorder, g_voicePreviousClip;
static bool Alive(Il2CppObject *object) { return object && object->alive; }
static bool AudioCallable(const MethodBase &method) { return method.available; }
static Il2CppObject *FindVoiceRecorder() { return state.present ? &recorder : nullptr; }
static void Notify(const char *, ...) {}
namespace ym {
void log(const char *format, ...) {
    char line[320];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    logs.emplace_back(line);
}
}
template<class Result, class... Args>
static Result AudioCall(const MethodBase &method, Il2CppObject *target, Args... args) {
    assert(method.available);
    if (method.op < GetHost) assert(target == &recorder);
    else if (method.op == GetHost) assert(target == &headTransform);
    else if (method.op == AddSource) assert(target == &headObject);
    else assert(target == &audioSource);
    calls.push_back(method.op);
    if (failures[method.op] > 0) {
        --failures[method.op];
        throw std::runtime_error("simulated managed failure");
    }
    if constexpr (sizeof...(Args) == 1) {
        auto set = [&](auto value) {
            using T = decltype(value);
            if constexpr (std::is_same_v<T, bool>) {
                if (method.op == SetLoop) state.loop = value;
                if (method.op == LocalLoop) local.loop = value;
                if (method.op == SetRecording || method.op == SetEnabled)
                    state.recording = state.enabled = value;
            } else if constexpr (std::is_same_v<T, float>) {
                if (method.op == LocalVolume) local.volume = value;
                if (method.op == LocalSpatial) local.spatial = value;
            } else if constexpr (std::is_same_v<T, int>) {
                assert(method.op == SetSource);
                state.source = value;
            } else if constexpr (std::is_same_v<T, Il2CppObject *>) {
                if (method.op == SetClip) state.clip = value;
                else { assert(method.op == LocalClip); local.clip = value; }
            }
        };
        (set(args), ...);
    }
    if (method.op == LocalPlay) local.playing = true;
    if (method.op == Stop) state.recording = false;
    if (method.op == Start) {
        if (state.noOpStartOnce) state.noOpStartOnce = false;
        else state.recording = true;
    }
    if constexpr (std::is_void_v<Result>) return;
    else if constexpr (std::is_same_v<Result, bool>) {
        switch (method.op) {
        case GetTransmit: return state.transmit;
        case GetRecording: return state.recording;
        case GetEnabled: return state.enabled;
        case GetLoop: return state.loop;
        case GetTransmitting: return state.transmit && state.recording && state.source == 1;
        default: throw std::runtime_error("unexpected bool accessor");
        }
    } else if constexpr (std::is_same_v<Result, int>) return state.source;
    else {
        if (method.op == GetHost) return &headObject;
        if (method.op == AddSource) return &audioSource;
        return state.clip;
    }
}

#include "voice_under_test.inc"

static void Reset() {
    G = {};
    state = {};
    failRoots = false;
    g_time = 10;
    g_voiceCheckAt = 0;
    recorder.alive = headTransform.alive = headObject.alive = audioSource.alive = true;
    local = {};
    g_soundStartPhase = SoundStartPhase::Idle;
    g_soundLocalStarted = false;
    g_soundRoomPlayback = true;
    g_soundNextAllowed = 0;
    g_head.set(&headTransform);
    g_soundSource.clear();
    g_soundClip.set(&newClip);
    calls.clear(); failures.clear(); logs.clear();
    g_voiceOverrideActive = g_voiceWasRecording = false;
    g_voiceRestoreAttempts = 0;
    g_voiceRestoreAt = 0;
    g_voiceRecorder.clear(); g_voicePreviousClip.clear();
}
static void Legacy() {
    G.voiceGetIsRecording.available = G.voiceSetIsRecording.available = false;
}
static void Direct() {
    G.voiceSetIsRecording.available = G.voiceSetRecordingEnabled.available = false;
}
static void AssertRestored() {
    assert(state.source == 0 && state.clip == &oldClip && state.loop);
    assert(state.recording && state.transmit);
    assert(!g_voiceOverrideActive && !g_voiceRecorder.get() && !g_voicePreviousClip.get());
}
int main() {
    // The same implementation must support modern properties, legacy enablement,
    // and direct start/stop. All must restore the exact previous input.
    for (int variant = 0; variant < 3; ++variant) {
        Reset();
        if (variant == 1) Legacy();
        if (variant == 2) Direct();
        assert(StartVoicePlayback(&newClip, 2.0));
        assert(state.source == 1 && state.clip == &newClip && !state.loop);
        assert(state.recording && g_voiceOverrideActive && g_voiceRestoreAt == g_time + 2.0);
        RestoreVoiceInput();
        AssertRestored();
    }
    Reset(); state.transmit = false;
    assert(!StartVoicePlayback(&newClip, 2.0));
    assert(state.source == 0 && state.clip == &oldClip && !g_voiceOverrideActive);
    Reset(); state.recording = false;
    assert(!StartVoicePlayback(&newClip, 2.0));
    assert(!state.recording && state.source == 0);
    Reset(); Legacy(); state.enabled = false;
    assert(!StartVoicePlayback(&newClip, 2.0));
    assert(!state.enabled && state.source == 0);
    Reset(); state.present = false;
    assert(!StartVoicePlayback(&newClip, 2.0));
    assert(calls.empty());
    Reset(); Legacy(); G.voiceGetRecordingEnabled.available = false;
    assert(!StartVoicePlayback(&newClip, 2.0));
    assert(calls.empty());
    Reset(); Legacy(); G.voiceGetRecordingEnabled.available = false;
    G.voiceRecordingEnabledField.value = &state.enabled;
    assert(StartVoicePlayback(&newClip, 2.0));
    RestoreVoiceInput(); AssertRestored();
    // Failure part-way through switching must roll back to the mic.
    for (Op op : {SetClip, SetLoop, SetSource, SetRecording}) {
        Reset(); failures[op] = 1;
        assert(!StartVoicePlayback(&newClip, 2.0));
        AssertRestored();
    }
    Reset(); Direct(); state.noOpStartOnce = true;
    assert(!StartVoicePlayback(&newClip, 2.0));
    AssertRestored();
    // A transient restoration failure must retain the snapshot for retry, rather
    // than losing the mic configuration and releasing a clip still in use.
    Reset(); assert(StartVoicePlayback(&newClip, 2.0));
    failures[SetSource] = 1;
    RestoreVoiceInput();
    assert(g_voiceOverrideActive && g_voiceRecorder.get() == &recorder);
    assert(g_voicePreviousClip.get() == &oldClip);
    calls.clear();
    assert(!StartVoicePlayback(&newClip, 2.0));
    assert(calls.empty());
    RestoreVoiceInput(); AssertRestored();
    // Bounded automatic restore attempts retain ownership; a later manual retry
    // can recover without losing the original settings.
    Reset(); assert(StartVoicePlayback(&newClip, 2.0));
    failures[SetSource] = 3;
    for (int attempt = 1; attempt <= 3; ++attempt) {
        RestoreVoiceInput();
        assert(g_voiceOverrideActive && g_voiceRestoreAttempts == attempt);
        if (attempt < 3) assert(g_voiceRestoreAt == g_time + 0.25);
        else assert(g_voiceRestoreAt == -1);
    }
    RestoreVoiceInput(); AssertRestored();
    // Snapshot failure or failed GC retention must not modify the microphone.
    Reset(); failures[GetClip] = 1;
    bool caught = false;
    try { StartVoicePlayback(&newClip, 2.0); } catch (...) { caught = true; }
    assert(caught && !g_voiceRecorder.get() && !g_voiceOverrideActive && state.source == 0);
    Reset(); failRoots = true;
    assert(!StartVoicePlayback(&newClip, 2.0));
    assert(!g_voiceOverrideActive && state.source == 0 && state.recording);
    // A live observation is made at most once, and is not a remote-receipt claim.
    Reset(); assert(StartVoicePlayback(&newClip, 2.0));
    calls.clear();
    CheckVoiceTransmission(); assert(calls.empty());
    g_time += 0.5; CheckVoiceTransmission();
    assert(calls.size() == 1 && calls[0] == GetTransmitting);
    CheckVoiceTransmission(); assert(calls.size() == 1);
    RestoreVoiceInput(); AssertRestored();
    Reset(); assert(StartVoicePlayback(&newClip, 2.0));
    state.transmit = false; // muting mid-clip must remain in effect after restoring
    g_time += 0.5; CheckVoiceTransmission(); RestoreVoiceInput();
    assert(!state.transmit && !g_voiceOverrideActive && state.source == 0 && state.recording);
    // Execute the actual post-100% state machine, not a rewritten implementation.
    Reset(); g_soundStartPhase = SoundStartPhase::Source;
    SoundStartTick();
    assert(g_soundStartPhase == SoundStartPhase::Local && !local.playing && state.source == 0);
    assert(g_soundSource.get() == &audioSource && local.spatial == 0 && !local.loop);
    SoundStartTick();
    assert(g_soundStartPhase == SoundStartPhase::Voice && local.playing && local.clip == &newClip);
    assert(local.volume == 0.75f && !g_voiceOverrideActive && state.source == 0);
    g_time += 0.1;
    SoundStartTick();
    assert(g_soundStartPhase == SoundStartPhase::Idle && g_voiceOverrideActive && state.source == 1);
    assert(g_soundNextAllowed == g_time + g_soundDuration);
    RestoreVoiceInput(); AssertRestored();
    Reset(); g_soundRoomPlayback = false; g_soundStartPhase = SoundStartPhase::Source;
    SoundStartTick(); SoundStartTick(); SoundStartTick();
    assert(local.playing && !g_voiceOverrideActive && state.source == 0);
    Reset(); state.transmit = false; g_soundStartPhase = SoundStartPhase::Source;
    SoundStartTick(); SoundStartTick(); SoundStartTick();
    assert(local.playing && !g_voiceOverrideActive && !state.transmit);
    for (Op op : {AddSource, LocalPlay}) {
        Reset(); g_soundStartPhase = SoundStartPhase::Source;
        failures[op] = 1;
        bool caughtFailure = false;
        try { SoundStartTick(); SoundStartTick(); } catch (...) { caughtFailure = true; }
        assert(caughtFailure && !g_voiceOverrideActive && state.source == 0);
    }
    // A destroyed recorder requires no Unity calls and must release snapshots.
    Reset(); assert(StartVoicePlayback(&newClip, 2.0));
    recorder.alive = false;
    calls.clear(); RestoreVoiceInput();
    assert(calls.empty() && !g_voiceOverrideActive && !g_voiceRecorder.get());
}
