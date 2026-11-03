// Yeeps Menu: a C++ port of YeepsMenu.ts (ii's menu style) for the Yeeps BNM mod loader.
// The .ts original ran under a Frida/Il2Cpp JS runtime that Yeeps' .so loader does not
// have, so its features are re-implemented here with this repo's SDK, following the
// starter menu's proven patterns. Only own-game features and read-only info tools are
// ported (see AGENTS.md); network/room/account/economy features from the .ts are
// not included.
//   Hold Y (left hand)      -> menu on your left hand, tipped like ii's; turn your palm to you
//                              (B and the right hand with "Right Hand Menu")
//   Touch a button with your other index finger to press it
//   < and > on the sides    -> previous / next page
//   Pages: Movement Mods, Settings, Utility Mods, Enabled Mods
#include "ymsdk.hpp"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

using namespace BNM;
using namespace BNM::Structures::Unity;
using BNM::IL2CPP::Il2CppObject;

YEEPS_MOD("Yeeps Menu")

// ---- small helpers ---------------------------------------------------------

static std::string TypeName(const BNM::IL2CPP::Il2CppType *t) {
    Class c(t);
    if (!c.IsValid() || !c.GetClass() || !c.GetClass()->name) return "?";
    auto *k = c.GetClass();
    return (k->namespaze && k->namespaze[0]) ? std::string(k->namespaze) + "." + k->name : k->name;
}
// Pick an overload by its exact parameter types, e.g. GetComponent(System.Type).
static MethodBase Overload(Class c, const char *name, std::vector<std::string> types) {
    for (auto &m : c.GetMethods(false)) {
        auto *mi = m.GetInfo();
        if (!mi || !mi->name || strcmp(mi->name, name) != 0 || mi->parameters_count != types.size()) continue;
        bool ok = true;
        for (size_t i = 0; i < types.size() && ok; i++) ok = TypeName(mi->parameters[i]) == types[i];
        if (ok) return m;
    }
    return {};
}

// Unity objects we keep between frames must be held by a GC handle, or the
// garbage collector can free them under us.
static void *(*p_handleNew)(void *, bool);
static void *(*p_handleTarget)(void *);
static void (*p_handleFree)(void *);
struct Ref {
    void *h = nullptr;
    void set(Il2CppObject *o) { clear(); if (o && p_handleNew) h = p_handleNew(o, false); }
    Il2CppObject *get() const { return h && p_handleTarget ? (Il2CppObject *)p_handleTarget(h) : nullptr; }
    void clear() { if (h && p_handleFree) p_handleFree(h); h = nullptr; }
};

static Color Lerp(Color a, Color b, float t) {
    return Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1);
}
static Color Rainbow(float h) {             // hue 0..1 -> bright color
    h = h - floorf(h);
    float r = fabsf(h * 6 - 3) - 1, g = 2 - fabsf(h * 6 - 2), b = 2 - fabsf(h * 6 - 4);
    auto c = [](float v) { return v < 0 ? 0.f : v > 1 ? 1.f : v; };
    return Color(c(r), c(g), c(b), 1);
}

// ---- game lookups, resolved once -------------------------------------------

struct XRDevice { uint64_t id; bool initialized; };
static struct {
    bool ok = false;
    Method<Il2CppObject *> createPrimitive, goTransform, goGetComponent, goAddComponent, compTransform, compGameObject;
    Method<void> goSetActive, destroy, trSetParent, trSetPosition, trSetRotation, trSetLocalPosition,
                 trSetLocalRotation, trSetLocalScale, rbSetVelocity, matSetColor, matSetShader,
                 textSetFont, textSetText, textSetFontSize, textSetAlignment, graphicSetColor,
                 canvasSetRenderMode, rtSetSizeDelta, textSetFontStyle, textSetBestFit, textSetMinSize,
                 textSetMaxSize, teleportTo, rbAddForce, rbSetUseGravity, colliderSetEnabled,
                 lrSetPositionCount, lrSetPosition, lrSetStartWidth, lrSetEndWidth, lrSetUseWorldSpace;
    Method<Quaternion> trGetRotation;
    Method<Vector3> trGetPosition, trGetForward, trGetRight, trGetUp, trInverseTransformPoint, trTransformPoint, rbGetVelocity;
    Method<bool> alive, tryGetBool, haptic, raycast;
    Method<XRDevice> deviceAtNode;
    Method<Il2CppObject *> trGetParent, trGetRoot, getComponentsInChildren;
    Method<Il2CppObject *> rendererGetMaterial, shaderFind, findAll, builtinResource;
    Field<Il2CppObject *> leftHand, rightHand, headTransform, bodyCollider, headCollider;
    Field<float> maxJumpSpeed, jumpMultiplier;
    Method<Vector3> plGetVelocity;
    Method<void> plSetVelocity, plNoGravityFor;
    Field<int> locomotionLayers, collisionLayers;
    Method<int> goGetLayer;
    Method<bool> colliderIsTrigger, behaviourGetEnabledCollider;
    Method<void> ignoreLayers;            // a LayerMask: which layers your hands can climb on
    Method<void> goSetLayer;
    BNM::MonoType *tRigidbody = nullptr, *tCollider = nullptr, *tCanvas = nullptr, *tText = nullptr,
                  *tFont = nullptr, *tRenderer = nullptr, *tMeshCollider = nullptr, *tLine = nullptr;
    BNM::MonoType *tSign = nullptr, *tMapObject = nullptr;
    Method<BNM::Structures::Mono::String *> signGetText, mapGetKey, mapGetColor;
    Field<Il2CppObject *> signText;
    BNM::MonoType *tItemInfo = nullptr, *tMobInfo = nullptr;
    Method<BNM::Structures::Mono::String *> itemInfoKey, mobInfoKey;
    Method<Vector3> mapPosition;
    Field<float> rollSpeed;
    Method<BNM::Structures::Mono::String *> objectName;
    Ref shader, font, usagePrimary, usageSecondary, usageGrip, usageTrigger;
} G;

static bool Lookups() {
    if (G.ok) return true;
    auto R = ym::api()->resolve;
    p_handleNew = (decltype(p_handleNew))R("il2cpp_gchandle_new");
    p_handleTarget = (decltype(p_handleTarget))R("il2cpp_gchandle_get_target");
    p_handleFree = (decltype(p_handleFree))R("il2cpp_gchandle_free");
    Class go("UnityEngine", "GameObject"), tr("UnityEngine", "Transform"), obj("UnityEngine", "Object"),
          rb("UnityEngine", "Rigidbody"), mat("UnityEngine", "Material"), rend("UnityEngine", "Renderer"),
          sh("UnityEngine", "Shader"), res("UnityEngine", "Resources"), text("UnityEngine.UI", "Text"),
          graphic("UnityEngine.UI", "Graphic"), canvas("UnityEngine", "Canvas"), rt("UnityEngine", "RectTransform"),
          dev("UnityEngine.XR", "InputDevices"), player("", "MasterPlayer", Image("Assembly-CSharp.dll"));
    if (!go || !tr || !player || !dev) { ym::log("yeeps-menu: game classes missing"); return false; }
    G.createPrimitive = go.GetMethod("CreatePrimitive", 1);
    G.goTransform = go.GetMethod("get_transform", 0);
    G.goGetComponent = Overload(go, "GetComponent", {"System.Type"});
    G.goAddComponent = Overload(go, "AddComponent", {"System.Type"});
    G.goSetActive = go.GetMethod("SetActive", 1);
    Class comp("UnityEngine", "Component");
    G.compTransform = comp.GetMethod("get_transform", 0);
    G.compGameObject = comp.GetMethod("get_gameObject", 0);
    G.destroy = Overload(obj, "Destroy", {"UnityEngine.Object"});
    G.alive = obj.GetMethod("op_Implicit", 1);
    G.trSetParent = Overload(tr, "SetParent", {"UnityEngine.Transform", "System.Boolean"});
    G.trSetPosition = tr.GetMethod("set_position", 1);
    G.trSetRotation = tr.GetMethod("set_rotation", 1);
    G.trSetLocalPosition = tr.GetMethod("set_localPosition", 1);
    G.trSetLocalRotation = tr.GetMethod("set_localRotation", 1);
    G.trSetLocalScale = tr.GetMethod("set_localScale", 1);
    G.trGetPosition = tr.GetMethod("get_position", 0);
    G.trGetRotation = tr.GetMethod("get_rotation", 0);
    G.trGetForward = tr.GetMethod("get_forward", 0);
    G.trGetRight = tr.GetMethod("get_right", 0);
    G.trGetUp = tr.GetMethod("get_up", 0);
    G.trInverseTransformPoint = Overload(tr, "InverseTransformPoint", {"UnityEngine.Vector3"});
    G.trTransformPoint = Overload(tr, "TransformPoint", {"UnityEngine.Vector3"});
    G.rbSetVelocity = rb.GetMethod("set_velocity", 1);
    G.rbGetVelocity = rb.GetMethod("get_velocity", 0);
    G.rendererGetMaterial = rend.GetMethod("get_material", 0);
    G.matSetColor = Overload(mat, "set_color", {"UnityEngine.Color"});
    G.matSetShader = mat.GetMethod("set_shader", 1);
    G.shaderFind = sh.GetMethod("Find", 1);
    G.findAll = Overload(res, "FindObjectsOfTypeAll", {"System.Type"});
    G.textSetFont = text.GetMethod("set_font", 1);
    G.textSetFontStyle = text.GetMethod("set_fontStyle", 1);
    G.textSetBestFit = text.GetMethod("set_resizeTextForBestFit", 1);
    G.textSetMinSize = text.GetMethod("set_resizeTextMinSize", 1);
    G.textSetMaxSize = text.GetMethod("set_resizeTextMaxSize", 1);
    G.textSetText = text.GetMethod("set_text", 1);
    G.textSetFontSize = text.GetMethod("set_fontSize", 1);
    G.textSetAlignment = text.GetMethod("set_alignment", 1);
    G.graphicSetColor = graphic.GetMethod("set_color", 1);
    G.canvasSetRenderMode = canvas.GetMethod("set_renderMode", 1);
    G.rtSetSizeDelta = rt.GetMethod("set_sizeDelta", 1);
    G.deviceAtNode = dev.GetMethod("GetDeviceAtXRNode", 1);
    G.tryGetBool = dev.GetMethod("TryGetFeatureValue_bool", 3);
    G.haptic = dev.GetMethod("SendHapticImpulse", 4);    // optional: a small buzz on press
    G.leftHand = player.GetField("leftHandTransform");
    G.rightHand = player.GetField("rightHandTransform");
    G.headTransform = player.GetField("headTransform");
    G.bodyCollider = player.GetField("bodyCollider");
    G.headCollider = player.GetField("headCollider");
    G.maxJumpSpeed = player.GetField("maxJumpSpeed");
    G.locomotionLayers = player.GetField("locomotionEnabledLayers");
    G.collisionLayers = player.GetField("collisionEnabledLayers");
    G.goGetLayer = go.GetMethod("get_layer", 0);
    G.ignoreLayers = Class("UnityEngine", "Physics").GetMethod("IgnoreLayerCollision", 3);
    // Yeeps moves your body through these, not through a Rigidbody on the player object
    G.plGetVelocity = player.GetMethod("GetVelocity", 0);
    G.plSetVelocity = player.GetMethod("ApplySetVelocityNoHaptic", 1);
    if (!G.plSetVelocity.IsValid()) G.plSetVelocity = player.GetMethod("ApplySetVelocity", 1);
    G.plNoGravityFor = player.GetMethod("ApplyNoGravityDuration", 1);
    ym::log("yeeps-menu: velocity %d/%d, no-gravity %d", G.plGetVelocity.IsValid(), G.plSetVelocity.IsValid(),
            G.plNoGravityFor.IsValid());
    G.goSetLayer = go.GetMethod("set_layer", 1);
    G.jumpMultiplier = player.GetField("jumpMultiplier");
    G.teleportTo = player.GetMethod("TeleportTo", 1);
    G.rbAddForce = Overload(rb, "AddForce", {"UnityEngine.Vector3", "UnityEngine.ForceMode"});
    G.rbSetUseGravity = rb.GetMethod("set_useGravity", 1);
    G.colliderSetEnabled = Class("UnityEngine", "Collider").GetMethod("set_enabled", 1);
    G.behaviourGetEnabledCollider = Class("UnityEngine", "Collider").GetMethod("get_enabled", 0);
    G.colliderIsTrigger = Class("UnityEngine", "Collider").GetMethod("get_isTrigger", 0);
    G.raycast = Overload(Class("UnityEngine", "Physics"), "Raycast",
                         {"UnityEngine.Vector3", "UnityEngine.Vector3", "UnityEngine.RaycastHit", "System.Single", "System.Int32"});
    G.builtinResource = Overload(res, "GetBuiltinResource", {"System.Type", "System.String"});
    Class line("UnityEngine", "LineRenderer");
    G.tLine = line.GetMonoType();
    G.lrSetPositionCount = line.GetMethod("set_positionCount", 1);
    G.lrSetPosition = line.GetMethod("SetPosition", 2);
    G.lrSetStartWidth = line.GetMethod("set_startWidth", 1);
    G.lrSetEndWidth = line.GetMethod("set_endWidth", 1);
    G.lrSetUseWorldSpace = line.GetMethod("set_useWorldSpace", 1);
    G.tMeshCollider = Class("UnityEngine", "MeshCollider").GetMonoType();
    G.trGetParent = tr.GetMethod("get_parent", 0);
    G.trGetRoot = tr.GetMethod("get_root", 0);
    G.getComponentsInChildren = Overload(go, "GetComponentsInChildren", {"System.Type", "System.Boolean"});
    {
        Class sign("", "TextSignMapObject", Image("Assembly-CSharp.dll"));
        if (sign.IsValid()) { G.tSign = sign.GetMonoType(); G.signText = sign.GetField("messageText"); }
        Class mapObj("", "MapObject", Image("Assembly-CSharp.dll"));
        if (mapObj.IsValid()) {
            G.tMapObject = mapObj.GetMonoType();
            G.mapGetKey = mapObj.GetMethod("get_key", 0);
            G.mapGetColor = mapObj.GetMethod("get_colorKey", 0);
        }
        G.signGetText = text.GetMethod("get_text", 0);
    }
    Class itemInfo("", "Item", Image("Assembly-CSharp.dll"));
    if (itemInfo.IsValid()) { G.tItemInfo = itemInfo.GetMonoType(); G.itemInfoKey = itemInfo.GetMethod("get_itemKey", 0); }
    Class mobInfo("", "Mob", Image("Assembly-CSharp.dll"));
    if (mobInfo.IsValid()) { G.tMobInfo = mobInfo.GetMonoType(); G.mobInfoKey = mobInfo.GetMethod("get_mobKey", 0); }
    Class mapInfo("", "MapObject", Image("Assembly-CSharp.dll"));
    if (mapInfo.IsValid()) G.mapPosition = mapInfo.GetMethod("get_placedPosition", 0);
    G.rollSpeed = player.GetField("rollSpeed");
    G.objectName = obj.GetMethod("get_name", 0);
    G.tRigidbody = rb.GetMonoType();
    G.tCollider = Class("UnityEngine", "Collider").GetMonoType();
    G.tRenderer = rend.GetMonoType();
    G.tCanvas = canvas.GetMonoType();
    G.tText = text.GetMonoType();
    G.tFont = Class("UnityEngine", "Font").GetMonoType();
    G.usagePrimary.set((Il2CppObject *)CreateMonoString("PrimaryButton"));
    G.usageSecondary.set((Il2CppObject *)CreateMonoString("SecondaryButton"));
    G.usageGrip.set((Il2CppObject *)CreateMonoString("GripButton"));
    G.usageTrigger.set((Il2CppObject *)CreateMonoString("TriggerButton"));
    for (const char *s : {"Universal Render Pipeline/Unlit", "Universal Render Pipeline/Lit"})
        if (!G.shader.get() && G.shaderFind.IsValid()) G.shader.set(G.shaderFind((BNM::Structures::Mono::String *)CreateMonoString(s)));
    // Arial, like ii's menu (Unity 6 calls it LegacyRuntime.ttf)
    for (const char *f : {"LegacyRuntime.ttf", "Arial.ttf"})
        if (!G.font.get() && G.builtinResource.IsValid() && G.tFont)
            G.font.set(G.builtinResource((Il2CppObject *)G.tFont, CreateMonoString(f)));
    if (!G.font.get() && G.findAll.IsValid() && G.tFont) {   // else any font the game loaded (one time)
        auto *arr = (uint8_t *)G.findAll((Il2CppObject *)G.tFont);
        if (arr && *(uintptr_t *)(arr + 0x18) > 0) G.font.set(((Il2CppObject **)(arr + 0x20))[0]);
    }
    G.ok = G.createPrimitive.IsValid() && G.goTransform.IsValid() && G.trSetPosition.IsValid() &&
           G.deviceAtNode.IsValid() && G.tryGetBool.IsValid() && G.leftHand.IsValid();
    if (!G.ok) ym::log("yeeps-menu: some game methods are missing, menu off");
    return G.ok;
}

static bool Alive(Il2CppObject *o) { return o && G.alive((Il2CppObject *)o); }
static Il2CppObject *Tr(Il2CppObject *gameObject) { return G.goTransform[gameObject](); }
static void Show(Ref &r, bool on) { if (Alive(r.get())) G.goSetActive[r.get()](on); }
static void Kill(Ref &r) { if (Alive(r.get())) G.destroy(r.get()); r.clear(); }

// A primitive with a flat color. collider=false removes its collider so it can't push you.
// Always give new objects the URP shader: Unity's default material only draws in one
// eye on Quest.
static Il2CppObject *Prim(int type, Color c, bool collider) {
    Il2CppObject *o = G.createPrimitive(type);   // 0 = Sphere, 3 = Cube
    if (!o) return nullptr;
    if (!collider) if (auto *col = G.goGetComponent[o](G.tCollider)) G.destroy(col);
    if (auto *r = G.goGetComponent[o](G.tRenderer)) {
        Il2CppObject *m = G.rendererGetMaterial[r]();
        if (m && G.shader.get()) G.matSetShader[m](G.shader.get());
        if (m) G.matSetColor[m](c);
    }
    return o;
}
static Il2CppObject *Cube(Color c, bool collider) { return Prim(3, c, collider); }
static void SetColor(Il2CppObject *o, Color c) {
    if (!Alive(o)) return;
    if (auto *r = G.goGetComponent[o](G.tRenderer)) if (Il2CppObject *m = G.rendererGetMaterial[r]()) G.matSetColor[m](c);
}
static void SetText(Ref &text, const char *s) {
    if (Alive(text.get())) G.textSetText[text.get()](CreateMonoString(s));
}

// ---- input -------------------------------------------------------------------

struct Buttons { bool a = false, b = false, x = false, y = false, lgrip = false, rgrip = false, ltrig = false, rtrig = false; };
static bool Pressed(int node, Ref &usage) {
    XRDevice d = G.deviceAtNode(node);
    bool v = false;
    if (d.initialized) G.tryGetBool(d.id, (BNM::Structures::Mono::String *)usage.get(), &v);
    return v;
}
static Buttons ReadButtons() {
    const int left = 4, right = 5;              // XRNode.LeftHand / RightHand
    Buttons b;
    b.x = Pressed(left, G.usagePrimary);  b.y = Pressed(left, G.usageSecondary);
    b.a = Pressed(right, G.usagePrimary); b.b = Pressed(right, G.usageSecondary);
    b.lgrip = Pressed(left, G.usageGrip); b.rgrip = Pressed(right, G.usageGrip);
    b.ltrig = Pressed(left, G.usageTrigger); b.rtrig = Pressed(right, G.usageTrigger);
    return b;
}

// ---- menu contents -------------------------------------------------------------

// Feature switches. Every feature only changes your own game.
static bool f_ironMan, f_tpGun, f_noclip, f_speedBoost, f_longArms, f_velocityFly, f_noGravity, f_signGun, f_mapGun;
static bool f_fly, f_platforms, f_rainbow, f_fps, f_clock, f_rightHand,
            f_notify = true;
static Ref g_player, g_rigidbody, g_leftHand, g_rightHand, g_head;

static void Notify(const char *fmt, ...);
static void NextTheme();
static void AllOff();
static void PointerToHand();
static void NextPointer();
static void NextIronSpeed();
static void ApplySpeedBoost();
static void ApplyLongArms();
static void ApplyGravity();
static void LocalHitEffect();
static void ApplyRoll();
static bool f_itemInfo, f_mobInfo, f_coordinateGun, f_fastRoll, f_freezeMenu, f_hoverFly, f_objectInfo;
static Vector3 g_savedCoordinate{};
static bool g_hasSavedCoordinate = false;

enum Cat { MAIN, MOVEMENT, SETTINGS, UTILITY, ENABLED, NCAT };
static const char *kCatName[NCAT] = {"Main", "Movement Mods", "Settings", "Utility Mods", "Enabled Mods"};
enum Kind { OPEN, TOGGLE, ACTION };
struct Item {
    Cat cat;
    const char *label;
    Kind kind;
    Cat target = MAIN;            // OPEN: the page it opens
    bool *flag = nullptr;         // TOGGLE: the switch
    void (*fn)() = nullptr;       // ACTION, or called after a TOGGLE changes
};
// Categories from the .ts (Main / Movement / Settings), plus Enabled Mods.
static Item g_items[] = {
    {MAIN, "Settings", OPEN, SETTINGS},
    {MAIN, "Movement Mods", OPEN, MOVEMENT},
    {MAIN, "Utility Mods", OPEN, UTILITY},
    {MAIN, "Enabled Mods", OPEN, ENABLED},

    {MOVEMENT, "Exit Movement Mods", OPEN, MAIN},
    {MOVEMENT, "Platforms", TOGGLE, MAIN, &f_platforms},
    {MOVEMENT, "Fly", TOGGLE, MAIN, &f_fly},
    {MOVEMENT, "Velocity Fly", TOGGLE, MAIN, &f_velocityFly},
    {MOVEMENT, "Teleport Gun", TOGGLE, MAIN, &f_tpGun},
    {MOVEMENT, "Iron Man", TOGGLE, MAIN, &f_ironMan},
    {MOVEMENT, "Noclip", TOGGLE, MAIN, &f_noclip},
    {MOVEMENT, "Long Arms", TOGGLE, MAIN, &f_longArms, ApplyLongArms},
    {MOVEMENT, "Speed Boost", TOGGLE, MAIN, &f_speedBoost, ApplySpeedBoost},
    {MOVEMENT, "No Gravity", TOGGLE, MAIN, &f_noGravity, ApplyGravity},
    {MOVEMENT, "Fast Roll Speed", TOGGLE, MAIN, &f_fastRoll, ApplyRoll},
    {MOVEMENT, "Fly (No gravity)", TOGGLE, MAIN, &f_hoverFly},
    {UTILITY, "Local Hit Effect", ACTION, MAIN, nullptr, LocalHitEffect},

    {UTILITY, "Exit Utility Mods", OPEN, MAIN},
    {UTILITY, "Get Sign Text Gun", TOGGLE, MAIN, &f_signGun},
    {UTILITY, "Get Map Object Gun", TOGGLE, MAIN, &f_mapGun},
    {UTILITY, "Object Info Gun", TOGGLE, MAIN, &f_objectInfo},
    {UTILITY, "Get Item Gun", TOGGLE, MAIN, &f_itemInfo},
    {UTILITY, "Get Mob Data Gun", TOGGLE, MAIN, &f_mobInfo},
    {UTILITY, "Save Map Coordinate Gun", TOGGLE, MAIN, &f_coordinateGun},

    {SETTINGS, "Exit Settings", OPEN, MAIN},
    {SETTINGS, "Freeze Player in Menu", TOGGLE, MAIN, &f_freezeMenu},
    {SETTINGS, "Change Pointer Position", ACTION, MAIN, nullptr, NextPointer},
    {SETTINGS, "Iron Man Speed", ACTION, MAIN, nullptr, NextIronSpeed},
    {SETTINGS, "Right Hand Menu", TOGGLE, MAIN, &f_rightHand, PointerToHand},
    {SETTINGS, "Change Menu Theme", ACTION, MAIN, nullptr, NextTheme},
    {SETTINGS, "Rainbow Menu", TOGGLE, MAIN, &f_rainbow},
    {SETTINGS, "Show FPS", TOGGLE, MAIN, &f_fps},
    {SETTINGS, "Show Clock", TOGGLE, MAIN, &f_clock},
    {SETTINGS, "Notifications", TOGGLE, MAIN, &f_notify},
    {SETTINGS, "Turn Everything Off", ACTION, MAIN, nullptr, AllOff},
};
static constexpr int kItems = sizeof(g_items) / sizeof(g_items[0]);
static Item g_returnItem = {ENABLED, "Exit Enabled Mods", OPEN, MAIN};

// Tooltips, shown under the menu while your finger is on a button (like ii's).
static const struct { const char *label, *tip; } kTips[] = {
    {"Settings", "Opens the settings."},
    {"Movement Mods", "Opens the movement mods."},
    {"Enabled Mods", "Shows every mod you have on."},
    {"Platforms", "Hold a grip to stand on a platform."},
    {"Fly", "Hold A to fly where you look."},
    {"Teleport Gun", "Hold right grip to aim, trigger to teleport."},
    {"Iron Man", "Hold X and A to fly with your hands."},
    {"Noclip", "Hold right trigger to go through walls."},
    {"Speed Boost", "Faster and higher jumps."},
    {"Velocity Fly", "Hold A to shoot where your hand points."},
    {"No Gravity", "Turns your gravity off."},
    {"Utility Mods", "Read-only tools: point at things to see what they are."},
    {"Get Sign Text Gun", "Hold right grip to aim, trigger reads the sign. Turn Teleport Gun off first."},
    {"Get Map Object Gun", "Hold right grip to aim, trigger shows the block name and color."},
    {"Long Arms", "Scales you to 1.25x, like the .ts Long Arms."},
    {"Change Menu Theme", "OG Orange, II Dark, Blaze, Orange, Blue, Green, Purple, Dark."},
    {"Rainbow Menu", "The menu cycles through colors."},
    {"Show FPS", "Frames per second in the title."},
    {"Show Clock", "The time in the title."},
    {"Change Pointer Position", "Moves the ball on your finger."},
    {"Iron Man Speed", "Changes how strong Iron Man is."},
    {"Right Hand Menu", "Hold B, menu on your right hand."},
    {"Notifications", "Messages when you switch something."},
    {"Turn Everything Off", "Switches every mod off."},
};
static const char *TipFor(const char *label) {
    if (strncmp(label, "Exit ", 5) == 0) return "Returns you back to the main category.";
    for (auto &t : kTips) if (strcmp(t.label, label) == 0) return t.tip;
    return "";
}

// Colors in the ii menu style: an animated background and darker buttons when on.
struct Theme { const char *name; Color bgA, bgB, button, buttonOn, title; };
static const Theme kThemes[] = {
    {"OG Orange", Color(1.0f, 0.5f, 0.0f, 1), Color(1.0f, 0.42f, 0.0f, 1), Color(0.666f, 0.333f, 0.0f, 1), Color(0.333f, 0.15f, 0.0f, 1), Color(1.0f, 0.745f, 0.49f, 1)},
    {"II Dark", Color(0.0f, 0.0f, 0.0f, 1), Color(0.05f, 0.05f, 0.05f, 1), Color(0.1f, 0.1f, 0.1f, 1), Color(0.4f, 0.4f, 0.4f, 1), Color(1.0f, 1.0f, 1.0f, 1)},
    {"Blaze",  Color(0.90f, 0.12f, 0.04f, 1), Color(1.0f, 0.42f, 0.0f, 1), Color(0.50f, 0.06f, 0.03f, 1), Color(1.0f, 0.45f, 0.05f, 1), Color(1.0f, 0.62f, 0.20f, 1)},
    {"Orange", Color(1.0f, 0.50f, 0.0f, 1), Color(1.0f, 0.40f, 0.0f, 1), Color(0.67f, 0.33f, 0.0f, 1), Color(0.33f, 0.16f, 0.0f, 1), Color(1.0f, 0.75f, 0.49f, 1)},
    {"Blue",   Color(0.20f, 0.50f, 1.0f, 1), Color(0.10f, 0.30f, 0.85f, 1), Color(0.10f, 0.28f, 0.65f, 1), Color(0.04f, 0.12f, 0.32f, 1), Color(0.70f, 0.85f, 1.0f, 1)},
    {"Green",  Color(0.30f, 0.85f, 0.30f, 1), Color(0.15f, 0.65f, 0.20f, 1), Color(0.12f, 0.45f, 0.12f, 1), Color(0.05f, 0.22f, 0.05f, 1), Color(0.75f, 1.0f, 0.75f, 1)},
    {"Purple", Color(0.65f, 0.30f, 1.0f, 1), Color(0.45f, 0.15f, 0.85f, 1), Color(0.38f, 0.15f, 0.60f, 1), Color(0.18f, 0.06f, 0.30f, 1), Color(0.88f, 0.75f, 1.0f, 1)},
    {"Dark",   Color(0.12f, 0.12f, 0.14f, 1), Color(0.05f, 0.05f, 0.06f, 1), Color(0.22f, 0.22f, 0.25f, 1), Color(0.45f, 0.45f, 0.50f, 1), Color(0.85f, 0.85f, 0.90f, 1)},
};
static constexpr int kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);
static int g_theme = 0;
static const Theme &T() { return kThemes[g_theme]; }

// ---- menu objects ----------------------------------------------------------------

// Layout in meters, matching ii's menu as it was ported to Yeeps. Everything is drawn
// on a "board" object: +x goes across, +y goes up, the player looks at it from -z.
static constexpr int kRows = 8;                                   // buttons per page
static const float kBoardW = 0.30f, kBoardH = 0.3825f, kBtnW = 0.27f, kBtnH = 0.0306f;
static const float kTitleY = 0.235f, kSideX = 0.2f, kSideW = 0.06f, kSideH = 0.344f;
static float RowY(int r) { return 0.13f - r * 0.04f; }

static Ref g_homeButton;
static Ref g_menu, g_board, g_bg, g_outline, g_pointer, g_title, g_titleBar;
static Ref g_rowCube[kRows], g_rowLabel[kRows], g_rowText[kRows];
static Ref g_prev, g_next, g_notifyGo, g_notifyText, g_tipText, g_leftPlatform, g_rightPlatform;
static double g_openTime = 0, g_flashUntil = 0;
static int g_flashRow = -2, g_hover = -2;
static bool g_built = false, g_menuShown = false, g_dirty = true;
static Cat g_cat = MAIN;
static int g_page = 0;
static std::vector<Item *> g_view;                                // what the current page list shows
static double g_time = 0, g_nextPress = 0, g_notifyUntil = 0, g_nextTitle = 0;
static int g_touching = -2, g_frames = 0;
static float g_fps = 0;

static int PageCount() { return g_view.empty() ? 1 : (int)(g_view.size() + kRows - 1) / kRows; }

static void BuildView() {
    g_view.clear();
    if (g_cat == ENABLED) {
        g_view.push_back(&g_returnItem);
        for (auto &it : g_items) if (it.kind == TOGGLE && *it.flag && it.flag != &f_notify) g_view.push_back(&it);
    } else {
        for (auto &it : g_items) if (it.cat == g_cat) g_view.push_back(&it);
    }
    if (g_page >= PageCount()) g_page = 0;
}

static Il2CppObject *Label(Il2CppObject *canvasT, const char *s, Vector3 pos, float w, float h, int size, Ref *textOut) {
    if (!G.font.get() || !G.tText) return nullptr;
    Il2CppObject *go = Class("UnityEngine", "GameObject").CreateNewObjectParameters(CreateMonoString("Label"));
    Il2CppObject *text = go ? G.goAddComponent[go](G.tText) : nullptr;
    if (!text) return nullptr;
    Il2CppObject *t = Tr(go);
    G.trSetParent[t](canvasT, false);
    G.rtSetSizeDelta[t](Vector2(w * 1000, h * 1000));
    G.trSetLocalPosition[t](pos * 1000);
    G.trSetLocalRotation[t](Quaternion(0, 0, 0, 1));
    G.trSetLocalScale[t](Vector3(1, 1, 1));
    G.textSetFont[text](G.font.get());
    G.textSetText[text](CreateMonoString(s));
    G.textSetFontSize[text](size);
    G.textSetAlignment[text](4);                 // MiddleCenter
    if (G.textSetFontStyle.IsValid()) G.textSetFontStyle[text](2);   // Italic, like ii's menu
    if (G.textSetBestFit.IsValid()) {            // shrink long labels to fit their button
        G.textSetBestFit[text](true);
        G.textSetMinSize[text](1);
        G.textSetMaxSize[text](size);
    }
    G.graphicSetColor[text](Color(1, 1, 1, 1));
    if (textOut) textOut->set(text);
    return go;
}

// A world-space canvas (1 unit = 1 mm) under parent.
static Il2CppObject *Canvas(Il2CppObject *parentT, Vector3 pos, float w, float h) {
    Il2CppObject *go = Class("UnityEngine", "GameObject").CreateNewObjectParameters(CreateMonoString("YeepsMenuText"));
    Il2CppObject *canvas = go && G.tCanvas ? G.goAddComponent[go](G.tCanvas) : nullptr;
    if (!canvas) return nullptr;
    G.canvasSetRenderMode[canvas](2);            // WorldSpace
    Il2CppObject *ct = Tr(go);
    G.trSetParent[ct](parentT, false);
    G.rtSetSizeDelta[ct](Vector2(w * 1000, h * 1000));
    G.trSetLocalPosition[ct](pos);
    G.trSetLocalRotation[ct](Quaternion(0, 0, 0, 1));
    G.trSetLocalScale[ct](Vector3(0.001f, 0.001f, 0.001f));
    return go;
}

static Il2CppObject *Part(Il2CppObject *parentT, Color c, Vector3 pos, Vector3 scale) {
    Il2CppObject *o = Cube(c, false);
    if (!o) return nullptr;
    Il2CppObject *t = Tr(o);
    G.trSetParent[t](parentT, false);
    G.trSetLocalPosition[t](pos);
    G.trSetLocalScale[t](scale);
    return o;
}

// Where the ball sits on your hand (ii's four pointer positions).
static const Vector3 kPointerSpots[] = {Vector3(0.01f, -0.017f, 0.19f), Vector3(0, -0.1f, 0.05f),
                                        Vector3(0, 0.1f, -0.05f), Vector3(0, 0.0666f, 0.1f)};
static int g_pointerSpot = 0;

static void PointerToHand() {
    Il2CppObject *p = g_pointer.get(), *hand = f_rightHand ? g_leftHand.get() : g_rightHand.get();
    if (!Alive(p) || !hand) return;
    Il2CppObject *t = Tr(p);
    Vector3 at = kPointerSpots[g_pointerSpot];
    if (f_rightHand) at.x = -at.x;
    G.trSetParent[t](hand, false);
    G.trSetLocalPosition[t](at);
}
static void NextPointer() { g_pointerSpot = (g_pointerSpot + 1) % 4; PointerToHand(); Notify("Pointer position %d", g_pointerSpot + 1); }

// Built once per room; afterwards only shown/hidden, moved and relabeled.
static void BuildMenu() {
    auto NewObject = [](const char *name) {
        return Class("UnityEngine", "GameObject").CreateNewObjectParameters(CreateMonoString(name));
    };
    Il2CppObject *menu = NewObject("YeepsMenu"), *board = NewObject("YeepsMenuBoard");
    if (!menu || !board) return;
    g_menu.set(menu);
    g_board.set(board);
    Il2CppObject *bt = Tr(board);
    G.trSetParent[bt](Tr(menu), false);
    // Like ii's menu: the board stands 10 cm out from the hand on its +x side, facing +x.
    G.trSetLocalPosition[bt](Vector3(0.1f, 0, 0));
    G.trSetLocalRotation[bt](Quaternion::LookRotation(Vector3(-1, 0, 0), Vector3(0, 0, 1)));

    g_outline.set(Part(bt, T().title, Vector3(0, 0, 0.004f), Vector3(kBoardW + 0.008f, kBoardH + 0.008f, 0.004f)));
    g_bg.set(Part(bt, T().bgA, Vector3(0, 0, 0), Vector3(kBoardW, kBoardH, 0.01f)));
    for (int r = 0; r < kRows; r++)
        g_rowCube[r].set(Part(bt, T().button, Vector3(0, RowY(r), -0.005f), Vector3(kBtnW, kBtnH, 0.009f)));
    g_prev.set(Part(bt, T().button, Vector3(-kSideX, 0, 0), Vector3(kSideW, kSideH, 0.009f)));
    // The title sits on its own dark bar above the board (where ii's Disconnect button was)
    g_titleBar.set(Part(bt, T().buttonOn, Vector3(0, kTitleY, -0.002f), Vector3(kBtnW, 0.045f, 0.009f)));
    g_homeButton.set(Part(bt, T().button, Vector3(0, 0.295f, -0.002f), Vector3(kBtnW, 0.035f, 0.009f)));
    g_next.set(Part(bt, T().button, Vector3(kSideX, 0, 0), Vector3(kSideW, kSideH, 0.009f)));

    if (Il2CppObject *canvas = Canvas(bt, Vector3(0, 0, -0.0105f), kBoardW + 2 * kSideX, kBoardH + 0.1f)) {
        Il2CppObject *ct = Tr(canvas);
        Label(ct, "Home", Vector3(0, 0.295f, 0), kBtnW, 0.035f, 24, nullptr);
        Label(ct, "Yeeps Menu", Vector3(0, kTitleY, 0), kBtnW, 0.045f, 32, &g_title);
        for (int r = 0; r < kRows; r++)
            g_rowLabel[r].set(Label(ct, "", Vector3(0, RowY(r), 0), kBtnW, kBtnH, 19, &g_rowText[r]));
        Label(ct, "<", Vector3(-kSideX, 0, -0.0005f), kSideW, 0.05f, 34, nullptr);
        Label(ct, ">", Vector3(kSideX, 0, -0.0005f), kSideW, 0.05f, 34, nullptr);
        Label(ct, "", Vector3(0, -kBoardH / 2 - 0.02f, 0), kBoardW + 0.1f, 0.03f, 15, &g_tipText);
        char ver[64];
        snprintf(ver, sizeof ver, "Yeeps build %s", ym::api()->gameVersion);
        Label(ct, ver, Vector3(0, -kBoardH / 2 + 0.012f, 0), kBoardW, 0.02f, 11, nullptr);
    }
    // Pointer: a small ball on the index finger of the other hand
    if (Il2CppObject *p = Prim(0, T().bgB, false)) {
        G.trSetLocalScale[Tr(p)](Vector3(0.01f, 0.01f, 0.01f));
        g_pointer.set(p);
        PointerToHand();
    }
    // Notifications: a line of text low in your view, like ii's notifications
    if (Il2CppObject *n = Canvas(g_head.get(), Vector3(0, -0.12f, 0.6f), 0.5f, 0.05f)) {
        g_notifyGo.set(n);
        Label(Tr(n), "", Vector3(0, 0, 0), 0.5f, 0.05f, 22, &g_notifyText);
        G.goSetActive[n](false);
    }
    G.goSetActive[menu](false);
    Show(g_pointer, false);
    g_built = true;
    g_dirty = true;
}

static void Notify(const char *fmt, ...) {
    if (!f_notify || !Alive(g_notifyGo.get())) return;
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    SetText(g_notifyText, buf);
    G.goSetActive[g_notifyGo.get()](true);
    g_notifyUntil = g_time + 2.0;
}

// Relabels and recolors the rows. Only runs after something changed.
static void RenderPage() {
    BuildView();
    for (int r = 0; r < kRows; r++) {
        int i = g_page * kRows + r;
        bool used = i < (int)g_view.size();
        Show(g_rowCube[r], used);
        Show(g_rowLabel[r], used);
        if (!used) continue;
        Item *it = g_view[i];
        bool on = it->kind == TOGGLE && *it->flag;
        SetText(g_rowText[r], it->label);
        SetColor(g_rowCube[r].get(), r == g_flashRow ? Color(1, 0.9f, 0.6f, 1) : on ? T().buttonOn : T().button);
        if (Alive(g_rowText[r].get())) G.graphicSetColor[g_rowText[r].get()](on ? Color(1, 1, 1, 1) : T().title);
    }
    bool pages = PageCount() > 1;
    Show(g_prev, pages);
    Show(g_next, pages);
    SetColor(g_prev.get(), T().button);
    SetColor(g_next.get(), T().button);
    SetColor(g_outline.get(), T().title);
    SetColor(g_titleBar.get(), T().buttonOn);
    SetColor(g_pointer.get(), T().bgB);
    if (Alive(g_title.get())) G.graphicSetColor[g_title.get()](Color(1, 1, 1, 1));
    g_nextTitle = 0;                     // refresh the title now
    g_dirty = false;
}

static int Hex(float v) { return (int)(v < 0 ? 0 : v > 1 ? 255 : v * 255); }

// The title is "Yeeps II" with a moving red-orange-yellow gradient, one color per letter
// (rich text). Rebuilt ten times a second, not every frame.
static void UpdateTitle() {
    if (g_time < g_nextTitle) return;
    g_nextTitle = g_time + 0.1;
    static const Color kFire[] = {Color(1, 0.1f, 0.05f, 1), Color(1, 0.45f, 0, 1), Color(1, 0.8f, 0.2f, 1), Color(1, 0.45f, 0, 1)};
    char buf[512];
    int n = 0;
    const char *word = "Yeeps Menu";
    for (int i = 0; word[i]; i++) {
        float t = (float)g_time * 1.5f - i * 0.35f;
        Color c;
        if (f_rainbow) c = Rainbow(t * 0.2f);
        else {
            float f = t - floorf(t / 4) * 4;          // 0..4 along the fire colors
            int k = (int)f;
            c = Lerp(kFire[k], kFire[(k + 1) % 4], f - k);
        }
        n += snprintf(buf + n, sizeof buf - n, "<b><color=#%02X%02X%02X>%c</color></b>", Hex(c.r), Hex(c.g), Hex(c.b), word[i]);
    }
    n += snprintf(buf + n, sizeof buf - n, " <color=grey>[</color><color=white>%d</color><color=grey>]</color>", g_page + 1);
    if (f_fps) n += snprintf(buf + n, sizeof buf - n, "<size=16>  %d fps</size>", (int)(g_fps + 0.5f));
    if (f_clock) {
        time_t now = time(nullptr);
        struct tm lt;
        localtime_r(&now, &lt);
        n += snprintf(buf + n, sizeof buf - n, "<size=16>  %d:%02d</size>", lt.tm_hour % 12 ? lt.tm_hour % 12 : 12, lt.tm_min);
    }
    SetText(g_title, buf);
}

static void ShowMenu(bool show) {
    if (show == g_menuShown) return;
    g_menuShown = show;
    if (show) { g_openTime = g_time; g_dirty = true; g_hover = -2; SetText(g_tipText, ""); }
    Show(g_menu, show);
    Show(g_pointer, show);
    g_touching = -2;
}

static float Dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Like ii's menu: the menu follows your hand and is tipped 45 degrees, so you read it
// by turning your palm toward your face. "Floating Menu" instead keeps the board above
// your hand, always facing you.
static void PlaceMenu() {
    Il2CppObject *menu = g_menu.get(), *board = g_board.get(), *head = g_head.get();
    Il2CppObject *hand = f_rightHand ? g_rightHand.get() : g_leftHand.get();
    if (!Alive(menu) || !Alive(board) || !hand || !head) return;
    Il2CppObject *t = Tr(menu), *bt = Tr(board);
    Vector3 handPos = G.trGetPosition[hand]();
    if (!G.trGetRotation.IsValid()) {                  // fallback: float above the hand, facing you
        Vector3 center = handPos + Vector3(0, 0.25f, 0), away = center - G.trGetPosition[head]();
        if (away.x * away.x + away.y * away.y + away.z * away.z < 1e-6f) return;
        G.trSetPosition[t](handPos);
        G.trSetPosition[bt](center);
        G.trSetRotation[bt](Quaternion::LookRotation(away));
        return;
    }
    const float s = 0.38268343f, c = 0.92387953f;                  // sin/cos of 22.5 degrees
    Quaternion rot = G.trGetRotation[hand]() * Quaternion(-s, 0, 0, c);   // tip -45 degrees on x
    if (f_rightHand) rot = rot * Quaternion(0, 0, 1, 0);           // mirrored: 180 degrees on z
    G.trSetPosition[t](handPos);
    G.trSetRotation[t](rot);
    G.trSetLocalPosition[bt](Vector3(0.1f, 0, 0));
    G.trSetLocalRotation[bt](Quaternion::LookRotation(Vector3(-1, 0, 0), Vector3(0, 0, 1)));
}

static void Press(Item *it) {
    switch (it->kind) {
    case OPEN:
        g_cat = it->target;
        g_page = 0;
        break;
    case TOGGLE:
        *it->flag = !*it->flag;
        if (it->fn) it->fn();
        Notify("[%s] %s", *it->flag ? "ON" : "OFF", it->label);
        break;
    case ACTION:
        if (it->fn) it->fn();
        break;
    }
    g_dirty = true;
    ym::log("yeeps-menu: %s", it->label);
}

// Which part of the menu the pointer touches: row 0..7, -1 = <, kRows = >, -2 = nothing.
static int Touching() {
    Il2CppObject *board = g_board.get(), *pointer = g_pointer.get();
    if (!Alive(board) || !Alive(pointer)) return -2;
    Vector3 p = G.trInverseTransformPoint[Tr(board)](G.trGetPosition[Tr(pointer)]());
    if (p.z < -0.035f || p.z > 0.015f) return -2;
    if (fabsf(p.x) < kBtnW / 2 && fabsf(p.y - 0.295f) < 0.0175f) return -3;
    if (fabsf(fabsf(p.x) - kSideX) < kSideW / 2 && fabsf(p.y) < kSideH / 2 && PageCount() > 1)
        return p.x < 0 ? -1 : kRows;
    if (fabsf(p.x) >= kBtnW / 2) return -2;
    for (int r = 0; r < kRows; r++)
        if (fabsf(p.y - RowY(r)) < kBtnH / 2) return r;
    return -2;
}

static void Buzz() {
    if (!G.haptic.IsValid()) return;
    XRDevice d = G.deviceAtNode(f_rightHand ? 4 : 5);    // the hand that presses
    if (d.initialized) G.haptic(d.id, 0u, 0.35f, 0.04f);
}

static void CheckPress() {
    int hit = Touching();
    if (hit != g_hover) {                                 // tooltip for the button under your finger
        g_hover = hit;
        int i = g_page * kRows + hit;
        if (hit >= 0 && hit < kRows && i < (int)g_view.size()) SetText(g_tipText, TipFor(g_view[i]->label));
    }
    if (hit != -2 && hit != g_touching && g_time >= g_nextPress) {   // one press per touch
        g_nextPress = g_time + 0.25;
        Buzz();
        if (hit >= 0 && hit < kRows) { g_flashRow = hit; g_flashUntil = g_time + 0.12; }
        if (hit == -3) {
            g_cat = MAIN; g_page = 0; g_dirty = true;
        } else if (hit == -1 || hit == kRows) {
            int n = PageCount();
            g_page = (g_page + (hit == kRows ? 1 : n - 1)) % n;
            g_dirty = true;
        } else {
            int i = g_page * kRows + hit;
            if (i < (int)g_view.size()) Press(g_view[i]);
        }
    }
    g_touching = hit;
}

static void AnimateMenu() {
    float open = (float)(g_time - g_openTime) / 0.15f;     // pop open like ii's menu
    if (open <= 1.05f && Alive(g_menu.get())) {
        float k = open >= 1 ? 1 : 1 - (1 - open) * (1 - open);
        G.trSetLocalScale[Tr(g_menu.get())](Vector3(k, k, k));
    }
    if (g_flashRow != -2 && g_time >= g_flashUntil) { g_flashRow = -2; g_dirty = true; }
    float t = 0.5f + 0.5f * sinf((float)g_time * 2.0f);
    Color c = f_rainbow ? Rainbow((float)g_time * 0.15f) : Lerp(T().bgA, T().bgB, t);
    SetColor(g_bg.get(), c);
}

// ---- features (local only) -----------------------------------------------------

static Il2CppObject *PlayerTransform() {
    Il2CppObject *player = g_player.get();
    return player ? G.compTransform[player]() : nullptr;
}

// Your body's velocity, through the game's own player functions (Rigidbody as backup).
static bool GetVel(Vector3 &v) {
    Il2CppObject *player = g_player.get(), *rb = g_rigidbody.get();
    if (player && G.plGetVelocity.IsValid()) { v = G.plGetVelocity[player](); return true; }
    if (rb && G.rbGetVelocity.IsValid()) { v = G.rbGetVelocity[rb](); return true; }
    return false;
}
static void SetVel(Vector3 v) {
    Il2CppObject *player = g_player.get(), *rb = g_rigidbody.get();
    if (player && G.plSetVelocity.IsValid()) G.plSetVelocity[player](v);
    else if (rb && G.rbSetVelocity.IsValid()) G.rbSetVelocity[rb](v);
}

// Where your body is (the body collider), which is what teleports aim for.
static bool BodyPos(Vector3 &out) {
    Il2CppObject *player = g_player.get(), *t = PlayerTransform();
    if (!player || !t) return false;
    Il2CppObject *body = G.bodyCollider.IsValid() ? G.bodyCollider[player]() : nullptr;
    out = body ? G.trGetPosition[G.compTransform[body]()]() : G.trGetPosition[t]();
    return true;
}
static void StopMoving() { SetVel(Vector3(0, 0, 0)); }
// Moves your body to pos, using the game's own TeleportTo when it has one.
static void TeleportPlayer(Vector3 pos) {
    Il2CppObject *player = g_player.get(), *t = PlayerTransform();
    Vector3 body;
    if (!player || !t || !BodyPos(body)) return;
    Vector3 target = pos - body + G.trGetPosition[t]();
    if (G.teleportTo.IsValid()) G.teleportTo[player](target);
    else G.trSetPosition[t](target);
    StopMoving();
}

static const float kIronSpeeds[] = {15, 12, 8, 4};
static int g_ironSpeed = 0;
static void NextIronSpeed() { g_ironSpeed = (g_ironSpeed + 1) % 4; Notify("Iron Man speed: %d", (int)kIronSpeeds[g_ironSpeed]); }
static void NextTheme() { g_theme = (g_theme + 1) % kThemeCount; Notify("Theme: %s", T().name); }
static void AllOff() {
    for (auto &it : g_items) if (it.kind == TOGGLE && it.flag != &f_notify) *it.flag = false;   // ticks undo each mod
    PointerToHand();
    Notify("Everything off");
}

static void FlyTick(const Buttons &in, float dt) {
    Il2CppObject *head = g_head.get(), *t = PlayerTransform();
    if (!f_fly || !in.a || !head || !t) return;
    Vector3 pos = G.trGetPosition[t]() + G.trGetForward[head]() * (25.0f * dt);   // same speed as the .ts Fly
    G.trSetPosition[t](pos);
    StopMoving();
}

// Bounded local-only alternative to the original effect-spam button.
// One reusable sphere, one press per second, no messages sent to the room.
static Ref g_localEffect;
static double g_effectUntil = 0, g_nextEffect = 0;
static void LocalHitEffect() {
    if (g_time < g_nextEffect || !g_rightHand.get()) return;
    g_nextEffect = g_time + 1.0;
    if (!g_localEffect.get()) g_localEffect.set(Prim(0, T().buttonOn, false));
    auto *go = g_localEffect.get();
    if (!go) return;
    G.trSetPosition[Tr(go)](G.trGetPosition[g_rightHand.get()]());
    G.trSetLocalScale[Tr(go)](Vector3(0.15f, 0.15f, 0.15f));
    Show(g_localEffect, true);
    g_effectUntil = g_time + 0.3;
}
static Il2CppObject *g_rollPlayer = nullptr;
static float g_oldRoll = 0;
static void ApplyRoll() {
    auto *player = g_player.get();
    if (!player || !G.rollSpeed.IsValid()) return;
    if (f_fastRoll && g_rollPlayer != player) {
        g_oldRoll = G.rollSpeed[player]();
        G.rollSpeed[player] = 900.0f;
        g_rollPlayer = player;
    } else if (!f_fastRoll && g_rollPlayer) {
        if (g_rollPlayer == player) G.rollSpeed[player] = g_oldRoll;
        g_rollPlayer = nullptr;
    }
}
// Own-player only. Freeze wins over movement while the menu is open.
static bool g_freezeCaptured = false;
static Vector3 g_freezePosition{};
static void ExtraMovementTick(const Buttons &in) {
    if (f_hoverFly && g_player.get() && G.plNoGravityFor.IsValid()) {
        G.plNoGravityFor[g_player.get()](0.25f);
        SetVel(in.a && g_rightHand.get() ? G.trGetForward[g_rightHand.get()]() * 20.0f : Vector3(0, 0, 0));
    }
    auto *t = PlayerTransform();
    if (f_freezeMenu && g_menuShown && t) {
        if (!g_freezeCaptured) { g_freezePosition = G.trGetPosition[t](); g_freezeCaptured = true; }
        G.trSetPosition[t](g_freezePosition);
        StopMoving();
        if (g_player.get() && G.plNoGravityFor.IsValid()) G.plNoGravityFor[g_player.get()](0.25f);
    } else g_freezeCaptured = false;
}
static void VelocityFlyTick(const Buttons &in) {
    Il2CppObject *hand = g_rightHand.get();
    if (!f_velocityFly || !in.a || !hand) return;
    SetVel(G.trGetForward[hand]() * 20.0f);   // same speed as the .ts Velocity Fly
}

static void PlatformFor(bool held, Ref &platform, Ref &hand) {
    if (!held || !f_platforms) {
        Kill(platform);
        return;
    }
    if (platform.get()) return;                             // already standing on one
    if (!hand.get()) return;
    Il2CppObject *p = Cube(T().button, true);               // keeps its collider: you stand on it
    if (!p) return;
    // Hands only collide with the layers in the player's locomotionEnabledLayers (like
    // Gorilla Tag), so the platform goes on the first of those layers.
    static int layer = -2;
    if (layer == -2) {
        layer = -1;
        int mask = G.locomotionLayers.IsValid() && g_player.get() ? G.locomotionLayers[g_player.get()]() : 0;
        for (int i = 0; i < 32 && layer < 0; i++) if (mask & (1 << i)) layer = i;
        ym::log("yeeps-menu: hand layers 0x%x, platforms on layer %d", mask, layer);
    }
    if (layer >= 0 && G.goSetLayer.IsValid()) G.goSetLayer[p](layer);
    Il2CppObject *t = Tr(p);
    G.trSetPosition[t](G.trGetPosition[hand.get()]() - Vector3(0, 0.08f, 0));
    G.trSetLocalScale[t](Vector3(0.28f, 0.02f, 0.28f));
    platform.set(p);
}

static void IronManTick(const Buttons &in, float dt) {
    Vector3 v;
    if (!f_ironMan || !G.trGetRight.IsValid() || (!in.x && !in.a) || !GetVel(v)) return;
    float speed = kIronSpeeds[g_ironSpeed] * dt;
    if (in.x && g_leftHand.get()) v = v + G.trGetRight[g_leftHand.get()]() * -speed;
    if (in.a && g_rightHand.get()) v = v + G.trGetRight[g_rightHand.get()]() * speed;
    SetVel(v);
}

// Speed Boost changes your own player; turning it off puts the old values back.
// Checked every frame so they also come back after a room change.
static float g_oldJumpSpeed = 0, g_oldJumpMultiplier = 0;
static Il2CppObject *g_boosted = nullptr;      // which player has the boost (compared only)

static void ApplySpeedBoost() {
    Il2CppObject *player = g_player.get();
    bool ok = G.maxJumpSpeed.IsValid() && G.jumpMultiplier.IsValid();
    if (f_speedBoost && ok && player && g_boosted != player) {
        g_oldJumpSpeed = G.maxJumpSpeed[player]();
        g_oldJumpMultiplier = G.jumpMultiplier[player]();
        G.maxJumpSpeed[player] = 9.0f;
        G.jumpMultiplier[player] = 1.5f;
        g_boosted = player;
    } else if (!f_speedBoost && g_boosted) {
        if (ok && player == g_boosted) {
            G.maxJumpSpeed[player] = g_oldJumpSpeed;
            G.jumpMultiplier[player] = g_oldJumpMultiplier;
        }
        g_boosted = nullptr;
    }
}

// Long Arms (.ts): scales your own player to 1.25x while on, back to 1x when off.
// Checked every frame (like ApplySpeedBoost) so the scale is restored after the
// toggle is switched off or by "Turn Everything Off", and reapplied in a new room.
static Il2CppObject *g_longArmsPlayer = nullptr;   // which player is scaled (compared only)
static void ApplyLongArms() {
    Il2CppObject *player = g_player.get(), *t = PlayerTransform();
    if (f_longArms && player && t && g_longArmsPlayer != player) {
        G.trSetLocalScale[t](Vector3(1.25f, 1.25f, 1.25f));
        g_longArmsPlayer = player;
    } else if (!f_longArms && g_longArmsPlayer) {
        if (player == g_longArmsPlayer && t) G.trSetLocalScale[t](Vector3(1, 1, 1));
        g_longArmsPlayer = nullptr;
    }
}

// No Gravity: the game's own "no gravity for a while" (what its levitation items
// use), renewed every frame while on; it wears off by itself once you turn it off.
static bool g_gravityOff = false;
static void ApplyGravity() {
    Il2CppObject *player = g_player.get();
    if (f_noGravity && player && G.plNoGravityFor.IsValid()) G.plNoGravityFor[player](0.25f);
    g_gravityOff = f_noGravity;
}

// Noclip: while you hold the right trigger you pass through everything. Yeeps moves
// your body with its own layer lists (locomotionEnabledLayers for your hands,
// collisionEnabledLayers for your body), so those are emptied, physics ignores your
// body against those layers, and your own colliders are off. Gravity stays normal, like
// Gorilla Tag noclip. All of it goes back when you let go. Only your own player changes; nothing is sent.
static bool g_noclipOn = false;
static Ref g_noclipOff[24];                    // your own colliders switched off during noclip
static int g_noclipOffN = 0;
// Every solid collider on your own player (body, head, hands...), switched off while
// noclip is held so nothing of you hits the level.
static void PlayerColliders(bool off) {
    if (!off) {
        for (int i = 0; i < g_noclipOffN; i++) {
            if (Alive(g_noclipOff[i].get())) G.colliderSetEnabled[g_noclipOff[i].get()](true);
            g_noclipOff[i].clear();
        }
        g_noclipOffN = 0;
        return;
    }
    Il2CppObject *player = g_player.get();
    if (!player || !G.colliderSetEnabled.IsValid()) return;
    auto offOne = [](Il2CppObject *c) {
        if (!Alive(c) || g_noclipOffN >= 24 || !G.behaviourGetEnabledCollider[c]()) return;
        if (G.colliderIsTrigger.IsValid() && G.colliderIsTrigger[c]()) return;
        for (int i = 0; i < g_noclipOffN; i++) if (g_noclipOff[i].get() == c) return;
        G.colliderSetEnabled[c](false);
        g_noclipOff[g_noclipOffN++].set(c);
    };
    // Your body and head colliders (fields on the player), then every solid collider in
    // the rig they belong to.
    Il2CppObject *own[2] = {G.bodyCollider.IsValid() ? G.bodyCollider[player]() : nullptr,
                            G.headCollider.IsValid() ? G.headCollider[player]() : nullptr};
    for (Il2CppObject *c : own) {
        if (!c) continue;
        offOne(c);
        Il2CppObject *rig = G.compGameObject[G.trGetRoot[G.compTransform[c]()]()]();
        auto *arr = rig && G.getComponentsInChildren.IsValid()
                        ? (uint8_t *)G.getComponentsInChildren[rig]((Il2CppObject *)G.tCollider, false) : nullptr;
        size_t n = arr ? *(uintptr_t *)(arr + 0x18) : 0;
        for (size_t i = 0; i < n; i++) offOne(((Il2CppObject **)(arr + 0x20))[i]);
    }
}
static int g_savedLoco = 0, g_savedColl = 0, g_bodyLayer = -1;
static void NoclipTick(const Buttons &in) {
    Il2CppObject *player = g_player.get();
    bool want = f_noclip && in.rtrig && player;
    if (want == g_noclipOn) return;
    g_noclipOn = want;
    if (!player || !G.locomotionLayers.IsValid()) return;
    if (want) {
        g_savedLoco = G.locomotionLayers[player]();
        g_savedColl = G.collisionLayers.IsValid() ? G.collisionLayers[player]() : 0;
        Il2CppObject *body = G.bodyCollider.IsValid() ? G.bodyCollider[player]() : nullptr;
        Il2CppObject *bodyGo = body ? G.compGameObject[body]() : nullptr;
        g_bodyLayer = bodyGo && G.goGetLayer.IsValid() ? G.goGetLayer[bodyGo]() : -1;
    }
    int mask = g_savedLoco | g_savedColl;
    G.locomotionLayers[player] = want ? 0 : g_savedLoco;
    if (G.collisionLayers.IsValid()) G.collisionLayers[player] = want ? 0 : g_savedColl;
    if (g_bodyLayer >= 0 && G.ignoreLayers.IsValid())
        for (int l = 0; l < 32; l++)
            if (mask & (1 << l)) G.ignoreLayers(g_bodyLayer, l, want);
    PlayerColliders(want);
    if (want) ym::log("yeeps-menu: noclip on (layers 0x%x, body layer %d, %d colliders off)", mask, g_bodyLayer, g_noclipOffN);
}

// Finds a component of the given type on the hit object or one of its parents.
static Il2CppObject *ParentComp(Il2CppObject *collider, BNM::MonoType *type) {
    if (!collider || !type || !G.trGetParent.IsValid()) return nullptr;
    Il2CppObject *t = G.compTransform[collider]();
    for (int up = 0; up < 8 && t; ++up) {
        Il2CppObject *go = G.compGameObject[t]();
        if (go) if (Il2CppObject *c = G.goGetComponent[go]((Il2CppObject *)type)) return c;
        t = G.trGetParent[t]();
    }
    return nullptr;
}
// Get Sign Text Gun (read-only): shows the text of the sign you point at.
static void ReadSign(Il2CppObject *collider) {
    Il2CppObject *sign = ParentComp(collider, G.tSign);
    Il2CppObject *textObj = sign && G.signText.IsValid() ? G.signText[sign]() : nullptr;
    auto *str = textObj && G.signGetText.IsValid() ? G.signGetText[textObj]() : nullptr;
    if (str) Notify("%s", str->str().c_str());
    else Notify("No sign there");
}
// Get Map Object Gun (read-only): shows the name and color of the block you point at.
static void ReadMapObject(Il2CppObject *collider) {
    Il2CppObject *mapObj = ParentComp(collider, G.tMapObject);
    if (!mapObj) { Notify("No map object there"); return; }
    auto *key = G.mapGetKey.IsValid() ? G.mapGetKey[mapObj]() : nullptr;
    auto *color = G.mapGetColor.IsValid() ? G.mapGetColor[mapObj]() : nullptr;
    Notify("%s / %s", key ? key->str().c_str() : "?", color ? color->str().c_str() : "?");
}

// Teleport Gun: hold right grip to aim (a line and a ball show where you land), press
// the right trigger to go there. The info guns share the same aim; Teleport wins if
// more than one of them is switched on.
static Ref g_gunLine, g_gunLineGo, g_gunBall;
static bool g_gunTrigger = false;
static void TeleportGunTick(const Buttons &in) {
    bool aiming = (f_tpGun || f_signGun || f_mapGun || f_itemInfo || f_mobInfo || f_coordinateGun || f_objectInfo) && in.rgrip;
    if (!aiming) {
        Show(g_gunLineGo, false);
        Show(g_gunBall, false);
        g_gunTrigger = in.rtrig;
        return;
    }
    Il2CppObject *hand = g_rightHand.get();
    if (!hand || !G.raycast.IsValid() || !G.lrSetPosition.IsValid()) return;
    if (!g_gunBall.get()) {                              // made once per room
        Il2CppObject *ball = Prim(0, T().bgB, false);
        Il2CppObject *lineGo = Class("UnityEngine", "GameObject").CreateNewObjectParameters(CreateMonoString("YeepsGunLine"));
        Il2CppObject *line = lineGo && G.tLine ? G.goAddComponent[lineGo](G.tLine) : nullptr;
        if (!ball || !line) return;
        G.trSetLocalScale[Tr(ball)](Vector3(0.1f, 0.1f, 0.1f));
        if (Il2CppObject *m = G.rendererGetMaterial[line]()) {
            if (G.shader.get()) G.matSetShader[m](G.shader.get());
            G.matSetColor[m](T().bgB);
        }
        G.lrSetStartWidth[line](0.025f);
        G.lrSetEndWidth[line](0.025f);
        G.lrSetPositionCount[line](2);
        G.lrSetUseWorldSpace[line](true);
        g_gunBall.set(ball);
        g_gunLine.set(line);
        g_gunLineGo.set(lineGo);
    }
    Vector3 start = G.trGetPosition[hand](), dir = G.trGetForward[hand]();
    BNM::Structures::Unity::RaycastHit hit{};
    bool found = G.raycast(start + dir * 0.25f, dir, &hit, 512.0f, -5);   // Physics.DefaultRaycastLayers
    Vector3 end = found ? hit.point : start + dir * 512.0f;
    Show(g_gunLineGo, true);
    Show(g_gunBall, true);
    G.trSetPosition[Tr(g_gunBall.get())](end);
    G.lrSetPosition[g_gunLine.get()](0, start);
    G.lrSetPosition[g_gunLine.get()](1, end);
    SetColor(g_gunBall.get(), in.rtrig ? T().buttonOn : T().bgB);
    if (in.rtrig && !g_gunTrigger && found) {
        if (f_tpGun) TeleportPlayer(end + Vector3(0, 1.0f, 0));
        else if (f_signGun) ReadSign((Il2CppObject *)hit.GetCollider());
        else if (f_mapGun) ReadMapObject((Il2CppObject *)hit.GetCollider());
        else {
            auto *collider = (Il2CppObject *)hit.GetCollider();
            if (f_objectInfo) {
                auto *name = collider && G.objectName.IsValid() ? G.objectName[collider]() : nullptr;
                auto *t = collider ? G.compTransform[collider]() : nullptr;
                auto *parent = t && G.trGetParent.IsValid() ? G.trGetParent[t]() : nullptr;
                auto *parentName = parent && G.objectName.IsValid() ? G.objectName[parent]() : nullptr;
                Notify("%s / parent: %s", name ? name->str().c_str() : "?", parentName ? parentName->str().c_str() : "none");
            } else if (f_itemInfo || f_mobInfo) {
                auto *obj = ParentComp(collider, f_itemInfo ? G.tItemInfo : G.tMobInfo);
                auto method = f_itemInfo ? G.itemInfoKey : G.mobInfoKey;
                auto *key = obj && method.IsValid() ? method[obj]() : nullptr;
                Notify("%s", key ? key->str().c_str() : "No readable item/mob there");
            } else {
                auto *obj = ParentComp(collider, G.tMapObject);
                if (obj && G.mapPosition.IsValid()) {
                    g_savedCoordinate = G.mapPosition[obj]();
                    g_hasSavedCoordinate = true;
                    Notify("Saved: %.2f, %.2f, %.2f", g_savedCoordinate.x, g_savedCoordinate.y, g_savedCoordinate.z);
                } else Notify("No map coordinates there");
            }
        }
    }
    g_gunTrigger = in.rtrig;
}

// ---- callbacks ---------------------------------------------------------------

static void OnRoomChange() {
    Kill(g_localEffect);
    Kill(g_homeButton);
    g_freezeCaptured = false;
    g_rollPlayer = nullptr;
    g_hasSavedCoordinate = false;
    // The old room's objects are gone (or about to be): forget them, build again.
    for (int r = 0; r < kRows; r++) { g_rowCube[r].clear(); g_rowLabel[r].clear(); g_rowText[r].clear(); }
    for (Ref *r : {&g_board, &g_bg, &g_outline, &g_title, &g_titleBar, &g_prev, &g_next, &g_notifyText, &g_tipText}) r->clear();
    for (Ref *r : {&g_menu, &g_pointer, &g_notifyGo, &g_leftPlatform, &g_rightPlatform, &g_gunBall, &g_gunLineGo}) Kill(*r);
    g_gunLine.clear();
    if (g_noclipOn && g_player.get()) {           // leaving the room mid-noclip: put things back
        Buttons none;                             // trigger "released"
        NoclipTick(none);
    }
    g_boosted = nullptr;
    g_longArmsPlayer = nullptr;
    g_gravityOff = false;
    for (Ref *r : {&g_player, &g_rigidbody, &g_leftHand, &g_rightHand, &g_head}) r->clear();
    g_built = false; g_menuShown = false; g_touching = -2;
}

static void OnUpdate(float dt) {
    g_time += dt;
    g_frames++;
    static double fpsStart = 0;
    if (g_time - fpsStart >= 1.0) { g_fps = (float)(g_frames / (g_time - fpsStart)); g_frames = 0; fpsStart = g_time; }
    if (!Lookups()) return;
    auto *player = (Il2CppObject *)ym::api()->localPlayer();
    if (!player) return;
    if (player != g_player.get()) {                 // first frame in this room
        OnRoomChange();
        g_player.set(player);
        g_leftHand.set(G.leftHand[player]());
        g_rightHand.set(G.rightHand[player]());
        g_head.set(G.headTransform[player]());
        Il2CppObject *pgo = G.compGameObject[player]();
        if (pgo) g_rigidbody.set(G.goGetComponent[pgo](G.tRigidbody));
    }
    if (!g_leftHand.get() || !g_rightHand.get() || !g_head.get()) return;
    if (!g_built) BuildMenu();

    Buttons in = ReadButtons();
    ShowMenu(f_rightHand ? in.b : in.y);
    if (g_menuShown) {
        PlaceMenu();
        CheckPress();
        if (g_dirty) RenderPage();
        UpdateTitle();
        AnimateMenu();
    }
    if (g_notifyUntil > 0 && g_time >= g_notifyUntil) { Show(g_notifyGo, false); g_notifyUntil = 0; }
    FlyTick(in, dt);
    PlatformFor(in.lgrip, g_leftPlatform, g_leftHand);
    PlatformFor(in.rgrip, g_rightPlatform, g_rightHand);
    ApplyRoll();
    if (g_time >= g_effectUntil) Show(g_localEffect, false);
    VelocityFlyTick(in);
    IronManTick(in, dt);
    ApplySpeedBoost();
    ApplyLongArms();
    ApplyGravity();
    NoclipTick(in);
    TeleportGunTick(in);
    ExtraMovementTick(in);
}

YM_EXPORT int YeepsMod_Init(const YeepsModApi *api) {
    if (!ym::init(api)) return -1;
    ym::onUpdate(OnUpdate);
    ym::log("Yeeps Menu loaded on %s (%s)", api->gameVersion, api->buildId);
    return 0;
}
