# Soundboard verification — revision 11

## What is exercised

The host suite compiles the real `SoundBoard.h` and extracts the actual recorder-routing,
restoration, transmission-check and post-upload state-machine functions from `menu.cpp`.
`voice_lifecycle_test.cpp` supplies fault-injecting Photon/Unity stand-ins, not a second
implementation of those functions.

Coverage includes:

- Real mono/stereo MP3 fixtures, generated with lameenc and decoded with the bundled dr_mp3.
- PCM8, PCM16 and float32 WAV parsing and sample conversion; frame alignment and sample caps.
- Float clamping and rejection of NaN/infinity.
- 20,000 deterministic malformed/random WAV inputs per test run.
- Runtime invocation argument marshalling, missing invokers and argument/instance guards.
- Pointer-sized GC-handle replacement and a round-trip above UINT32_MAX.
- Modern IsRecording, legacy RecordingEnabled and direct start/stop control paths.
- Muted/disabled voice chat, missing recorder, unavailable state and field-read fallbacks.
- Failures during state snapshotting, GC retention and input reconfiguration.
- Microphone restoration, bounded retries, retained snapshots after failure, destroyed recorders,
  and replacement playback blocked until restoration succeeds.
- The real staged source/local/voice handoff after upload completion, including local-only tone,
  source creation failure and local Play failure without changing the mic.
- A single transmission observation per clip and preservation of a mid-clip mute.
- Full-menu host C++ syntax checking and branding/output checks.

## Reproduce

```sh
python3 -m venv build/test-venv
build/test-venv/bin/pip install lameenc==1.9.0
build/test-venv/bin/python mods/dark-matter/tests/test_soundboard.py

SOUNDBOARD_SANITIZERS=1 \
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
build/test-venv/bin/python mods/dark-matter/tests/test_soundboard.py
```

Both modes are run during this verification pass. Sanitizers cover host executables,
not Android/Unity. Optional MP3 fixture coverage is skipped without lameenc; a skip is
not evidence that MP3 decoding passed.

## Regression found in this pass

A simulated transient source-restoration failure reproduced loss of the saved mic state:
the old code cleared the override and snapshot even when a restore setter threw. The new
code retains the snapshot and live clip, retries at most three times automatically, and
allows an explicit Stop retry. Replacement playback cannot overwrite unrecovered state.
The regression test fails on the old behavior and passes on the fix.

The new one-shot live check distinguishes input configuration from the recorder reporting
`IsCurrentlyTransmitting`. If no activity is observed it logs whether the source or clip
was replaced. No automatic replay/restart loop or transmit override is introduced.

## What this does not establish

Mocks do not execute the headset's Unity native audio code, game hooks, Photon transport,
voice-detection behavior or another player's speaker. A transmission flag is not proof of
remote audibility. Android compilation is verified separately in GitHub Actions.

The latest supplied headset checkpoint is revision 8: it identifies the IsRecording
compatibility failure, not the behavior of this revision. Do not label room audio as
confirmed working without a new headset/listener result.

## Required headset acceptance checks

1. Remove duplicate client libraries; install only the latest `diddy-client.so`.
2. Confirm **Soundboard Verification 11** appears and normal microphone chat works first.
3. Play Test Tone (Local), then a short WAV and MP3 with voice chat unmuted.
4. Have a second consenting player in normal voice range confirm the file is audible.
5. Check the one-shot sending/no-activity notification and `dark-matter sound:` lines.
6. Confirm the mic works after natural completion, Stop, All Off and a room change.
7. Confirm muted/disabled chat is not overridden, repeated play is blocked by cooldown,
   and Stop during loading followed by a rescan does not crash or start a stale clip.
8. If transmission remains absent, send only the new sound/voice diagnostic lines. Full
   checkpoints may contain account-related URLs and should remain private.
