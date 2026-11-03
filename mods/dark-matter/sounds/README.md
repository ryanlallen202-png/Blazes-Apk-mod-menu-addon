# DARK MATTER Soundboard

Create this folder on the headset:

`Android/data/com.TrassGames.Yeeps/files/mods/sounds/`

Put your own `.wav` or `.mp3` clips in it, then open **DARK MATTER → Soundboard**. The menu scans the folder when you open the page. Each sound file appears as its own button/tab; tap that file's button to play it. Use **Scan Sounds Folder** after adding or removing files while the menu is already open.

Supported:
- WAV: PCM 16-bit, mono or stereo, 8–48 kHz
- MP3: Android MediaCodec-supported MP3 files
- Maximum clip length: 30 seconds

Playback fills the Unity audio clip with a short half-second prebuffer, then feeds the remaining audio in small chunks while it plays. This avoids creating and copying one huge managed audio array all at once. The clip is sent through the local player's Photon Voice recorder so room members can hear it through that player's voice stream when a compatible active recorder is available.

Only one sound plays at a time. A sound button stays locked until the current clip finishes, and the next play is rate-limited to at least the clip length (minimum one second). Looping and stacking are disabled. Playback volume is bounded, and the previous recorder source/settings are restored when the clip ends, is stopped, or the room changes.

If no active voice recorder is found, the menu reports local-only playback instead of claiming the room can hear it.

## Other newly ported controls

- Movement: Mega Jump, Thumbstick Fly, Jump Power, Fling Gun
- Item actions: Launch Items Gun, 9×3 duplicate grid, one-time 27-item circle, one-time 27-item rain batch
- Settings: Menu Size (50%–150%)

Item batches use the game's item message, a three-second batch cooldown, no more than 27 items per batch, and at most two sends per frame. The circle/rain actions are single batches, not automatic spawn loops.
