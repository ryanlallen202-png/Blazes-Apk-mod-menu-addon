# ClientDaBell

ClientDaBell is built as `clientdabell.so` by the repository's NDK workflow.

## Soundboard

Place audio files in:

`Android/data/com.TrassGames.Yeeps/files/mods/sounds/`

Supported formats:
- WAV: PCM 8/16/24/32-bit or 32-bit float
- MP3: mono or stereo MP3

Tracks are mixed to mono and resampled to 48 kHz while loading. A track may be up to about 4.5 minutes; MP3 files are limited to 32 MB. Only one sound plays at a time, looping is disabled, and playback is locked for the clip duration.
