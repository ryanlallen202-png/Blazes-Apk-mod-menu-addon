"""Host-side regression checks; these do not replace Unity/headset tests.
Run: python3 mods/dark-matter/tests/test_soundboard.py
"""
import pathlib
import os
import json
import re
import shutil
import struct
import subprocess
import tempfile
import unittest

SOURCE = pathlib.Path(__file__).resolve().parents[1] / "src/menu.cpp"
NATIVE_FLAGS = (["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g"]
                if os.environ.get("SOUNDBOARD_SANITIZERS") == "1" else [])


class SoundboardTests(unittest.TestCase):
    def test_combined_port_inventory(self):
        menu = SOURCE.read_text()
        ports = (SOURCE.parent / "CombinedPorts.hpp").read_text()
        manifest = json.loads((SOURCE.parents[1] / "combined-ports.json").read_text())
        self.assertEqual({s["file"] for s in manifest["sources"]},
                         {"ym.ts", "YeepsMenu.ts", "yeepsmenuD.ts", "IIStupidYeepsMenu.ts"})
        self.assertEqual(manifest["output"], "diddy-client.so")
        self.assertIn(manifest["version"], menu)
        self.assertEqual(menu.count("YM_EXPORT int YeepsMod_Init("), 1)
        self.assertEqual(menu.count("ym::onUpdate(OnUpdate)"), 1)
        labels = set(re.findall(r'\{[A-Z_]+, "([^"]+)"', menu))
        for source in manifest["sources"]:
            self.assertRegex(source["sha256"], r"^[a-f0-9]{64}$")
            self.assertGreater(source["bytes"], 0)
            for feature in source["features"]:
                self.assertIn(feature["status"], {"merged", "ported", "adapted", "navigation", "unsupported", "excluded"})
                if feature["status"] in {"merged", "ported", "adapted"}:
                    for label in feature["target"].split(" / "):
                        self.assertIn(label, labels)
        for forbidden in ("PhotonNetwork", "TryRefund", "SetIsSpectatorMode", "Il2Cpp.$config"):
            self.assertNotIn(forbidden, ports)
        self.assertIn("CombinedMovementReset();", menu[menu.index("static void OnRoomChange() {"):])
        self.assertIn("g_utilitySelected < 0 && in.rtrig", menu)
        self.assertIn("printed < 4", ports)
        self.assertIn("g_time + 0.6", ports)

    def test_combined_native_movement(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")
        code = (SOURCE.parent / "CombinedPorts.hpp").read_text()
        section = code[code.index("static Ref g_frozenTransform"):code.index("// ---- read-only TS utility tools")]
        with tempfile.TemporaryDirectory() as directory:
            tmp = pathlib.Path(directory)
            (tmp / "combined_movement.inc").write_text(section)
            selection = code[code.index("static void PrimaryGunPicked()"):code.index("static void UtilityTick(")]
            (tmp / "combined_selection.inc").write_text(selection)
            exe = tmp / "combined_movement"
            subprocess.run([compiler, "-std=c++20", *NATIVE_FLAGS, "-Wall", "-Wextra", "-Werror",
                            "-I" + str(SOURCE.parent), "-I" + directory,
                            str(pathlib.Path(__file__).with_name("combined_movement_test.cpp")), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_voice_lifecycle(self):
        """Execute actual routing/restoration code against a fault-injecting SDK mock."""
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")
        source = SOURCE.read_text()
        start = source.index("template<class T>\nstatic T ReadRecorderValue")
        implementation = source[start:source.index("// ---- soundboard playback", start)]
        stage_start = source.index("static void SoundStartTick() {")
        implementation += source[stage_start:source.index("static void SoundLoadTick() {", stage_start)]
        harness = pathlib.Path(__file__).with_name("voice_lifecycle_test.cpp")
        with tempfile.TemporaryDirectory() as directory:
            tmp = pathlib.Path(directory)
            (tmp / "voice_under_test.inc").write_text(implementation)
            exe = tmp / "voice_lifecycle_test"
            subprocess.run([compiler, "-std=c++20", *NATIVE_FLAGS, "-Wall", "-Wextra", "-Werror",
                            "-I" + directory, str(harness), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_client_branding(self):
        source = SOURCE.read_text()
        self.assertIn('YEEPS_MOD("Diddy Client")', source)
        self.assertIn('const char *word = "DIDDY CLIENT";', source)
        self.assertIn('Label(ct, "DIDDY CLIENT"', source)
        self.assertNotIn('DARK MATTER', source)
        cmake = SOURCE.parents[1] / "CMakeLists.txt"
        self.assertIn('add_yeeps_mod(diddy-client ', cmake.read_text())
        self.assertIn('dark-matter sound:', source)

    def test_wav_parser(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")
        parser = '#include "SoundBoard.h"\nusing namespace diddy::soundboard;\n'

        def wav(channels=1, rate=44100, data=b"\0\0" * 8, bits=16, audio_format=1):
            width = bits // 8
            fmt = struct.pack("<HHIIHH", audio_format, channels, rate, rate * channels * width, channels * width, bits)
            body = b"WAVEfmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
            if len(data) & 1:
                body += b"\0"
            return b"RIFF" + struct.pack("<I", len(body)) + body

        cases = [(wav(), True), (wav(channels=2), True), (wav(rate=8000), True),
                 (wav(rate=48000), True), (wav(channels=3), False),
                 (wav(rate=7999), False), (wav(rate=48001), False),
                 (wav(bits=8), True), (wav(bits=24), False),
                 (wav(bits=32, audio_format=3, data=struct.pack("<ff", 0.25, -0.5)), True),
                 (wav(bits=32), False), (wav(bits=8, data=b"abc"), True), (wav(data=b""), False),
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
            subprocess.run([compiler, "-std=c++20", *NATIVE_FLAGS, "-Wall", "-Wextra", "-Werror", "-I" + str(SOURCE.parent), str(cpp), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_adapted_header_sample_decoding(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")

        def wav(data, bits, audio_format):
            fmt = struct.pack("<HHIIHH", audio_format, 1, 48000, 48000 * (bits // 8), bits // 8, bits)
            body = b"WAVEfmt " + struct.pack("<I", 16) + fmt + b"data" + struct.pack("<I", len(data)) + data
            if len(data) & 1:
                body += b"\0"
            return b"RIFF" + struct.pack("<I", len(body)) + body

        cases = [(wav(bytes([0, 128, 255]), 8, 1), [-1.0, 0.0, 127 / 128]),
                 (wav(struct.pack("<hhh", -32768, 0, 32767), 16, 1), [-1.0, 0.0, 32767 / 32768]),
                 (wav(struct.pack("<fff", -2.0, 0.25, 2.0), 32, 3), [-1.0, 0.25, 1.0])]
        checks = []
        for payload, expected in cases:
            data = ",".join(map(str, payload))
            values = ",".join(str(v) + "f" for v in expected)
            checks.append("{ std::vector<uint8_t> bytes = {" + data + "}; WavInfo info; std::vector<float> samples; "
                          "assert(DecodeWav(bytes, info, samples, 3)); "
                          "std::vector<float> expected = {" + values + "}; "
                          "assert(samples.size() == expected.size()); "
                          "for (size_t i=0; i<samples.size(); ++i) assert(std::fabs(samples[i]-expected[i]) < 0.00001f); "
                          "assert(!DecodeWav(bytes, info, samples, 2)); assert(samples.empty()); }")
        for value in (float("nan"), float("inf"), float("-inf")):
            data = ",".join(map(str, wav(struct.pack("<f", value), 32, 3)))
            checks.append("{ std::vector<uint8_t> bytes = {" + data + "}; WavInfo info; std::vector<float> samples; "
                          "assert(!DecodeWav(bytes, info, samples, 3)); assert(samples.empty()); }")
        program = '#include "SoundBoard.h"\n#include <cassert>\nusing namespace diddy::soundboard;\nint main() {\n' + "\n".join(checks) + "\n}\n"
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "header_decode_test.cpp"
            exe = pathlib.Path(directory) / "header_decode_test"
            cpp.write_text(program)
            subprocess.run([compiler, "-std=c++20", *NATIVE_FLAGS, "-Wall", "-Wextra", "-Werror", "-I" + str(SOURCE.parent), str(cpp), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
        source = SOURCE.read_text()
        self.assertIn('diddy::soundboard::kVoiceObjectName', source)
        self.assertIn('persistent + "/HSounds"', source)
        self.assertIn('DecodeWav(bytes, w, pcm, kMaxSoundSamples)', source)
        self.assertNotIn('lobbyHearBoost', source)

    def test_wav_adversarial_inputs(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("A host C++ compiler is required")
        fmt = struct.pack("<HHIIHH", 1, 1, 48000, 96000, 2, 16)
        body = b"WAVEfmt " + struct.pack("<I", 16) + fmt + b"data" + struct.pack("<I", 8) + bytes(8)
        baseline = b"RIFF" + struct.pack("<I", len(body)) + body
        data = ",".join(map(str, baseline))
        program = '''#include "SoundBoard.h"
#include <cassert>
using namespace diddy::soundboard;
int main() {
    std::vector<uint8_t> baseline = {'''+data+'''};
    uint32_t state = 0x467b9183;
    auto random = [&]() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; };
    for (int trial = 0; trial < 20000; ++trial) {
        std::vector<uint8_t> bytes;
        if (trial % 2) {
            bytes = baseline;
            if (trial % 3 == 0) bytes.resize(random() % (baseline.size() + 1));
            for (unsigned change = 0, count = random() % 8; change < count && !bytes.empty(); ++change)
                bytes[random() % bytes.size()] = uint8_t(random());
        } else {
            bytes.resize(random() % 512);
            for (auto &byte : bytes) byte = uint8_t(random());
        }
        WavInfo info;
        std::vector<float> samples;
        if (DecodeWav(bytes, info, samples, 128)) {
            assert(!samples.empty() && samples.size() <= 128);
            assert(info.channels == 1 || info.channels == 2);
            assert(samples.size() % info.channels == 0);
            for (float sample : samples) assert(std::isfinite(sample) && sample >= -1 && sample <= 1);
        }
    }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "wav_adversarial.cpp"
            exe = pathlib.Path(directory) / "wav_adversarial"
            cpp.write_text(program)
            subprocess.run([compiler, "-std=c++20", *NATIVE_FLAGS, "-Wall", "-Wextra", "-Werror",
                            "-I" + str(SOURCE.parent), str(cpp), "-o", str(exe)], check=True)
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
            subprocess.run([compiler, "-std=c++20", *NATIVE_FLAGS, "-Wall", "-Wextra", "-Werror", "-I" + str(SOURCE.parent), str(cpp), "-o", str(exe)], check=True)
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
            subprocess.run([compiler, "-std=c++20", *NATIVE_FLAGS, "-Wall", "-Wextra", "-Werror", "-I" + str(SOURCE.parent), str(cpp), "-o", str(exe)], check=True)
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
            subprocess.run([compiler, "-std=c++20", *NATIVE_FLAGS, "-DNDEBUG", "-D__has_feature(x)=0", "-fsyntax-only",
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
            subprocess.run([compiler, "-std=c++20", *NATIVE_FLAGS, "-I" + str(SOURCE.parent), str(cpp),
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
            subprocess.run([compiler, "-std=c++20", *NATIVE_FLAGS, "-Wall", "-Wextra", "-Werror", "-I" + str(SOURCE.parent), str(cpp), "-o", str(exe)], check=True)
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
            subprocess.run([compiler, "-std=c++20", *NATIVE_FLAGS, "-Wall", "-Wextra", "-Werror", "-I" + str(SOURCE.parent), str(cpp), "-o", str(exe)], check=True)
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
        self.assertIn("if (!g_voiceOverrideActive) Kill(g_soundClip)", stop)
        self.assertIn("g_voiceRestoreAt > 0", source)
        play = source[source.index("static void PlaySound() {"):source.index("static void PlayTestTone() {")]
        self.assertLess(play.index("StopSound();"), play.index('if (g_voiceOverrideActive)'))
        self.assertLess(play.index('if (g_voiceOverrideActive)'), play.index("g_soundDecodeState.store(1)"))


if __name__ == "__main__":
    unittest.main()
