# Combined TypeScript port — Diddy Client

**Output:** `diddy-client.so` (Android arm64), **Combined TS Port 12**.
**Scope:** one native menu combining the supported features, not all supplied behavior. No Frida runtime, JavaScript engine, scrambled export table or raw TypeScript is loaded.

The source menus are credited to @goldentrophy / @crimsoncauldron in their headers. The native foundation retains its existing credits. The original files are not committed; SHA-256 identities and every literal button disposition are in `combined-ports.json`.

## Included in the single library

- Shared native movement, long arms, menu settings, themes, pointer and speed controls. Duplicated buttons share state rather than running multiple menus.
- New Freeze Player in Menu, Fly (No Gravity), local-only Flip Gravity adaptation and bounded Fast Roll Speed with restoration.
- Read-only sign, item, map-key, map-origin, mob-key, public tech-pattern and object-label tools; dispenser presence inspection (not a completed dispenser-item extractor). Right grip aims; a fresh trigger press reads once, with a 0.5 s cooldown. Inspection and teleport/spawn guns are mutually exclusive.
- A manual, paged replacement for yeepsmenuD: one class summary or at most four member names per press, 0.6 s cooldown. No values are dumped. The metadata export must be available through the game SDK.
- Existing bounded item spawning, dances, local effects and MP3/WAV soundboard remain in this same library. This does not newly establish remote audibility.

## Important differences and exclusions

Native SDK bindings replace Frida hooks and version-specific export names. Missing classes, fields or AOT invokers cause an unavailable notice, not raw writes or a guessed call. Sign text is bounded and stripped of rich-text delimiters. No custom spawning commands are parsed from signs.

Room destruction/flooding, moderation, identity/permission changes, economy actions, forced room controls, global/shared-map/item mutations, automatic sends and unknown unsafe tests are omitted. No disabled placeholder buttons for these are included.

Spectator mode, anti-tag behavior, game-specific hint sound sending, text-input tests and other unverified helpers are not implemented. Native item spawning remains restricted to the existing held-item queue; uploaded arbitrary prefab/mob/map spawning is not imported.

Fly modes can conflict if multiple movement toggles are enabled; hover has priority over inverted gravity, and menu freeze pins your position last. Fast Roll requires a float rollSpeed field with a finite initial value in (0,40]; it changes it once and restores the saved value on Off or scene transition. Disable other copies of this client.

## Build, tests and install

Run `python3 tools/check_mod.py` and `python3 mods/dark-matter/tests/test_soundboard.py`. Install lameenc 1.9.0 in a virtualenv to include actual MP3 fixtures. Set `SOUNDBOARD_SANITIZERS=1` to compile host executables with ASan/UBSan. Tests include the actual extracted movement/restoration/selection code and the actual voice lifecycle/handoff code, plus full-menu syntax checking.

Build via `ANDROID_NDK=/path/to/ndk-r27c ./build.sh`, or use the successful GitHub Actions run linked in delivery. The multi-mod Actions artifact also contains unrelated libraries: extract and install **only `diddy-client.so`**, not every `.so`. Remove the previous diddy-client/dark-matter duplicate, use CX File Explorer to place the file in `Android/data/com.TrassGames.Yeeps/files/mods/`, restart and verify the version.

Headset acceptance is still needed: toggle features Off and All Off, close/reopen the menu while frozen, switch rooms with roll enabled, check that tools only read on fresh presses, test missing components, and verify mic restoration and a consenting second listener. Host mocks and Android compilation are not headset testing.

## Every supplied literal button

| Source | Source label | Disposition | Native target |
|---|---|---|---|
| ym.ts | Settings | merged | Settings |
| ym.ts | Movement Mods | merged | Movement Mods |
| ym.ts | Fun Mods | merged | Effects |
| ym.ts | Rig Mods | merged | Rig Mods |
| ym.ts | Spawner Mods | merged | Items |
| ym.ts | Overpowered Mods | merged | Overpowered |
| ym.ts | Disconnect | unsupported | Not included |
| ym.ts | PreviousPage | navigation | Native Home/Back/page arrows |
| ym.ts | NextPage | navigation | Native Home/Back/page arrows |
| ym.ts | Exit Settings | navigation | Native Home/Back/page arrows |
| ym.ts | Freeze Player in Menu | ported | Freeze Player in Menu |
| ym.ts | Unfinished/Dev Mods | unsupported | Not included |
| ym.ts | Exit Movement Mods | navigation | Native Home/Back/page arrows |
| ym.ts | Iron Man | merged | Iron Man |
| ym.ts | Fly | merged | Fly |
| ym.ts | Fly (No gravity) | adapted | Fly (No Gravity) |
| ym.ts | Noclip | merged | Noclip |
| ym.ts | Long Arms | merged | Long Arms |
| ym.ts | Speed Boost | merged | Speed Boost |
| ym.ts | No Tag Freeze | unsupported | Not included |
| ym.ts | Exit Fun Mods | navigation | Native Home/Back/page arrows |
| ym.ts | Start new round | excluded | Not included |
| ym.ts | New round spam [T] | excluded | Not included |
| ym.ts | Half creative mode | excluded | Not included |
| ym.ts | Free Stuff | excluded | Not included |
| ym.ts | No Restrictions | excluded | Not included |
| ym.ts | Spoof Build Team | excluded | Not included |
| ym.ts | Destroy All Items | excluded | Not included |
| ym.ts | Fling All Items | excluded | Not included |
| ym.ts | Spaz All Items | excluded | Not included |
| ym.ts | Items follow you | excluded | Not included |
| ym.ts | Creative Mode | excluded | Not included |
| ym.ts | Creative Mode 2 | excluded | Not included |
| ym.ts | Anti grab | excluded | Not included |
| ym.ts | Unban Self | excluded | Not included |
| ym.ts | Ban Self | excluded | Not included |
| ym.ts | Diddy Mode | excluded | Not included |
| ym.ts | Diddy Button | excluded | Not included |
| ym.ts | Diddy Button 2 | excluded | Not included |
| ym.ts | Spawn Item Spam | excluded | Not included |
| ym.ts | Spawn Block Test | excluded | Not included |
| ym.ts | Blackhole Colour | excluded | Not included |
| ym.ts | Pickup All Blocks | excluded | Not included |
| ym.ts | Colour Objects | excluded | Not included |
| ym.ts | Colour Blocks | excluded | Not included |
| ym.ts | Destroy Map | excluded | Not included |
| ym.ts | Make Everything Stuffed | excluded | Not included |
| ym.ts | Rotate Map 90 | excluded | Not included |
| ym.ts | Destroy Map Manager | excluded | Not included |
| ym.ts | Knockback Gun | excluded | Not included |
| ym.ts | Add Kick Perms | excluded | Not included |
| ym.ts | Add CW Staff Perms | excluded | Not included |
| ym.ts | Enable Staff Display | excluded | Not included |
| ym.ts | Exit Rig Mods | navigation | Native Home/Back/page arrows |
| ym.ts | Spectator Mode | unsupported | Not included |
| ym.ts | Disable Spectator Mode | unsupported | Not included |
| ym.ts | Fast Roll Speed | adapted | Fast Roll Speed |
| ym.ts | Exit OP Mods | navigation | Native Home/Back/page arrows |
| ym.ts | Set masterclient | excluded | Not included |
| ym.ts | auto masterclient | excluded | Not included |
| ym.ts | Destroy All | excluded | Not included |
| ym.ts | Hider Hint Sound | unsupported | Not included |
| ym.ts | Hit Effect Spam | excluded | Not included |
| ym.ts | Send Mobile Cash Out Spam | excluded | Not included |
| ym.ts | Send Mobile Cash Out | excluded | Not included |
| ym.ts | Refresh Map | excluded | Not included |
| ym.ts | Exit Spawner Mods | navigation | Native Home/Back/page arrows |
| ym.ts | Item Spawners | merged | Items |
| ym.ts | Exit Dev Mods | navigation | Native Home/Back/page arrows |
| ym.ts | Become CW Staff | excluded | Not included |
| ym.ts | Refresh Network Messenger | adapted | Check Room Sender |
| ym.ts | Object Info Gun | adapted | Object Info Gun |
| ym.ts | Exit Item Mods | navigation | Native Home/Back/page arrows |
| YeepsMenu.ts | Settings | merged | Settings |
| YeepsMenu.ts | Movement Mods | merged | Movement Mods |
| YeepsMenu.ts | Fun Mods | merged | Effects |
| YeepsMenu.ts | Advantage Mods | excluded | Not included |
| YeepsMenu.ts | Utility Mods | merged | Utility Mods |
| YeepsMenu.ts | Disconnect | unsupported | Not included |
| YeepsMenu.ts | PreviousPage | navigation | Native Home/Back/page arrows |
| YeepsMenu.ts | NextPage | navigation | Native Home/Back/page arrows |
| YeepsMenu.ts | Exit Settings | navigation | Native Home/Back/page arrows |
| YeepsMenu.ts | Change Menu Theme | merged | Change Menu Theme |
| YeepsMenu.ts | Change Pointer Position | merged | Change Pointer Position |
| YeepsMenu.ts | Iron Man Speed | merged | Iron Man Speed |
| YeepsMenu.ts | Exit Movement Mods | navigation | Native Home/Back/page arrows |
| YeepsMenu.ts | Platforms | merged | Platforms |
| YeepsMenu.ts | Fly | merged | Fly |
| YeepsMenu.ts | Velocity Fly | merged | Velocity Fly |
| YeepsMenu.ts | Teleport Gun | merged | Teleport Gun |
| YeepsMenu.ts | Iron Man | merged | Iron Man |
| YeepsMenu.ts | Noclip | merged | Noclip |
| YeepsMenu.ts | Long Arms | merged | Long Arms |
| YeepsMenu.ts | Speed Boost | merged | Speed Boost |
| YeepsMenu.ts | No Tag Freeze | unsupported | Not included |
| YeepsMenu.ts | Exit Fun Mods | navigation | Native Home/Back/page arrows |
| YeepsMenu.ts | Spectator Mode | unsupported | Not included |
| YeepsMenu.ts | Disable Spectator Mode | unsupported | Not included |
| YeepsMenu.ts | Test | excluded | Not included |
| YeepsMenu.ts | Refresh Network Messenger | adapted | Check Room Sender |
| YeepsMenu.ts | Refresh Map | excluded | Not included |
| YeepsMenu.ts | Hider Hint Sound | unsupported | Not included |
| YeepsMenu.ts | Hit Effect Spam | excluded | Not included |
| YeepsMenu.ts | Send Mobile Cash Out | excluded | Not included |
| YeepsMenu.ts | Enter Creative Mode | excluded | Not included |
| YeepsMenu.ts | Enter Play Mode | excluded | Not included |
| YeepsMenu.ts | No Gravity | merged | No Gravity |
| YeepsMenu.ts | Flip Gravity | adapted | Flip Gravity (Local) |
| YeepsMenu.ts | Free Stuff | excluded | Not included |
| YeepsMenu.ts | Bring All Items | excluded | Not included |
| YeepsMenu.ts | Destroy All Items | excluded | Not included |
| YeepsMenu.ts | Destroy Map Object Gun | excluded | Not included |
| YeepsMenu.ts | Unban Self | excluded | Not included |
| YeepsMenu.ts | Ban Self | excluded | Not included |
| YeepsMenu.ts | Color Map Object Gun | excluded | Not included |
| YeepsMenu.ts | Color All Map Objects | excluded | Not included |
| YeepsMenu.ts | Redeem All Butt Coin Stashes | excluded | Not included |
| YeepsMenu.ts | Destroy All Map Objects | excluded | Not included |
| YeepsMenu.ts | Become CW Staff | excluded | Not included |
| YeepsMenu.ts | Fast Roll Speed | adapted | Fast Roll Speed |
| YeepsMenu.ts | Spawn Block Test | excluded | Not included |
| YeepsMenu.ts | Reset Network Messenger | unsupported | Not included |
| YeepsMenu.ts | Remove All Stuffing | excluded | Not included |
| YeepsMenu.ts | Add Max Stuffing | excluded | Not included |
| YeepsMenu.ts | Call Vote | excluded | Not included |
| YeepsMenu.ts | TestS | excluded | Not included |
| YeepsMenu.ts | Knockback Gun | excluded | Not included |
| YeepsMenu.ts | Destroy Item Gun | excluded | Not included |
| YeepsMenu.ts | Exit Advantage Mods | navigation | Native Home/Back/page arrows |
| YeepsMenu.ts | Master Client | excluded | Not included |
| YeepsMenu.ts | Destroy All | excluded | Not included |
| YeepsMenu.ts | Lag All | excluded | Not included |
| YeepsMenu.ts | Exit Utility Mods | navigation | Native Home/Back/page arrows |
| YeepsMenu.ts | Spawn Mob | excluded | Not included |
| YeepsMenu.ts | Spawn Mob Gun | excluded | Not included |
| YeepsMenu.ts | Get Sign Text Gun | ported | Get Sign Text Gun |
| YeepsMenu.ts | Save MapObj Coordinate Gun | adapted | Save MapObj Coordinate Gun |
| YeepsMenu.ts | Get Dispenser Item Gun | adapted | Dispenser Info Gun |
| YeepsMenu.ts | Get Item Gun | ported | Get Item Gun |
| YeepsMenu.ts | Get Map Object Gun | adapted | Get Map Object Gun |
| YeepsMenu.ts | Get Mob Data Gun | ported | Get Mob Data Gun |
| YeepsMenu.ts | Get Tech Web Connection Gun | adapted | Get Tech Web Connection Gun |
| YeepsMenu.ts | Test Dispenser Item Gun | unsupported | Not included |
| YeepsMenu.ts | Unlock MG Gun | excluded | Not included |
| YeepsMenu.ts | Clear Mobs | excluded | Not included |
| YeepsMenu.ts | 100 Player Lobby | excluded | Not included |
| YeepsMenu.ts | Unlock All | excluded | Not included |
| yeepsmenuD.ts | Assembly-CSharp metadata dump | adapted | Next Metadata Class / Next Metadata Members |
| IIStupidYeepsMenu.ts | Settings | merged | Settings |
| IIStupidYeepsMenu.ts | Movement Mods | merged | Movement Mods |
| IIStupidYeepsMenu.ts | Fun Mods | merged | Effects |
| IIStupidYeepsMenu.ts | Advantage Mods | excluded | Not included |
| IIStupidYeepsMenu.ts | Utility Mods | merged | Utility Mods |
| IIStupidYeepsMenu.ts | Disconnect | unsupported | Not included |
| IIStupidYeepsMenu.ts | PreviousPage | navigation | Native Home/Back/page arrows |
| IIStupidYeepsMenu.ts | NextPage | navigation | Native Home/Back/page arrows |
| IIStupidYeepsMenu.ts | Exit Settings | navigation | Native Home/Back/page arrows |
| IIStupidYeepsMenu.ts | Change Menu Theme | merged | Change Menu Theme |
| IIStupidYeepsMenu.ts | Change Pointer Position | merged | Change Pointer Position |
| IIStupidYeepsMenu.ts | Iron Man Speed | merged | Iron Man Speed |
| IIStupidYeepsMenu.ts | Exit Movement Mods | navigation | Native Home/Back/page arrows |
| IIStupidYeepsMenu.ts | Platforms | merged | Platforms |
| IIStupidYeepsMenu.ts | Fly | merged | Fly |
| IIStupidYeepsMenu.ts | Teleport Gun | merged | Teleport Gun |
| IIStupidYeepsMenu.ts | Iron Man | merged | Iron Man |
| IIStupidYeepsMenu.ts | Noclip | merged | Noclip |
| IIStupidYeepsMenu.ts | Long Arms | merged | Long Arms |
| IIStupidYeepsMenu.ts | Speed Boost | merged | Speed Boost |
| IIStupidYeepsMenu.ts | No Tag Freeze | unsupported | Not included |
| IIStupidYeepsMenu.ts | Exit Fun Mods | navigation | Native Home/Back/page arrows |
| IIStupidYeepsMenu.ts | Spectator Mode | unsupported | Not included |
| IIStupidYeepsMenu.ts | Disable Spectator Mode | unsupported | Not included |
| IIStupidYeepsMenu.ts | Test | excluded | Not included |
| IIStupidYeepsMenu.ts | Refresh Network Messenger | adapted | Check Room Sender |
| IIStupidYeepsMenu.ts | Refresh Map | excluded | Not included |
| IIStupidYeepsMenu.ts | Hider Hint Sound | unsupported | Not included |
| IIStupidYeepsMenu.ts | Hit Effect Spam | excluded | Not included |
| IIStupidYeepsMenu.ts | Send Mobile Cash Out | excluded | Not included |
| IIStupidYeepsMenu.ts | Enter Creative Mode | excluded | Not included |
| IIStupidYeepsMenu.ts | Enter Play Mode | excluded | Not included |
| IIStupidYeepsMenu.ts | No Gravity | merged | No Gravity |
| IIStupidYeepsMenu.ts | Flip Gravity | adapted | Flip Gravity (Local) |
| IIStupidYeepsMenu.ts | Free Stuff | excluded | Not included |
| IIStupidYeepsMenu.ts | Bring All Items | excluded | Not included |
| IIStupidYeepsMenu.ts | Destroy All Items | excluded | Not included |
| IIStupidYeepsMenu.ts | Destroy Map Object Gun | excluded | Not included |
| IIStupidYeepsMenu.ts | Set Ban | excluded | Not included |
| IIStupidYeepsMenu.ts | Color Map Object Gun | excluded | Not included |
| IIStupidYeepsMenu.ts | Color All Map Objects | excluded | Not included |
| IIStupidYeepsMenu.ts | Redeem All Butt Coin Stashes | excluded | Not included |
| IIStupidYeepsMenu.ts | Destroy All Map Objects | excluded | Not included |
| IIStupidYeepsMenu.ts | Fast Roll Speed | adapted | Fast Roll Speed |
| IIStupidYeepsMenu.ts | Spawn Block Test | excluded | Not included |
| IIStupidYeepsMenu.ts | Text Input Test | unsupported | Not included |
| IIStupidYeepsMenu.ts | Anonymous | excluded | Not included |
| IIStupidYeepsMenu.ts | Reset Network Messenger | unsupported | Not included |
| IIStupidYeepsMenu.ts | Remove All Stuffing | excluded | Not included |
| IIStupidYeepsMenu.ts | Add Max Stuffing | excluded | Not included |
| IIStupidYeepsMenu.ts | Call Vote | excluded | Not included |
| IIStupidYeepsMenu.ts | TestS | excluded | Not included |
| IIStupidYeepsMenu.ts | Knockback Gun | excluded | Not included |
| IIStupidYeepsMenu.ts | Destroy Item Gun | excluded | Not included |
| IIStupidYeepsMenu.ts | Exit Advantage Mods | navigation | Native Home/Back/page arrows |
| IIStupidYeepsMenu.ts | Master Client | excluded | Not included |
| IIStupidYeepsMenu.ts | Destroy All | excluded | Not included |
| IIStupidYeepsMenu.ts | Lag All | excluded | Not included |
| IIStupidYeepsMenu.ts | Exit Utility Mods | navigation | Native Home/Back/page arrows |
| IIStupidYeepsMenu.ts | Spawn Mob | excluded | Not included |
| IIStupidYeepsMenu.ts | Spawn Mob Gun | excluded | Not included |
| IIStupidYeepsMenu.ts | Get Sign Text Gun | ported | Get Sign Text Gun |
| IIStupidYeepsMenu.ts | Save MapObj Coordinate Gun | adapted | Save MapObj Coordinate Gun |
| IIStupidYeepsMenu.ts | Get Dispenser Item Gun | adapted | Dispenser Info Gun |
| IIStupidYeepsMenu.ts | Get Item Gun | ported | Get Item Gun |
| IIStupidYeepsMenu.ts | Get Map Object Gun | adapted | Get Map Object Gun |
| IIStupidYeepsMenu.ts | Get Mob Data Gun | ported | Get Mob Data Gun |
| IIStupidYeepsMenu.ts | Get Tech Web Connection Gun | adapted | Get Tech Web Connection Gun |
| IIStupidYeepsMenu.ts | Test Dispenser Item Gun | unsupported | Not included |
| IIStupidYeepsMenu.ts | Unlock MG Gun | excluded | Not included |
| IIStupidYeepsMenu.ts | Clear Mobs | excluded | Not included |
| IIStupidYeepsMenu.ts | 100 Player Lobby | excluded | Not included |
| IIStupidYeepsMenu.ts | Unlock All | excluded | Not included |
