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

All four tests passed in the development sandbox. They do not run Unity or Photon Voice. Android compilation requires the NDK; room audio and microphone restoration must be tested on a headset with another consenting listener.

Headset checklist:

- Try mono/stereo WAV and MP3; use a stereo clip longer than one upload chunk to catch offset errors.
- Try an empty folder, invalid WAV, oversized input, and uppercase file extensions.
- Rescan and switch MP3/WAV tabs while loading and playing. The original clip should retain its filename and finish without crashing.
- Stop while loading, stop during playback, and try to replay before the cooldown expires.
- Confirm local preview volume controls work and room playback respects mute.
- Confirm the microphone works after natural completion, Stop, Turn Everything Off, and leaving/rejoining a room.
- Smoke-test every other menu page on-device: navigation and hand switching; movement toggles individually and after disabling; rig/emote tracker restoration; item queues and cooldowns; effects cleanup; Overpowered toggles and reset; Enabled Mods and Turn Everything Off.

No headset/runtime verification is implied by the host tests.
