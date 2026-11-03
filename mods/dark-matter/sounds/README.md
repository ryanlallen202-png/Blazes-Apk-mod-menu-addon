# Diddy Client Soundboard

Put your own clips on the headset in:

`Android/data/com.TrassGames.Yeeps/files/mods/sounds/`

Files directly in `mods/`, `Android/data/com.TrassGames.Yeeps/files/sounds/`, or `Android/data/com.TrassGames.Yeeps/files/HSounds/` are also scanned. Subfolders are not scanned.

1. Open **Soundboard → Scan Sounds Folder**.
2. Open **MP3 Files** or **WAV Files**.
3. Press a filename to load and play it. Use the page arrows for more files.
4. **Stop Sound** cancels loading/playback and restores the microphone. It does not bypass the playback cooldown.

## Formats and limits

- MP3: mono/stereo, 8–192 kHz.
- WAV: PCM format 1 (8-bit unsigned or 16-bit signed), or IEEE float format 3 (32-bit), mono/stereo, 8–48 kHz. Data must contain complete frames. Non-finite floats are rejected; finite float samples are clamped to normal amplitude.
- Decoded audio: at most 8,388,608 interleaved samples (about 95 seconds of stereo at 44.1 kHz). Short clips are recommended.
- Input files larger than 17 MiB are rejected before decoding. Only regular files are accepted (not directories, symlinks, or named pipes).
- The menu lists at most 256 clips across the scanned folders. Keep the folder small for predictable navigation.
- One clip at a time. The next play remains locked for the clip duration, at least one second.

The menu attempts to feed the clip into the game's existing Photon Voice recorder and play a local preview. Voice chat must already be recording and unmuted. If its runtime APIs are unavailable, the notification says **Local only**; local playback does not prove other players hear the clip. The Volume buttons control the local preview, not the voice-chat clip level. Keep source clips at normal voice volume.

## Verification

Host regression checks:

`python3 mods/dark-matter/tests/test_soundboard.py`

These exercise the actual WAV parser, source guards, and a full-menu host C++ syntax check. The syntax check uses minimal JNI/log declarations; it is not an Android build or link test.

To also generate and decode real mono/stereo MP3 fixtures with the bundled decoder:

```sh
python3 -m venv build/test-venv
build/test-venv/bin/pip install lameenc==1.9.0
build/test-venv/bin/python mods/dark-matter/tests/test_soundboard.py
```

All fifteen tests passed in the development sandbox, including runtime-call argument marshalling, GC-handle replacement, exact audio overload selection, recorder ordering, and startup retry guards. They do not run Unity or Photon Voice. Android compilation requires the NDK; room audio and microphone restoration must be tested on a headset with another consenting listener.

## Playback crash diagnostics

The **Unity 6 Handle Fix 5** build retains exact overload selection and uses `il2cpp_runtime_invoke` for clip creation, PCM uploads, AudioSource playback, and voice-recorder configuration. Managed exceptions are checked rather than escaping from those calls. It uses the SDK's pointer-sized `Il2CppGCHandle` ABI for Unity 6 and roots replacement objects and upload arrays before runtime calls. A managed voice-routing failure preserves local playback where possible. These address concrete playback risks; the reported headset crash cannot be confirmed resolved without an on-device retest.

## Menu missing after restart

Remove the old `dark-matter.so` and install the newly built `diddy-client.so`; do not leave duplicate client `.so` copies installed. The loader can skip a previously crashed binary until its file changes. This patch does not disable that crash protection.

The menu now retries startup lookups, waits for valid hand/head transforms and a font, and retries failed UI construction. It rebuilds after room-change callbacks or loss of its scene objects. On a fresh launch, hold **Y** to open it (or **B** if right-hand mode has been selected in that session).

If the menu is still missing, inspect `bnm_checkpoint.txt` for the mod's load/skip reason. A `DISABLE` file in `mods/` intentionally prevents all mods loading; remove it only when you intend to re-enable them. Confirm the newest blaze APK from the official Discord is installed. Do not clear game account data.

If either format still crashes, send the final lines containing `dark-matter sound:` from:

`Android/data/com.TrassGames.Yeeps/files/bnm_checkpoint.txt`

Stage markers distinguish clip creation, the first PCM upload, local playback, voice recorder discovery, and recorder startup. Also report whether the crash occurs before or after the loading percentage reaches 100%, and whether the clip is audible first. A native crash may require the headset's Android crash log in addition to these markers.

Headset checklist:

- Try mono/stereo WAV and MP3; use a stereo clip longer than one upload chunk to catch offset errors.
- Try an empty folder, invalid WAV, oversized input, and uppercase file extensions.
- Rescan and switch MP3/WAV tabs while loading and playing. The original clip should retain its filename and finish without crashing.
- Stop while loading, stop during playback, and try to replay before the cooldown expires.
- Confirm local preview volume controls work and room playback respects mute.
- Confirm the microphone works after natural completion, Stop, Turn Everything Off, and leaving/rejoining a room.
- Smoke-test every other menu page on-device: navigation and hand switching; movement toggles individually and after disabling; rig/emote tracker restoration; item queues and cooldowns; effects cleanup; Overpowered toggles and reset; Enabled Mods and Turn Everything Off.

No headset/runtime verification is implied by the host tests.

## Startup crash regression (Fix 5)

Fix 4 incorrectly declared GC handles as `uint32_t`. The SDK's Unity 2023+/Unity 6 headers define `Il2CppGCHandle` as `void *`; returning or passing it as 32 bits can truncate it on arm64. Fix 5 uses that SDK type for storage and all three GC-handle functions. The host regression round-trips a handle above `UINT32_MAX`, including replacement and release. No SDK or loader crash protection has been changed.

If the previous build crashes on entry, remove its `.so` from the headset's mods folder before launching again. Install only the new `diddy-client.so`, not all libraries from the artifact. Keep other mods removed temporarily when isolating a load crash. A successful Actions build confirms compilation, not on-device recovery; send the loader's final checkpoint lines if startup still fails.

## Audio Invoke Guard 6: remaining playback crash investigation

Fix 6 checks both the native entry point and AOT invoker before calling `il2cpp_runtime_invoke`, plus argument count and non-null instance for instance methods. The runtime's invoke implementation dereferences the invoker without a null check; metadata presence alone does not prove it is callable. An absent invoker now fails with a checkpoint message instead of entering that path. This is a concrete guard, not proof it caused the reported headset crash. Native crashes elsewhere still cannot be caught by C++ exceptions.

Clip allocation and the first PCM upload happen in separate update frames. Additional stage markers distinguish GameObject allocation, AddComponent and AudioSource.Play. Managed playback errors now include the exception category in the checkpoint log.

**Test Tone (Local)** plays a quiet 0.25-second 440 Hz tone through the same Unity clip/upload/AudioSource path, without reading files, invoking a decoder or changing voice chat. Normal filename buttons retain room-audio routing. To help isolate the remaining fault, report whether this local tone also crashes, whether file loading reaches 100%, and the final `dark-matter sound:` lines from `bnm_checkpoint.txt`. Do not repeatedly relaunch a build that crashes on entry; remove its `.so` to recover first.

The build and host tests cannot establish on-device playback correctness. No headset result has been collected for this revision.

## Playback Handoff Fix 7: crash immediately after 100%

The reported timing is after the final PCM upload. Fix 7 removes soundboard GameObject allocation and manual construction from that handoff. It adds a dedicated AudioSource component to the existing active head GameObject instead; it never takes over an existing game AudioSource and never destroys the player/head GameObject. Room cleanup destroys only the added component.

Upload completion, source setup, local playback and voice routing now occur on separate update frames. A pending start is treated as busy, Stop cancels it, and playback cooldowns begin at playback rather than upload completion. Normal filename buttons still request room playback; the local test tone still bypasses voice.

Checkpoints now report `source stage`, `local start stage`, and `voice stage`. Please report whether **Test Tone (Local)** crashes too, and provide the last `dark-matter sound:` lines if it does. The exact native crash cause has not been confirmed without headset logs. Host tests check source ownership and handoff structure; compilation alone does not establish that headset playback is fixed.

## Voice Compatibility Fix 8: checkpoint findings

The supplied previous-run checkpoint (revision 6) ends during AudioSource component creation. The current-run checkpoint identifies revision 7 and records completed local starts for MP3, WAV and the built-in tone, followed by further menu interactions. Its repeated `local=1 voice=0` entries show that voice routing is not starting. They do not establish audible output or delivery to another player, and neither checkpoint contains a native backtrace.

For file playback, `voice stage requested=1` is followed by `voice=0` without recorder-discovery markers, locating the rejection in the compatibility preflight. The old preflight reports only a generic UI notification, so the supplied log does not identify which required binding is absent.

Fix 8 logs every unavailable required binding. It accepts the public `IsRecording` property setter as an alternative to separate StartRecording/StopRecording metadata. Saved source/clip/loop state and transmit/recording checks can use known Recorder state fields when public getter invocation is unavailable. Fields are read only; configuration remains through public setters. The microphone's source, clip and loop settings are preserved and restored. Playback still requires voice chat already recording and unmuted; it does not force transmit or bypass other players' mute or distance settings.

The new host test exercises both recording-control variants. Actual voice delivery still needs an on-device test with another listener. If the new checkpoint reports `voice unavailable: ...`, send those specific lines rather than the entire log. Full checkpoints can contain account-related URLs and should not be posted publicly. No uploaded checkpoints are included in the repository.

## Legacy Voice Fix 9: confirmed missing recording-state binding

The revision 8 checkpoint explicitly reports `start=1 stop=1 recordingSetter=0`, `recording=0`, and `voice unavailable: IsRecording`. Local playback completes. This establishes that the recording-state preflight blocks room routing; it does not identify a new playback crash.

Older Photon Voice versions expose `RecordingEnabled`/`recordingEnabled` instead of `IsRecording`/`isRecording`. Fix 9 resolves both API families, preferring live IsRecording state when available. For the legacy API it reads enablement and uses the public RecordingEnabled setter to disable input before changing clip/source configuration and to re-enable afterward. This also avoids legacy configuration setters auto-restarting the recorder while input is only partially configured. The old private StartRecording/StopRecording entry points are used only if neither public recording-control setter is invokable.

Missing state still fails closed: it is never assumed enabled. A false recording-enabled value or muted transmit state still prevents room playback. Legacy enablement is not proof of live transmission; the checkpoint explicitly logs that limitation. The source, prior clip and loop settings are still restored after completion/Stop/room change.

The regression suite now exercises modern, legacy and direct recording-control variants plus live/legacy/missing recording-state reads. The Android build and host tests do not replace testing with another listener. If room audio still fails, send only the new `dark-matter voice` and `dark-matter sound:` lines. Uploaded full checkpoints remain private and are not committed.

## Diddy Client rename

The loader name and both the static and animated menu titles are now **Diddy Client**. The build output is **`diddy-client.so`**. The source folder remains `mods/dark-matter/`, and the `dark-matter voice` / `dark-matter sound:` diagnostic prefixes are intentionally retained so existing log instructions remain usable.

Remove the old `dark-matter.so` (and any duplicate copies), then install only the new `diddy-client.so`. Do not load both clients: they register separate callbacks and can compete over the same voice recorder.

This is a branding-only change. The user's latest report confirms local-only audio; room delivery is still unresolved. A new checkpoint after a playback attempt on Legacy Voice Fix 9 is needed to determine which stage now fails. The revision 8 checkpoint only establishes the earlier missing IsRecording binding.

## SoundBoard Port 10: user-supplied header integration

`src/SoundBoard.h` is an SDK-compatible adaptation of the supplied header, not an unmodified copy. The supplied file includes generated Unity/Photon wrappers, menu settings and resolver headers that are not present in this repository. The adaptation is used by the actual menu build.

Ported behavior:

- Direct WAV decoding for PCM 8/16-bit and IEEE float32, with complete-frame validation, RIFF bounds checks, decoded-sample caps and finite-float validation. Tests compile and exercise the actual new header.
- Recorder discovery on the named **PhotonVRVoice** object, then the game's VoiceChatManager primary recorder, then a unique active scene Recorder. Ambiguous scene recorders are rejected rather than choosing an arbitrary component. Discovery runs only for a playback request, not every frame.
- Read support for the supplied header's `HSounds` folder, in addition to existing locations.

Existing MP3 decoding, modern/legacy recording APIs, cooldowns, GC roots, staged local playback and saved-input restoration are retained. The header's external-wrapper dependencies, file-request/OGG loader, forced unmute, raw configuration-field writes, per-frame scene searches and local volume/distance booster are not imported. The “Hear All” booster changes local reception, not transmission to other players. This port does not boost voice levels or override mute settings.

Install only the new **diddy-client.so** and confirm **SoundBoard Port 10** in the menu. Room delivery is still unverified until tested with another listener; the source adaptation alone cannot prove it. If it remains local-only, provide new `dark-matter voice` and `dark-matter sound:` checkpoint lines, including the recorder-discovery result.

## Soundboard Verification 11

This pass adds fault-injection tests of the actual voice routing/restoration and post-100% handoff code, plus 20,000 deterministic malformed WAV inputs. The suite can run under AddressSanitizer and UndefinedBehaviorSanitizer; see [the verification report](../tests/VERIFICATION.md) for commands, coverage and headset acceptance checks.

A regression test reproduced a microphone-restoration error that discarded saved input state after a setter failed. Restoration now retains state/clip ownership, makes at most three automatic attempts and supports a manual Stop retry. New playback is blocked until unrecovered state is restored. Snapshot errors and failed GC retention do not mutate the recorder.

A one-shot live check uses the optional `IsCurrentlyTransmitting` getter. The initial message now says **Voice input configured**, not **Playing to room**. It then reports voice activity, no observed activity, an input replaced by the game, or an unavailable accessor in the checkpoint. Even a positive sending flag does not prove another player hears it. No automatic replay or forced transmit is added.

Host and Android builds cannot certify receiver audibility. A second-player headset test remains necessary.
