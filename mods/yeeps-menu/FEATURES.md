# Combined menu status

This is a native port, not a TypeScript runner. It merges duplicate working features
from YeepsIIMenu.ts, YeepsMenu.ts, menu2.ts, ym.ts and IIStupidYeepsMenu.ts.
yeepsmenuD.ts is an assembly-inspection script, not a feature menu.

## Included

- Persistent Home button (menu main page), paging, enabled-feature list.
- Platforms, Fly, Velocity Fly, Fly (No gravity), Teleport Gun, Iron Man,
  own-player Noclip, Long Arms with restoration, Speed Boost with restoration,
  No Gravity, own-player inverted gravity, Fast Roll with restoration,
  No Tag Freeze (local movement flag only), Spectator Mode (own-player setter).
- Freeze Player in Menu, pointer settings, Iron Man speed, right-hand menu,
  themes, rainbow, FPS, clock, notifications, Turn Everything Off.
- Sign text, map-object key/color, item key, mob key, object/parent name readers.
- In-memory map coordinate capture and recall display (no file writing).
- Tech-pattern reader. Unlike the upload, it **does not call the refund method**.
- Local visual effect and one-press shared confetti (one-second cooldown).
  Confetti is sent only if the exact supported game sender exists; otherwise the
  button reports unavailable. It is not the upload's repeated stuffing effect.
- Own-hand balloon spawning (0.3-second press cooldown), 3x3 and 3x3x3 balloon
  batches (three-second cooldown). Hold a balloon in the left hand first.
  A conservative balloon-only allowlist excludes unknown/explosive item payloads.
- Official starter rig poses: Wave, Helicopter, T-Pose, Windmill, Swim,
  Jumping Jacks, Spaz and Beyblade. Pick one and hold left trigger with menu closed.
  Controller trackers are handed back on release, room change, or update failure.

## Network and performance limits

- Only player presses enqueue items; holding the trigger never repeats a spawn.
- At most 27 items per batch, 32 waiting, two mod sends total per frame
  (the shared effect consumes the same budget as queued items).
- Item queue cannot change its selected key while draining; room changes and
  Turn Everything Off cancel pending items.
- Item lookup is press-only, cooldown-gated and bounded to 512 candidates.
- Class/method lookups run once; primitive effect object is reused.
- Unsupported methods fail closed rather than invoking an arbitrary overload.

## Not claimed as implemented

- Game sounds / Hider Hint / TestS: clip durations and the supported sender still
  need verification before a playback-length cooldown can be implemented.
- Get Dispenser Item: the source's field access is commented out; its button has
  no working item reader. No fabricated item property is used in this port.
- Full Object Info component dumps and Text Input Test: not ported. Object Info
  currently shows names only, not every component and member.
- Arbitrary held item keys, item-at-target spawning, map/block and mob spawners:
  not included in the conservative own-hand balloon replacement.
- The inspection script, runtime hooks, loader experiments and unfinished test
  stubs are not executable menu features in this port.

Restricted account/identity, economy/unlock, creative/role/round control,
room-wrecking, destructive map/item/player operations and uncapped spam are not
ported. Labels such as Anti grab and the Diddy buttons were reviewed by their
bodies, not their names: they manipulate other items, creative or unlock state.

## Validation

Run `python3 -m unittest discover -s mods/yeeps-menu/tests -v` for source-level
regression guards. These do not test game behavior. The Android NDK build is the
compile test; on-headset testing is still required for version-specific methods.
