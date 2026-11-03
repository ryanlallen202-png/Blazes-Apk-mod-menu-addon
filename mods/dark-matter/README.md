# Diddy Client — Combined TS Port 12

One native Android arm64 library: **`diddy-client.so`**.

This combines the **supported features**, not every behavior, from `ym.ts`,
`YeepsMenu.ts`, `IIStupidYeepsMenu.ts` and the `yeepsmenuD.ts` metadata utility.
It preserves the existing Diddy Client soundboard, item queue, dances and effects.
It does not load TypeScript, Frida or another script runtime.

- [Port scope, differences, exclusions and every literal source button](COMBINED_PORTS.md)
- [Machine-readable source identities and feature dispositions](combined-ports.json)
- [Custom MP3/WAV setup and soundboard history](sounds/README.md)
- [Voice test coverage and headset acceptance checklist](tests/VERIFICATION.md)

## Installation

Get the successful GitHub Actions artifact linked in the delivery message. Extract
**only `diddy-client.so`**; other `.so` files in the multi-mod artifact are unrelated.
Remove previous copies of this client, including the old `dark-matter.so`, and use
CX File Explorer to put the new file in:

`Android/data/com.TrassGames.Yeeps/files/mods/`

Restart Yeeps and confirm **Combined TS Port 12**. Hold Y for the menu, or B when
Right Hand Menu is enabled. Touch a button with the opposite index finger.

Missing optional game methods produce an unavailable notice. Compilation and host
simulations do not prove a feature works on the headset. In particular, custom
soundboard audibility still requires a second-listener test in normal voice range.
