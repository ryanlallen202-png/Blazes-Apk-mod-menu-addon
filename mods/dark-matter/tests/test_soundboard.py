"""Host-side regression checks; these do not replace Unity/headset tests.
Run: python3 mods/dark-matter/tests/test_soundboard.py
"""
import pathlib
import shutil
import struct
import subprocess
import tempfile
import unittest

SOURCE = pathlib.Path(__file__).resolve().parents[1] / "src/menu.cpp"


class SoundboardTests(unittest.TestCase):
    def test_wav_parser(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")
        source = SOURCE.read_text()
        parser = source[source.index("struct WavInfo"):source.index("static std::string PersistentPath")]

        def wav(channels=1, rate=44100, data=b"\0\0" * 8, bits=16):
            fmt = struct.pack("<HHIIHH", 1, channels, rate, rate * channels * 2, channels * 2, bits)
            body = b"WAVEfmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
            return b"RIFF" + struct.pack("<I", len(body)) + body

        cases = [(wav(), True), (wav(channels=2), True), (wav(rate=8000), True),
                 (wav(rate=48000), True), (wav(channels=3), False),
                 (wav(rate=7999), False), (wav(rate=48001), False),
                 (wav(bits=8), False), (wav(data=b""), False),
                 (wav(channels=2, data=b"\0\0"), False),
                 (wav()[:-1], False), (b"not a WAV", False)]
        checks = []
        for data, expected in cases:
            values = ",".join(str(v) for v in data)
            checks.append("{ WavInfo w; std::vector<uint8_t> b = {" + values + "}; assert(ParseWav(b,w) == " + str(expected).lower() + "); }")
        program = "#include <vector>\n#include <cstdint>\n#include <cstring>\n#include <cassert>\n" + parser + "\nint main() {\n" + "\n".join(checks) + "\n}\n"
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "wav_test.cpp"
            exe = pathlib.Path(directory) / "wav_test"
            cpp.write_text(program)
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_runtime_audio_marshalling(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")
        source = SOURCE.read_text()
        helper = source[source.index("static Il2CppObject *(*p_audioInvoke)"):source.index("static Color Lerp")]
        program = '''#include <cassert>
#include <type_traits>
#include <stdexcept>
namespace BNM::IL2CPP {
struct MethodInfo {
    const char *name;
    void *methodPointer = (void *)1;
    void *invoker_method = (void *)1;
    unsigned parameters_count = 0;
    unsigned flags = 0;
};
struct Il2CppException {};
}
struct Il2CppObject { bool value; };
struct MethodBase {
    bool valid = true;
    BNM::IL2CPP::MethodInfo info{"test"};
    bool IsValid() const { return valid; }
    const BNM::IL2CPP::MethodInfo *GetInfo() const { return &info; }
};
namespace ym { void log(const char *, ...) {} }
'''+helper+'''
static Il2CppObject instance{false}, reference{false}, boxed{true};
static BNM::IL2CPP::Il2CppException exception;
static int mode = 0;
static Il2CppObject *Invoke(const BNM::IL2CPP::MethodInfo *, void *target, void **args,
                           BNM::IL2CPP::Il2CppException **error) {
    assert(target == &instance);
    if (mode == 0) {
        assert(args[0] == &reference);
        assert(*(int *)args[1] == 4096);
        assert(*(float *)args[2] == 0.75f);
        assert(*(bool *)args[3] == false);
        assert(args[4] == nullptr);
    } else {
        assert(args == nullptr);
        if (mode == 2) *error = &exception;
    }
    return &boxed;
}
static void *Unbox(Il2CppObject *object) { return &object->value; }
int main() {
    MethodBase method;
    p_audioInvoke = Invoke;
    p_audioUnbox = Unbox;
    method.info.parameters_count = 5;
    assert(AudioCall<bool>(method, &instance, &reference, 4096, 0.75f, false, (Il2CppObject *)nullptr));
    method.info.parameters_count = 0;
    mode = 1;
    assert(AudioCall<Il2CppObject *>(method, &instance) == &boxed);
    AudioCall<void>(method, &instance);
    mode = 2;
    bool caught = false;
    try { AudioCall<void>(method, &instance); } catch (const std::runtime_error &) { caught = true; }
    assert(caught);
    mode = 1;
    method.info.invoker_method = nullptr;
    caught = false;
    try { AudioCall<void>(method, &instance); } catch (const std::runtime_error &) { caught = true; }
    assert(caught);
    method.info.invoker_method = (void *)1;
    method.info.methodPointer = nullptr;
    caught = false;
    try { AudioCall<void>(method, &instance); } catch (const std::runtime_error &) { caught = true; }
    assert(caught);
    method.info.methodPointer = (void *)1;
    method.info.parameters_count = 1;
    caught = false;
    try { AudioCall<void>(method, &instance); } catch (const std::runtime_error &) { caught = true; }
    assert(caught);
    method.info.parameters_count = 0;
    caught = false;
    try { AudioCall<void>(method, nullptr); } catch (const std::runtime_error &) { caught = true; }
    assert(caught);
    p_audioInvoke = nullptr;
    caught = false;
    try { AudioCall<void>(method, &instance); } catch (const std::runtime_error &) { caught = true; }
    assert(caught);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "invoke_test.cpp"
            exe = pathlib.Path(directory) / "invoke_test"
            cpp.write_text(program)
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_gc_handle_replacement(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")
        source = SOURCE.read_text()
        helper = source[source.index("using GcHandle ="):source.index("// Audio calls use")]
        program = '''#include <cstdint>
#include <cassert>
#include <map>
namespace BNM::IL2CPP { using Il2CppGCHandle = void *; }
struct Il2CppObject {};
'''+helper+'''
static std::map<GcHandle, Il2CppObject *> handles;
// Exercise the bits a 32-bit handle would lose on the arm64 target.
static uintptr_t nextHandle = (uintptr_t{1} << 40) | 0x1234;
static_assert(sizeof(GcHandle) == sizeof(void *));
static bool replacing = false;
static GcHandle New(Il2CppObject *object, bool pinned) {
    assert(!pinned);
    if (replacing) assert(!handles.empty());
    auto handle = reinterpret_cast<GcHandle>(nextHandle++);
    handles[handle] = object;
    return handle;
}
static Il2CppObject *Target(GcHandle handle) { return handles.at(handle); }
static void Free(GcHandle handle) { assert(handles.erase(handle) == 1); }
int main() {
    p_handleNew = New; p_handleTarget = Target; p_handleFree = Free;
    Il2CppObject object;
    Ref ref;
    ref.set(&object);
    assert(reinterpret_cast<uintptr_t>(ref.h) > UINT32_MAX);
    assert(ref.get() == &object);
    replacing = true;
    ref.set(ref.get());
    assert(ref.get() == &object && handles.size() == 1);
    ref.clear();
    assert(handles.empty() && ref.get() == nullptr);
    ref.clear();
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "handle_test.cpp"
            exe = pathlib.Path(directory) / "handle_test"
            cpp.write_text(program)
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_startup_retries(self):
        source = SOURCE.read_text()
        start = source.index("static void OnUpdate(float dt) {")
        update = source[start:source.index("YM_EXPORT int YeepsMod_Init", start)]
        self.assertIn("if (!Alive(g_leftHand.get())) g_leftHand.set", update)
        self.assertIn("if (!Alive(g_rightHand.get())) g_rightHand.set", update)
        self.assertIn("if (!Alive(g_head.get())) g_head.set", update)
        self.assertIn("if (!g_built) return;", update)
        self.assertIn("ym::onRoomChange(OnRoomChange)", source)
        self.assertIn("using GcHandle = BNM::IL2CPP::Il2CppGCHandle;", source)
        self.assertNotIn("static uint32_t (*p_handleNew)", source)

    def test_menu_syntax(self):
        """Parse the full translation unit; platform declarations are host-only shims."""
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")
        root = SOURCE.parents[3]
        with tempfile.TemporaryDirectory() as directory:
            include = pathlib.Path(directory)
            (include / "android").mkdir()
            (include / "jni.h").write_text("#pragma once\nstruct JNIEnv; typedef void *jobject;\n")
            (include / "android/log.h").write_text('#pragma once\nextern "C" int __android_log_print(int, const char *, const char *, ...);\n')
            subprocess.run([compiler, "-std=c++20", "-DNDEBUG", "-D__has_feature(x)=0", "-fsyntax-only",
                            "-I" + directory, "-I" + str(root / "sdk/include"),
                            "-I" + str(root / "sdk/bnm/include"),
                            "-I" + str(root / "sdk/bnm/external/include"), str(SOURCE)], check=True)

    def test_mp3_decoder(self):
        """Generate real mono/stereo MP3 fixtures without committing binary audio."""
        try:
            import lameenc
        except ImportError:
            self.skipTest("Install optional lameenc in a virtualenv for MP3 fixture tests")
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")
        import math
        with tempfile.TemporaryDirectory() as directory:
            tmp = pathlib.Path(directory)
            fixtures = []
            for channels in (1, 2):
                encoder = lameenc.Encoder()
                encoder.set_bit_rate(128)
                encoder.set_in_sample_rate(44100)
                encoder.set_channels(channels)
                encoder.set_quality(2)
                pcm = b"".join(struct.pack("<h", int(4000 * math.sin(2 * math.pi * 440 * n / 44100)))
                               * channels for n in range(44100))
                fixture = tmp / (str(channels) + ".mp3")
                fixture.write_bytes(encoder.encode(pcm) + encoder.flush())
                fixtures.append(str(fixture))
            program = '''#include <cassert>
#include <vector>
#include <cmath>
#include "dr_mp3.h"
int main(int argc, char **argv) {
    assert(argc == 3);
    for (int channels = 1; channels <= 2; ++channels) {
        drmp3 decoder{};
        assert(drmp3_init_file(&decoder, argv[channels], nullptr));
        assert(decoder.channels == (unsigned)channels);
        assert(decoder.sampleRate == 44100);
        auto count = drmp3_get_pcm_frame_count(&decoder);
        assert(count >= 44100 && count < 48000);
        std::vector<float> pcm(count * channels);
        assert(drmp3_read_pcm_frames_f32(&decoder, count, pcm.data()) == count);
        float peak = 0;
        for (float sample : pcm) { assert(std::isfinite(sample)); peak = std::fmax(peak, std::fabs(sample)); }
        assert(peak > 0.01f && peak < 1.0f);
        drmp3_uninit(&decoder);
    }
}
'''
            cpp = tmp / "mp3_test.cpp"
            exe = tmp / "mp3_test"
            cpp.write_text(program)
            subprocess.run([compiler, "-std=c++20", "-I" + str(SOURCE.parent), str(cpp),
                            str(SOURCE.parent / "dr_mp3_impl.cpp"), "-o", str(exe)], check=True)
            subprocess.run([str(exe), *fixtures], check=True)

    def test_exact_audio_bindings_and_recorder_order(self):
        source = SOURCE.read_text()
        self.assertIn('Overload(aclip, "SetData", {"System.Single[]", "System.Int32"})', source)
        self.assertNotIn('aclip.GetMethod("SetData", 2)', source)
        self.assertNotIn("G.soundGameObjectCtor", source)
        self.assertNotIn('G.voiceRestartRecording', source)
        playback_start = source.index("static bool StartVoicePlayback")
        playback = source[playback_start:source.index("// ---- soundboard playback", playback_start)]
        self.assertLess(playback.index("SetVoiceRecording(recorder, false)"),
                        playback.index("AudioCall<void>(G.voiceSetAudioClip, recorder, clip)"))
        self.assertLess(playback.index("AudioCall<void>(G.voiceSetSourceType, recorder, 1)"),
                        playback.index("SetVoiceRecording(recorder, true)"))
        restore_start = source.index("static void RestoreVoiceInput() {")
        restore = source[restore_start:playback_start]
        self.assertLess(restore.index("SetVoiceRecording(recorder, false)"),
                        restore.index("AudioCall<void>(G.voiceSetAudioClip, recorder"))
        self.assertLess(restore.index("AudioCall<void>(G.voiceSetSourceType, recorder"),
                        restore.index("SetVoiceRecording(recorder, true)"))

    def test_recording_property_compatibility(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")
        source = SOURCE.read_text()
        start = source.index("static void SetVoiceRecording(")
        helper = source[start:source.index("static void RestoreVoiceInput() {", start)]
        program = '''#include <cassert>
#include <vector>
struct Il2CppObject {};
struct Method { int id; bool available; };
struct { Method voiceSetIsRecording{1, true}, voiceStartRecording{2, true}, voiceStopRecording{3, true}, voiceSetRecordingEnabled{4, true}; } G;
static std::vector<int> calls;
static bool AudioCallable(const Method &method) { return method.available; }
template<class Result, class... Args>
static Result AudioCall(const Method &method, Il2CppObject *recorder, Args... args) {
    assert(method.available && recorder);
    calls.push_back(method.id);
    if constexpr (sizeof...(Args) == 1) {
        bool values[] = {args...};
        calls.push_back(values[0] ? 10 : 20);
    }
}
'''+helper+'''
int main() {
    Il2CppObject recorder;
    SetVoiceRecording(&recorder, false);
    SetVoiceRecording(&recorder, true);
    assert((calls == std::vector<int>{1, 20, 1, 10}));
    calls.clear();
    G.voiceSetIsRecording.available = false;
    SetVoiceRecording(&recorder, false);
    SetVoiceRecording(&recorder, true);
    assert((calls == std::vector<int>{4, 20, 4, 10}));
    calls.clear();
    G.voiceSetRecordingEnabled.available = false;
    SetVoiceRecording(&recorder, false);
    SetVoiceRecording(&recorder, true);
    assert((calls == std::vector<int>{3, 2}));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "recording_test.cpp"
            exe = pathlib.Path(directory) / "recording_test"
            cpp.write_text(program)
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
        self.assertIn('voice unavailable: %s', source)
        self.assertIn('ReadRecorderValue(G.voiceGetTransmitEnabled, G.voiceTransmitField, recorder)', source)
        self.assertNotIn('set_TransmitEnabled', source)

    def test_legacy_recording_state(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")
        source = SOURCE.read_text()
        start = source.index("static bool VoiceRecordingStateAvailable() {")
        helper = source[start:source.index("static void SetVoiceRecording(", start)]
        program = '''#include <cassert>
#include <stdexcept>
struct Il2CppObject {};
struct Accessor {
    bool available = false, value = false;
    bool IsValid() const { return available; }
};
struct {
    Accessor voiceGetIsRecording, voiceRecordingField, voiceGetRecordingEnabled, voiceRecordingEnabledField;
} G;
static bool AudioCallable(const Accessor &accessor) { return accessor.available; }
static bool ReadRecorderValue(const Accessor &getter, const Accessor &field, Il2CppObject *) {
    if (getter.available) return getter.value;
    if (field.available) return field.value;
    throw std::runtime_error("state unavailable");
}
'''+helper+'''
int main() {
    Il2CppObject recorder;
    assert(!VoiceRecordingStateAvailable());
    bool caught = false;
    try { ReadVoiceRecordingState(&recorder); } catch (const std::runtime_error &) { caught = true; }
    assert(caught);
    G.voiceGetRecordingEnabled = {true, true};
    assert(VoiceRecordingStateAvailable() && ReadVoiceRecordingState(&recorder));
    G.voiceGetRecordingEnabled.value = false;
    assert(!ReadVoiceRecordingState(&recorder));
    G.voiceGetRecordingEnabled.available = false;
    G.voiceRecordingEnabledField = {true, true};
    assert(ReadVoiceRecordingState(&recorder));
    G.voiceGetIsRecording = {true, false};
    assert(!ReadVoiceRecordingState(&recorder)); // live state takes precedence
    G.voiceGetIsRecording.value = true;
    assert(ReadVoiceRecordingState(&recorder));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "legacy_state_test.cpp"
            exe = pathlib.Path(directory) / "legacy_state_test"
            cpp.write_text(program)
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
        self.assertIn('get_RecordingEnabled', source)
        self.assertIn('set_RecordingEnabled', source)
        self.assertIn('legacy recorder enabled; live transmission unverified', source)

    def test_staged_playback_handoff(self):
        source = SOURCE.read_text()
        start = source.index("static void SoundStartTick() {")
        handoff = source[start:source.index("static void SoundLoadTick() {", start)]
        self.assertNotIn("CreateNewInstance", handoff)
        self.assertNotIn("CreateNewObject", handoff)
        self.assertIn("AudioCall<Il2CppObject *>(G.compGameObject, g_head.get())", handoff)
        self.assertIn("AudioCall<Il2CppObject *>(G.goAddComponent, host, G.tAudioSource)", handoff)
        self.assertIn("case SoundStartPhase::Source:", handoff)
        self.assertIn("case SoundStartPhase::Local:", handoff)
        self.assertIn("case SoundStartPhase::Voice:", handoff)
        self.assertIn("Kill(g_soundSource)", source)
        upload = source[source.index("static void SoundLoadTick() {"):source.index("// ---- effects: snow")]
        self.assertNotIn("StartVoicePlayback", upload)
        self.assertNotIn("G.asPlay", upload)
        stop = source[source.index("static void StopSound() {"):source.index("static void SoundVolumeUp() {")]
        self.assertIn("g_soundStartPhase = SoundStartPhase::Idle", stop)

    def test_upload_and_playback_guards(self):
        source = SOURCE.read_text()
        self.assertIn("AudioCall<bool>(G.audioClipSetData, g_soundClip.get(), arr, (int)g_soundLoadOffset)", source)
        self.assertIn("if (state != 2) return;", source)
        self.assertIn("const std::string &p = g_soundPlayingPath;", source)
        start = source.index("static void StopSound() {")
        stop = source[start:source.index("static void SoundVolumeUp() {", start)]
        self.assertNotIn("g_soundNextAllowed =", stop)
        self.assertIn("++g_soundGeneration", stop)


if __name__ == "__main__":
    unittest.main()
