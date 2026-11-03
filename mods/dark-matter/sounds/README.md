# DARK MATTER Soundboard

Put your own clips on the headset in:

`Android/data/com.TrassGames.Yeeps/files/mods/sounds/`

Files directly in `mods/` or in `Android/data/com.TrassGames.Yeeps/files/sounds/` are also scanned. Subfolders are not scanned.

1. Open **Soundboard → Scan Sounds Folder**.
2. Open **MP3 Files** or **WAV Files**.
3. Press a filename to load and play it. Use the page arrows for more files.
4. **Stop Sound** cancels loading/playback and restores the microphone. It does not bypass the playback cooldown.

## Formats and limits

- MP3: mono/stereo, 8–192 kHz.
- WAV: PCM format 1, 16-bit, mono/stereo, 8–48 kHz; data must contain complete frames.
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

All eight tests passed in the development sandbox, including runtime-call argument marshalling, GC-handle replacement, exact audio overload selection, recorder ordering, and startup retry guards. They do not run Unity or Photon Voice. Android compilation requires the NDK; room audio and microphone restoration must be tested on a headset with another consenting listener.

## Playback crash diagnostics

The **Audio Runtime Fix 4** build retains exact overload selection and uses `il2cpp_runtime_invoke` for clip creation, PCM uploads, AudioSource playback, and voice-recorder configuration. Managed exceptions are checked rather than escaping from those calls. It uses the documented 32-bit GC-handle ABI and roots replacement objects and upload arrays before runtime calls. A managed voice-routing failure preserves local playback where possible. These address concrete playback risks; the reported headset crash cannot be confirmed resolved without an on-device retest.

## Menu missing after restart

Replace the existing `dark-matter.so` with this newly built revision; do not leave multiple Dark Matter `.so` copies installed. The loader can skip a previously crashed binary until its file changes. This patch does not disable that crash protection.

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
