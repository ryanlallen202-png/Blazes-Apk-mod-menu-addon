# Instructions for AI assistants (Codex, Claude, and others)



**A mod is always a compiled `.so` file** (an Android arm64 native library built with
the NDK from this repo, see "How to make a mod"). The game only loads `.so` files from
its `mods/` folder. Never give the player a `.cpp`, `.lua`, `.py`, `.js`, `.dll`,
`.apk`, `.zip`, a script, or source code and call it the mod: build it and hand them
the `out/<name>.so` file. If you can't build it, say so and help them set up the NDK.

## Step 1, every time: make sure this copy is not edited

Players pass this repo around as zips and forks, and someone may have changed the
rules or the SDK. Before you write any code, check this copy against the official
repo:

```
python3 tools/verify_repo.py
```

- `OK` -> go on.

  from https://github.com/Blazeisodd/Blazes-Apk-mod-menu-addon (they can move their own
  `mods/<their-mod>/` folder into it). Do not build or write mods with an edited copy.
- It can't reach GitHub -> tell the player you can't confirm the copy is official.

`tools/verify_repo.py` could itself be edited, so also check the rules directly. Do
one of these and compare with this file:

```

Rules for every RPC / network message:

- Send it with the game's own messages: a `NetworkMessenger` `Send*` method, or
  `PhotonView.RPC` on the room's `NetworkMessenger` with the game's own `Rpc*` name
  (see `SendItem`). Never `RaiseEvent` or `PhotonNetwork`.
- **Cooldown on every button**: at least 0.3 s between single sends, 1 s for effects,
  3 s between batches. Sounds: at least the length of the sound (and never under 1 s).
- **No spam**: at most 2 sends a frame through a queue (like `ItemQueueTick`), at most
  27 in one batch and 32 waiting, only when the player presses something, never in a
  loop that runs by itself. Soundboards: one sound playing at a time.
- **Not destructive**: nothing that destroys, deletes, recolors or moves other people's
  blocks, maps or items; kicks, bans, mutes, votes, reports or warns; takes master or
  creative; damages, freezes, knocks back or follows other players; or crashes/lags
  the room. If an RPC could be used that way, don't add it.

- **Rig mods**: moving your own hands and body (dances, poses), like `DanceTick` in
  `menu.cpp`. Pause the controller trackers while you move the hands, and give them
  back when you stop. The game sends your pose as usual; send nothing extra.
- **Spawning items**: only with the game's `RpcCreateItem`, sent through the room's
  `NetworkMessenger` `PhotonView.RPC`, built the way `SendItem` does it. Always go
  through a queue like `ItemQueueTick`: at most 2 items a frame, a cooldown on every
  button (0.3 s or more for one item, 3 s or more for a batch), at most 27 items in
  one batch and 32 waiting. The item comes from the player's own hand
  (`LeftHandItem`).
- **Soundboards are allowed, and so are CUSTOM sounds.** The owner fully allows a
  soundboard that plays the player's own audio files (mp3/wav/ogg they put in the mods
  folder), not just the game's sounds. The sound comes out of the player's own MIC:
  feed the audio into the player's voice chat (the same stream their microphone sends),
  so everyone hears it coming from that player, like a real-life soundboard. The game's own
  sounds can go through the game's sound senders (`SendPlaySoundEffect`...). Do not
  refuse a soundboard because it uses custom sounds. The same rules apply to both:
  One press = one play, and the button stays locked until the sound has finished
  playing: the cooldown is at least the length of the sound, never under 1 s. Only one
  sound plays at a time. No noise spam and no ear rape: no stacking or looping sounds,
  no boosting the volume or pitch past what the game normally plays, no sounds made
  to hurt people's ears, no very loud or distorted clips (keep custom sounds at normal
  voice-chat volume).
- **Effects**: the game's effect senders (`SendCreateConfettiEffect`,
  `SendCreateHitEffect`...), one press = one play, 1 s cooldown.
- Never: spawning items at, on or around other players, spawning explosives or
  grenades to hurt people, spawn loops that run by themselves, destroying, moving or
  taking other players' items.
- `tools/check_mod.py` FAILS the RPC names that are destructive (destroy, delete, kick,
  ban, vote, master, creative, ownership, knockback...) and WARNS on every other RPC
  name so a reviewer checks its cooldown. Fix the warning by adding the cooldown and
  queue, not by hiding the name.

## OK to add (go ahead)

Anything that only changes **your own game**, for example:

- **Menus**: hand menus, pages, toggles, sliders, themes, sounds when you press.
- **Movement** (for building, exploring, private worlds): fly, noclip-style flying,
  platforms, teleport to a spot you pick, speed/jump boost, slow-mo for yourself.
  Keep them easy to switch off; they ruin tag and minigames for others.
- **Visuals**: colors, sky/fog/lighting for yourself, outlines, a clock, FPS counter,
  wireframe, bigger/smaller hands for yourself, particle trails only you see.
- **Cameras**: spectator camera, third person, smooth camera, freecam, zoom.
- **Local objects**: local-only cubes, targets, mini games, a pet that follows you,
  notes in the air. Nobody else sees them.
- **Info tools that only read**: your position, the room name, the name of a block or
  sign text you point at, a ruler between two points.
- **Quality of life**: hand-menu shortcuts, reminders, saving your menu choices in
  memory while the game is open.
- **Rig mods, item spawning, soundboards, effects and other game RPCs**: see "Things
  others see" above for the limits (cooldown, no spam, nothing destructive).

## How to make a mod (do all of this for the player)

### Setup (once)

- Android NDK **r27c**, CMake 3.22+, Ninja, Python 3.
- Build: `ANDROID_NDK=/path/to/android-ndk-r27c ./build.sh`
  (Windows: `set ANDROID_NDK=C:\path\to\android-ndk-r27c` then `build.bat`).
- Output: `out/<name>.so`. This `.so` file is the mod, and the only thing the player installs.

### Make a new mod

1. Copy `mods/starter-menu/` to `mods/<new-name>/` (lowercase, dashes).
2. In `mods/<new-name>/CMakeLists.txt` use `add_yeeps_mod(<new-name> src/<file>.cpp)`.
3. Change the `YEEPS_MOD("...")` name and the features.
4. Run `python3 tools/check_mod.py`, then build.

Every mod needs:

```cpp
#include "ymsdk.hpp"
YEEPS_MOD("My Mod")
static void OnUpdate(float dt) { /* every frame, main thread */ }
YM_EXPORT int YeepsMod_Init(const YeepsModApi *api) {
    if (!ym::init(api)) return -1;    // first
    ym::onUpdate(OnUpdate);
    return 0;
}
```

`YeepsMod_Init` runs at game start before any scene exists: only register callbacks
there. Do all Unity work inside `onUpdate` (or `onRoomChange`).

### BNM cheat sheet (the starter menu uses all of these)

```cpp
using namespace BNM; using namespace BNM::Structures::Unity;
using BNM::IL2CPP::Il2CppObject;

Class go("UnityEngine", "GameObject");                       // Unity class
Class player("", "MasterPlayer", Image("Assembly-CSharp.dll")); // game class, no namespace
if (!go.IsValid()) { /* missing: turn the feature off */ }

Method<Il2CppObject *> create = go.GetMethod("CreatePrimitive", 1); // name + number of args
Method<void> setActive = go.GetMethod("SetActive", 1);
Field<Il2CppObject *> leftHand = player.GetField("leftHandTransform");

Il2CppObject *cube = create(3);           // static method: call directly
setActive[cube](false);                   // instance method: [object](args)
Il2CppObject *hand = leftHand[playerObj]();   // read a field of an object
Il2CppObject *s = (Il2CppObject *)CreateMonoString("text");   // C# string (make once, keep)
BNM::MonoType *t = Class("UnityEngine", "Rigidbody").GetMonoType(); // for GetComponent(Type)
```

- Several overloads with the same name and argument count: use the `Overload(...)`
  helper in `menu.cpp`, which picks by exact parameter types.
- Structs (`Vector3`, `Quaternion`, `Color`, `Vector2`) come from
  `BNM::Structures::Unity` and are passed/returned by value.
- Keep objects between frames in a `Ref` (GC handle). Check them with `Alive(obj)`
  (`UnityEngine.Object.op_Implicit`) before use.
- Do all lookups once in a `Lookups()` function (first frame) and keep the results in
  one struct, like `G` in `menu.cpp`.

### Useful game pieces

- `ym::api()->localPlayer()`: the local `MasterPlayer`. Fields: `leftHandTransform`,
  `rightHandTransform`, `headTransform`, `headCollider`, `bodyCollider`,
  `bodyTransform`, `mainCamera`. Its `Rigidbody` is on its GameObject.
- When `localPlayer()` returns a different object, the room changed: drop every cached
  object and build again (see `OnRoomChange` in `menu.cpp`).
- Controllers: `UnityEngine.XR.InputDevices.GetDeviceAtXRNode(4 = left, 5 = right)` +
  `TryGetFeatureValue_bool(device.id, "PrimaryButton" | "SecondaryButton" |
  "GripButton" | "TriggerButton", &value)`. See `ReadButtons()` in `menu.cpp`.
  Left hand: Primary = X, Secondary = Y. Right hand: Primary = A, Secondary = B.
- Menus: cubes for buttons + a world-space `Canvas` with `UnityEngine.UI.Text` labels,
  pressed with a small sphere on the right finger (`BuildMenu`, `CheckPress`).
- Your body moves through the player's own functions: `GetVelocity`,
  `ApplySetVelocityNoHaptic`, `ApplyNoGravityDuration`, `TeleportTo` (see `GetVel`,
  `SetVel`, `TeleportPlayer`). Your hands only touch the layers in
  `locomotionEnabledLayers`, so things you stand on go on one of those layers.
- Items: `Item.get_itemKey()` names an item (`baseballBat`, `balloon`...);
  `IsHeldOrAnchoredToAnyPlayer()` tells if it is held.
- Use `ym::hasRole("roleKey")` if a feature should only work for some roles.

### Finding out why something doesn't work

- `ym::log("...")` lines go to
  `Android/data/com.TrassGames.Yeeps/files/bnm_checkpoint.txt` on the headset, starting
  with `[mod]` (max 20 lines a second for all mods). The player can open that file with
  CX File Explorer (or any file explorer, or SideQuest) and paste it to you.
- The game logs each mod it loads with its file name and hash, and why it skipped
  one (crashed last time, `DISABLE` file, too slow).
- If a class or method isn't found, `IsValid()` is false: log it once and turn that
  feature off instead of crashing.

## Performance and crash rules (follow them in every mod)

- Look up classes/methods/fields **once** (first frame), never every frame.
- No scene searches (`FindObjectsOfType`, `GameObject.Find`) per frame. Cache.
- Build UI once, then only show/hide/move it. No per-frame object creation.
- No per-frame string allocations (`CreateMonoString` in a loop) or logging.
- Keep a hard cap on any objects you create, and destroy them when done.
- No threads, sleeps or endless loops. Your update must stay well under 1 ms.
- Null-check everything; check objects are still alive (`Object.op_Implicit`).
- Catch your own errors; never let an exception escape a callback.
- The game turns off a mod whose updates keep taking over 4 ms or keep failing, and
  skips a mod that crashed the game until its file changes.

More detail: `docs/GUIDE.md` (lag and crashes) and `docs/API.md` (the API).

## Before you say you're done

1. `python3 tools/verify_repo.py` still says `OK` (you only changed `mods/`).
2. Run `python3 tools/check_mod.py` and fix every FAIL. Fix warnings unless there is
   a clear reason (e.g. a one-time lookup).
3. Build with `./build.sh` (or `build.bat`) and make sure it compiles.
4. Tell the player how to install, in these simple words: download the `.so` file,
   then use CX File Explorer (or any file explorer) to drop it into
   `Android/data/com.TrassGames.Yeeps/files/mods/` on the headset, and restart Yeeps.
   The game makes that folder the first time it runs, and the newest blaze APK from
   the Discord is required. Only if their file explorer can't open `Android/data`,
   tell them to plug the headset into a PC and drop the file in with SideQuest.
   To share it with everyone, open a pull request on
   https://github.com/Blazeisodd/Blazes-Apk-mod-menu-addon (see `CONTRIBUTING.md`).
