// Blaze Menu (the starter menu): a hand menu in the style of ii's Stupid Menu (Gorilla Tag).
// The layout idea comes from ii's menu; the code is new.
//   Hold Y (left hand)      -> menu on your left hand, tipped like ii's; turn your palm to you
//                              (B and the right hand with "Right Hand Menu")
//   Touch a button with your other index finger to press it
//   < and > on the sides    -> previous / next page
//   Pages: Movement Mods, Rig Mods, Items, Settings, Enabled Mods
// Movement only changes your own game. Rig mods (dances) and Items are seen by the room,
// within the limits in AGENTS.md ("Things others see"). Read docs/GUIDE.md before adding.
#include "ymsdk.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <dirent.h>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

using namespace BNM;
using namespace BNM::Structures::Unity;
using BNM::IL2CPP::Il2CppObject;

YEEPS_MOD("DARK MATTER")

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
    Method<bool> alive, tryGetBool, tryGetVector2, haptic, raycast;
    Method<XRDevice> deviceAtNode;
    Method<Il2CppObject *> trGetParent, trGetRoot, getComponentsInChildren, netLocal;
    Method<void> pvRpc;
    BNM::IL2CPP::MethodInfo *rpcCreateItem = nullptr;     // only read for its parameter types
    BNM::IL2CPP::Il2CppClass *objectClass = nullptr, *stringClass = nullptr;
    BNM::MonoType *tPhotonView = nullptr, *tItem = nullptr;
    Method<BNM::Structures::Mono::String *> itemKey, itemId;
    Method<bool> itemHeld;
    Method<void> itemDrop, setHeldItem;
    void *(*arrayNew)(void *, uintptr_t) = nullptr;
    void *(*valueBox)(void *, void *) = nullptr;
    Method<bool> behaviourGetEnabled;
    Method<void> behaviourSetEnabled;
    std::vector<BNM::MonoType *> trackerTypes;
    Method<Il2CppObject *> rendererGetMaterial, shaderFind, findAll, findScene, findSceneByType, builtinResource;
    Field<Il2CppObject *> leftHand, rightHand, headTransform, bodyCollider, headCollider;
    Field<float> maxJumpSpeed, jumpMultiplier;
    Method<Vector3> plGetVelocity;
    Method<void> plSetVelocity, plNoGravityFor;
    Field<int> locomotionLayers, collisionLayers;
    Method<int> goGetLayer;
    Method<bool> colliderIsTrigger, behaviourGetEnabledCollider;
    Method<void> ignoreLayers;            // a LayerMask: which layers your hands can climb on
    Method<Il2CppObject *> audioClipCreate, applicationPersistentPath;
    Method<bool> audioClipSetData;
    Method<void> audioSourceSetClip, audioSourcePlay, audioSourceStop, audioSourceSetVolume, audioSourceSetSpatialBlend;
    BNM::MonoType *tAudioSource = nullptr, *tRecorder = nullptr;
    Method<int> recorderGetSourceType;
    Method<void> recorderSetSourceType, recorderSetAudioClip, recorderSetLoopAudioClip, recorderRestart;
    Method<Il2CppObject *> recorderGetAudioClip;
    Method<bool> recorderGetLoopAudioClip, recorderGetVoiceDetection, recorderGetTransmitEnabled,
                 recorderGetTransmitting, recorderGetRecording;
    Method<void> recorderSetVoiceDetection, recorderSetRecording;
    Method<void> goSetLayer;
    BNM::MonoType *tRigidbody = nullptr, *tCollider = nullptr, *tCanvas = nullptr, *tText = nullptr,
                  *tFont = nullptr, *tRenderer = nullptr, *tMeshCollider = nullptr, *tLine = nullptr;
    Ref shader, font, usagePrimary, usageSecondary, usageGrip, usageTrigger, usageAxis;
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
          dev("UnityEngine.XR", "InputDevices"), player("", "MasterPlayer", Image("Assembly-CSharp.dll")),
          audioClip("UnityEngine", "AudioClip"), audioSource("UnityEngine", "AudioSource"),
          application("UnityEngine", "Application");
    if (!go || !tr || !player || !dev) { ym::log("dark-matter: game classes missing"); return false; }
    G.createPrimitive = go.GetMethod("CreatePrimitive", 1);
    G.goTransform = go.GetMethod("get_transform", 0);
    G.goGetComponent = Overload(go, "GetComponent", {"System.Type"});
    G.goAddComponent = Overload(go, "AddComponent", {"System.Type"});
    G.audioClipCreate = audioClip.GetMethod("Create", 5);
    G.audioClipSetData = audioClip.GetMethod("SetData", 2);
    G.audioSourceSetClip = audioSource.GetMethod("set_clip", 1);
    G.audioSourcePlay = audioSource.GetMethod("Play", 0);
    G.audioSourceStop = audioSource.GetMethod("Stop", 0);
    G.audioSourceSetVolume = audioSource.GetMethod("set_volume", 1);
    G.audioSourceSetSpatialBlend = audioSource.GetMethod("set_spatialBlend", 1);
    G.tAudioSource = audioSource.GetMonoType();
    G.applicationPersistentPath = application.GetMethod("get_persistentDataPath", 0);
    Class recorder("Photon.Voice.Unity", "Recorder");
    if (recorder.IsValid()) {
        G.tRecorder = recorder.GetMonoType();
        G.recorderGetSourceType = recorder.GetMethod("get_SourceType", 0);
        G.recorderSetSourceType = recorder.GetMethod("set_SourceType", 1);
        G.recorderGetAudioClip = recorder.GetMethod("get_AudioClip", 0);
        G.recorderSetAudioClip = recorder.GetMethod("set_AudioClip", 1);
        G.recorderGetLoopAudioClip = recorder.GetMethod("get_LoopAudioClip", 0);
        G.recorderSetLoopAudioClip = recorder.GetMethod("set_LoopAudioClip", 1);
        G.recorderGetVoiceDetection = recorder.GetMethod("get_VoiceDetection", 0);
        G.recorderSetVoiceDetection = recorder.GetMethod("set_VoiceDetection", 1);
        G.recorderGetTransmitEnabled = recorder.GetMethod("get_TransmitEnabled", 0);
        G.recorderGetTransmitting = recorder.GetMethod("get_IsCurrentlyTransmitting", 0);
        G.recorderGetRecording = recorder.GetMethod("get_RecordingEnabled", 0);
        if (!G.recorderGetRecording.IsValid()) G.recorderGetRecording = recorder.GetMethod("get_IsRecording", 0);
        G.recorderSetRecording = recorder.GetMethod("set_RecordingEnabled", 1);
        if (!G.recorderSetRecording.IsValid()) G.recorderSetRecording = recorder.GetMethod("set_IsRecording", 1);
        G.recorderRestart = recorder.GetMethod("RestartRecording", 0);
    }
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
    G.tryGetVector2 = dev.GetMethod("TryGetFeatureValue_Vector2f", 3);
    if (!G.tryGetVector2.IsValid()) G.tryGetVector2 = dev.GetMethod("TryGetFeatureValue_Vector2", 3);
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
    ym::log("dark-matter: velocity %d/%d, no-gravity %d", G.plGetVelocity.IsValid(), G.plSetVelocity.IsValid(),
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
    G.findScene = Overload(obj, "FindObjectsOfType", {"System.Type"});      // only when noclip starts
    G.findSceneByType = Overload(obj, "FindObjectsByType", {"System.Type", "UnityEngine.FindObjectsSortMode"});
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
    Class net("", "NetworkMessenger", Image("Assembly-CSharp.dll"));
    if (net.IsValid()) {
        G.netLocal = net.GetMethod("get_local", 0);
        for (auto &m : net.GetMethods(false))
            if (m.GetInfo() && m.GetInfo()->name && strcmp(m.GetInfo()->name, "RpcCreateItem") == 0) G.rpcCreateItem = m.GetInfo();
    }
    {
        Class pv("Photon.Pun", "PhotonView");
        G.tPhotonView = pv.IsValid() ? pv.GetMonoType() : nullptr;
        if (pv.IsValid()) G.pvRpc = Overload(pv, "RPC", {"System.String", "Photon.Pun.RpcTarget", "System.Object[]"});
        Class item("", "Item", Image("Assembly-CSharp.dll"));
        if (item.IsValid()) {
            G.tItem = item.GetMonoType();
            G.itemKey = item.GetMethod("get_itemKey", 0);
            G.itemHeld = item.GetMethod("IsHeldOrAnchoredToAnyPlayer", 0);
            G.itemId = item.GetMethod("get_itemID", 0);
            G.itemDrop = item.GetMethod("TrySendDrop", 0);
            G.setHeldItem = Class("", "MasterPlayer", Image("Assembly-CSharp.dll")).GetMethod("SetHeldItem", 2);
        }
        G.objectClass = Class("System", "Object").GetClass();
        G.stringClass = Class("System", "String").GetClass();
        G.arrayNew = (decltype(G.arrayNew))ym::api()->resolve("il2cpp_array_new");
        G.valueBox = (decltype(G.valueBox))ym::api()->resolve("il2cpp_value_box");
        ym::log("dark-matter: items rpc=%d pv=%d item=%d box=%d", G.rpcCreateItem != nullptr, G.pvRpc.IsValid(),
                G.itemKey.IsValid(), G.arrayNew && G.valueBox);
    }
    Class behaviour("UnityEngine", "Behaviour");
    G.behaviourGetEnabled = behaviour.GetMethod("get_enabled", 0);
    G.behaviourSetEnabled = behaviour.GetMethod("set_enabled", 1);
    // What moves your hands with the controllers (paused while you dance)
    for (auto nc : std::vector<std::pair<const char *, const char *>>{
             {"UnityEngine.XR.Interaction.Toolkit", "XRBaseController"},
             {"UnityEngine.InputSystem.XR", "TrackedPoseDriver"},
             {"UnityEngine.SpatialTracking", "TrackedPoseDriver"}}) {
        Class c(nc.first, nc.second);
        if (c.IsValid()) G.trackerTypes.push_back(c.GetMonoType());
    }
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
    G.usageAxis.set((Il2CppObject *)CreateMonoString("Primary2DAxis"));
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
    if (!G.ok) ym::log("dark-matter: some game methods are missing, menu off");
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
static bool f_velocityFly, f_ironMan, f_tpGun, f_noclip, f_speedBoost, f_noGravity;
static bool f_itemGun, f_spawnOnHand, f_car, f_launchGun, f_megaJump, f_flingGun;
static char g_itemKey[64] = "";                // the last item you held (learned on grab)
struct PendingItem { Vector3 pos, vel; };
static std::vector<PendingItem> g_itemQueue;
static double g_nextItem = 0, g_nextBatch = 0;
static int g_dupeArmed = 0;

static constexpr int kDances = 16;
static bool g_danceSel[kDances];
static int g_dance = -1;            // the picked dance, -1 = none
static bool f_fly, f_fastFly, f_platforms, f_lowGravity, f_rainbow, f_fps, f_clock, f_rightHand,
            f_notify = true;
static bool f_superSpeed, f_superFly, f_rocketBoots, f_dash, f_grapple, f_groundPound, f_thumbstickFly;
static bool f_rain, f_snow;
static Ref g_soundSource, g_soundClip;
static std::vector<std::string> g_soundFiles;
static std::string g_soundDirectory;
static size_t g_soundIndex = 0;
static float g_soundVolume = 0.75f;
static Ref g_player, g_rigidbody, g_leftHand, g_rightHand, g_head;
static Vector3 g_spot;
static bool g_haveSpot = false;

static void Notify(const char *fmt, ...);
static void SaveSpot();
static void GoToSpot();
static void NextTheme();
static void AllOff();
static void PointerToHand();
static void NextPointer();
static void NextIronSpeed();
static void NextCarSpeed();
static void NextJumpPower();
static void NextMenuSize();
static void ApplySpeedBoost();
static void ApplyGravity();
static void DancePicked();
static void ArmDupe9();
static void ArmDupe27();
static void ArmDupe27Wide();
static void SpawnItemCircle();
static void SpawnItemRain();
static bool LeftHandItem();
static void ScanSounds();
static void PlaySound();
static void StopSound();
static void PreviousSound();
static void NextSound();
static void SoundVolumeUp();
static void SoundVolumeDown();

enum Cat { MAIN, MOVEMENT, RIG, ITEMS, SETTINGS, EFFECTS, SOUND, ENABLED, NCAT };
static const char *kCatName[NCAT] = {"Main", "Movement Mods", "Rig Mods", "Items", "Settings", "Effects", "Soundboard", "Enabled Mods"};
enum Kind { OPEN, TOGGLE, ACTION };
struct Item {
    Cat cat;
    const char *label;
    Kind kind;
    Cat target = MAIN;            // OPEN: the page it opens
    bool *flag = nullptr;         // TOGGLE: the switch
    void (*fn)() = nullptr;       // ACTION, or called after a TOGGLE changes
};
// The owner's layout (2026-10-31). Keep this order.
static Item g_items[] = {
    {MAIN, "Settings", OPEN, SETTINGS},
    {MAIN, "Movement Mods", OPEN, MOVEMENT},
    {MAIN, "Rig Mods", OPEN, RIG},
    {MAIN, "Items", OPEN, ITEMS},
    {MAIN, "Effects", OPEN, EFFECTS},
    {MAIN, "Soundboard", OPEN, SOUND},
    {MAIN, "Enabled Mods", OPEN, ENABLED},

    {MOVEMENT, "Exit Movement Mods", OPEN, MAIN},
    {MOVEMENT, "Platforms", TOGGLE, MAIN, &f_platforms},
    {MOVEMENT, "Fly", TOGGLE, MAIN, &f_fly},
    {MOVEMENT, "Fast Fly", TOGGLE, MAIN, &f_fastFly},
    {MOVEMENT, "Velocity Fly", TOGGLE, MAIN, &f_velocityFly},
    {MOVEMENT, "Car", TOGGLE, MAIN, &f_car},
    {MOVEMENT, "Teleport Gun", TOGGLE, MAIN, &f_tpGun},
    {MOVEMENT, "Iron Man", TOGGLE, MAIN, &f_ironMan},
    {MOVEMENT, "Noclip", TOGGLE, MAIN, &f_noclip},
    {MOVEMENT, "Speed Boost", TOGGLE, MAIN, &f_speedBoost, ApplySpeedBoost},
    {MOVEMENT, "No Gravity", TOGGLE, MAIN, &f_noGravity, ApplyGravity},
    {MOVEMENT, "Low Gravity", TOGGLE, MAIN, &f_lowGravity},
    {MOVEMENT, "Save Spot", ACTION, MAIN, nullptr, SaveSpot},
    {MOVEMENT, "Go To Spot", ACTION, MAIN, nullptr, GoToSpot},
    {MOVEMENT, "Super Speed", TOGGLE, MAIN, &f_superSpeed},
    {MOVEMENT, "Super Fly", TOGGLE, MAIN, &f_superFly},
    {MOVEMENT, "Rocket Boots", TOGGLE, MAIN, &f_rocketBoots},
    {MOVEMENT, "Dash", TOGGLE, MAIN, &f_dash},
    {MOVEMENT, "Grapple Hook", TOGGLE, MAIN, &f_grapple},
    {MOVEMENT, "Ground Pound", TOGGLE, MAIN, &f_groundPound},
    {MOVEMENT, "Mega Jump", TOGGLE, MAIN, &f_megaJump},
    {MOVEMENT, "Thumbstick Fly", TOGGLE, MAIN, &f_thumbstickFly},
    {MOVEMENT, "Jump Power", ACTION, MAIN, nullptr, NextJumpPower},
    {MOVEMENT, "Fling Gun", TOGGLE, MAIN, &f_flingGun},

    {RIG, "Exit Rig Mods", OPEN, MAIN},
    {RIG, "Wave", TOGGLE, MAIN, &g_danceSel[0], DancePicked},
    {RIG, "Helicopter", TOGGLE, MAIN, &g_danceSel[1], DancePicked},
    {RIG, "T-Pose", TOGGLE, MAIN, &g_danceSel[2], DancePicked},
    {RIG, "Windmill", TOGGLE, MAIN, &g_danceSel[3], DancePicked},
    {RIG, "Swim", TOGGLE, MAIN, &g_danceSel[4], DancePicked},
    {RIG, "Jumping Jacks", TOGGLE, MAIN, &g_danceSel[5], DancePicked},
    {RIG, "Spaz", TOGGLE, MAIN, &g_danceSel[6], DancePicked},
    {RIG, "Beyblade", TOGGLE, MAIN, &g_danceSel[7], DancePicked},
    {RIG, "Floss", TOGGLE, MAIN, &g_danceSel[8], DancePicked},
    {RIG, "Dab", TOGGLE, MAIN, &g_danceSel[9], DancePicked},
    {RIG, "Clap", TOGGLE, MAIN, &g_danceSel[10], DancePicked},
    {RIG, "Chicken Wings", TOGGLE, MAIN, &g_danceSel[11], DancePicked},
    {RIG, "Robot", TOGGLE, MAIN, &g_danceSel[12], DancePicked},
    {RIG, "Salute", TOGGLE, MAIN, &g_danceSel[13], DancePicked},
    {RIG, "Cheer", TOGGLE, MAIN, &g_danceSel[14], DancePicked},
    {RIG, "Hype", TOGGLE, MAIN, &g_danceSel[15], DancePicked},

    {ITEMS, "Exit Items", OPEN, MAIN},
    {ITEMS, "Spawn Item Gun", TOGGLE, MAIN, &f_itemGun},
    {ITEMS, "Duplicate Items (3x3)", ACTION, MAIN, nullptr, ArmDupe9},
    {ITEMS, "Duplicate Items (3x3x3)", ACTION, MAIN, nullptr, ArmDupe27},
    {ITEMS, "Duplicate Items (9x3)", ACTION, MAIN, nullptr, ArmDupe27Wide},
    {ITEMS, "Item Circle (27)", ACTION, MAIN, nullptr, SpawnItemCircle},
    {ITEMS, "Item Rain (27)", ACTION, MAIN, nullptr, SpawnItemRain},
    {ITEMS, "Launch Items Gun", TOGGLE, MAIN, &f_launchGun},
    {ITEMS, "Spawn On Hand", TOGGLE, MAIN, &f_spawnOnHand},

    {SETTINGS, "Exit Settings", OPEN, MAIN},
    {SETTINGS, "Change Pointer Position", ACTION, MAIN, nullptr, NextPointer},
    {SETTINGS, "Iron Man Speed", ACTION, MAIN, nullptr, NextIronSpeed},
    {SETTINGS, "Car Speed", ACTION, MAIN, nullptr, NextCarSpeed},
    {SETTINGS, "Menu Size", ACTION, MAIN, nullptr, NextMenuSize},
    {SETTINGS, "Right Hand Menu", TOGGLE, MAIN, &f_rightHand, PointerToHand},
    {SETTINGS, "Change Menu Theme", ACTION, MAIN, nullptr, NextTheme},
    {SETTINGS, "Rainbow Menu", TOGGLE, MAIN, &f_rainbow},
    {SETTINGS, "Show FPS", TOGGLE, MAIN, &f_fps},
    {SETTINGS, "Show Clock", TOGGLE, MAIN, &f_clock},
    {SETTINGS, "Notifications", TOGGLE, MAIN, &f_notify},
    {SETTINGS, "Turn Everything Off", ACTION, MAIN, nullptr, AllOff},

    {EFFECTS, "Exit Effects", OPEN, MAIN},
    {EFFECTS, "Rain", TOGGLE, MAIN, &f_rain},
    {EFFECTS, "Snow", TOGGLE, MAIN, &f_snow},

    {SOUND, "Exit Soundboard", OPEN, MAIN},
    {SOUND, "Scan Sounds Folder", ACTION, MAIN, nullptr, ScanSounds},
    {SOUND, "Previous Sound", ACTION, MAIN, nullptr, PreviousSound},
    {SOUND, "Next Sound", ACTION, MAIN, nullptr, NextSound},
    {SOUND, "Play Sound", ACTION, MAIN, nullptr, PlaySound},
    {SOUND, "Stop Sound", ACTION, MAIN, nullptr, StopSound},
    {SOUND, "Volume Up", ACTION, MAIN, nullptr, SoundVolumeUp},
    {SOUND, "Volume Down", ACTION, MAIN, nullptr, SoundVolumeDown},
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
    {"Fast Fly", "Makes Fly a lot faster."},
    {"Velocity Fly", "Hold A to shoot where your hand points."},
    {"Car", "Hold A to drive where you look, floating just above the ground."},
    {"Car Speed", "Slow, normal or fast."},
    {"Teleport Gun", "Hold right grip to aim, trigger to teleport."},
    {"Iron Man", "Hold X and A to fly with your hands."},
    {"Noclip", "Hold right trigger to go through walls."},
    {"Speed Boost", "Faster and higher jumps."},
    {"No Gravity", "Turns your gravity off."},
    {"Low Gravity", "Jump higher, fall slower."},
    {"Save Spot", "Remembers where you are."},
    {"Go To Spot", "Teleports you to your saved spot."},
    {"Super Speed", "Hold A to move quickly in the direction you face."},
    {"Super Fly", "Hold A to fly freely in the direction you look."},
    {"Rocket Boots", "Hold both triggers to thrust upward."},
    {"Dash", "Press X for a short forward burst."},
    {"Grapple Hook", "Hold left grip and right trigger to pull toward a surface."},
    {"Ground Pound", "Press B to drop quickly."},
    {"Mega Jump", "Press B for a powerful jump; cooldown prevents repeated firing."},
    {"Thumbstick Fly", "Left stick moves; right stick controls height."},
    {"Jump Power", "Cycle jump strength; select normal to restore the original values."},
    {"Fling Gun", "Press A to launch yourself in the direction of your right hand."},
    {"Floss", "Hold left trigger to floss."},
    {"Dab", "Hold left trigger to dab."},
    {"Clap", "Hold left trigger to clap."},
    {"Chicken Wings", "Hold left trigger to flap your arms."},
    {"Robot", "Hold left trigger for robotic arm movements."},
    {"Salute", "Hold left trigger to salute."},
    {"Cheer", "Hold left trigger to cheer."},
    {"Hype", "Hold left trigger for a hype dance."},
    {"Rain", "Local rain effect; only you see it."},
    {"Snow", "Local snow effect; only you see it."},
    {"Duplicate Items (9x3)", "Spawn a one-time 27-item grid, with a 3-second batch cooldown."},
    {"Item Circle (27)", "Spawn one ring of 27 copies around you."},
    {"Item Rain (27)", "Spawn one bounded batch of 27 copies above you."},
    {"Launch Items Gun", "Aim with right grip and press trigger to launch your held item."},
    {"Menu Size", "Cycle the menu between 50% and 150% size."},
    {"Soundboard", "Play your WAV/MP3 clips through your Photon voice-chat stream."},
    {"Scan Sounds Folder", "Find WAV files in the DARK MATTER sounds folder."},
    {"Previous Sound", "Select the previous WAV clip."},
    {"Next Sound", "Select the next WAV clip."},
    {"Play Sound", "Send the selected WAV/MP3 through your voice-chat stream when available."},
    {"Stop Sound", "Stop local sound playback."},
    {"Volume Up", "Raise local soundboard volume."},
    {"Volume Down", "Lower local soundboard volume."},
    {"Change Menu Theme", "VOID, NEBULA or ECLIPSE."},
    {"Rainbow Menu", "The menu cycles through colors."},
    {"Show FPS", "Frames per second in the title."},
    {"Show Clock", "The time in the title."},
    {"Rig Mods", "Pick a dance, then hold left trigger. Everyone sees it."},
    {"Items", "Spawn copies of the item in your left hand. Everyone sees them."},
    {"Spawn Item Gun", "Hold an item in your left hand. Right grip aims, trigger spawns it."},
    {"Duplicate Items (3x3)", "9 of the item in your left hand appear in front of you."},
    {"Duplicate Items (3x3x3)", "27 of the item in your left hand appear in front of you."},
    {"Spawn On Hand", "Each right trigger spawns the item from your left hand at your right hand."},
    {"Wave", "Waves one hand, the other on your hip. Hold left trigger."},
    {"Helicopter", "Both arms spin around you. Hold left trigger."},
    {"T-Pose", "The classic T-pose. Hold left trigger."},
    {"Windmill", "Arms spin like a windmill. Hold left trigger."},
    {"Swim", "Swims through the air. Hold left trigger."},
    {"Jumping Jacks", "Arms go up and down at your sides. Hold left trigger."},
    {"Spaz", "Hands shake all over the place. Hold left trigger."},
    {"Beyblade", "Hands spin fast around your waist. Hold left trigger."},
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
    {"VOID", Color(0.018f, 0.025f, 0.055f, 1), Color(0.055f, 0.075f, 0.13f, 1), Color(0.075f, 0.09f, 0.15f, 1), Color(0.28f, 0.13f, 0.48f, 1), Color(0.63f, 0.48f, 1.0f, 1)},
    {"NEBULA", Color(0.018f, 0.04f, 0.065f, 1), Color(0.035f, 0.11f, 0.14f, 1), Color(0.055f, 0.12f, 0.16f, 1), Color(0.055f, 0.32f, 0.37f, 1), Color(0.32f, 0.92f, 0.94f, 1)},
    {"ECLIPSE", Color(0.035f, 0.018f, 0.055f, 1), Color(0.11f, 0.035f, 0.14f, 1), Color(0.14f, 0.06f, 0.18f, 1), Color(0.43f, 0.09f, 0.34f, 1), Color(0.98f, 0.43f, 0.78f, 1)},
};
static constexpr int kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);
static int g_theme = 0;
static const Theme &T() { return kThemes[g_theme]; }

// ---- menu objects ----------------------------------------------------------------

// Layout in meters, matching ii's menu as it was ported to Yeeps. Everything is drawn
// on a "board" object: +x goes across, +y goes up, the player looks at it from -z.
static constexpr int kRows = 8;                                   // buttons per page
static const float kBoardW = 0.46f, kBoardH = 0.43f, kBtnW = 0.205f, kBtnH = 0.058f;
static const float kTitleY = 0.18f, kSideX = 0.20f, kSideW = 0.055f, kSideH = 0.055f;
static const float kSideY = 0.18f;
static float RowY(int r) { return 0.105f - (r / 2) * 0.074f; }
static float ColX(int r) { return (r % 2 == 0) ? -0.112f : 0.112f; }

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
static const float kMenuSizes[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f};
static int g_menuSizeIndex = 2;
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
    if (G.textSetFontStyle.IsValid()) G.textSetFontStyle[text](1);   // Bold, crisp dashboard typography
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
    Il2CppObject *go = Class("UnityEngine", "GameObject").CreateNewObjectParameters(CreateMonoString("DarkMatterText"));
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
    Il2CppObject *menu = NewObject("DarkMatter"), *board = NewObject("DarkMatterBoard");
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
        g_rowCube[r].set(Part(bt, T().button, Vector3(ColX(r), RowY(r), -0.005f), Vector3(kBtnW, kBtnH, 0.009f)));
    g_prev.set(Part(bt, T().button, Vector3(-kSideX, kSideY, 0), Vector3(kSideW, kSideH, 0.009f)));
    // Wide header with compact page controls: a new landscape console layout.
    g_titleBar.set(Part(bt, T().buttonOn, Vector3(0, kTitleY, -0.002f), Vector3(kBoardW - 0.11f, 0.045f, 0.009f)));
    g_next.set(Part(bt, T().button, Vector3(kSideX, kSideY, 0), Vector3(kSideW, kSideH, 0.009f)));

    if (Il2CppObject *canvas = Canvas(bt, Vector3(0, 0, -0.0105f), kBoardW + 0.04f, kBoardH + 0.1f)) {
        Il2CppObject *ct = Tr(canvas);
        Label(ct, "DARK MATTER", Vector3(0, kTitleY, 0), kBoardW - 0.11f, 0.045f, 26, &g_title);
        for (int r = 0; r < kRows; r++)
            g_rowLabel[r].set(Label(ct, "", Vector3(ColX(r), RowY(r), 0), kBtnW, kBtnH, 17, &g_rowText[r]));
        Label(ct, "<", Vector3(-kSideX, kSideY, -0.0005f), kSideW, 0.05f, 28, nullptr);
        Label(ct, ">", Vector3(kSideX, kSideY, -0.0005f), kSideW, 0.05f, 28, nullptr);
        Label(ct, "", Vector3(0, -kBoardH / 2 - 0.02f, 0), kBoardW + 0.1f, 0.03f, 15, &g_tipText);
        char ver[64];
        snprintf(ver, sizeof ver, "DARK MATTER  //  %s", ym::api()->gameVersion);
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

// The title is "Blaze" with a moving red-orange-yellow gradient, one color per letter
// (rich text). Rebuilt ten times a second, not every frame.
static void UpdateTitle() {
    if (g_time < g_nextTitle) return;
    g_nextTitle = g_time + 0.1;
    static const Color kFire[] = {Color(0.62f, 0.42f, 1.0f, 1), Color(0.30f, 0.86f, 1.0f, 1), Color(0.78f, 0.48f, 1.0f, 1), Color(0.36f, 0.54f, 1.0f, 1)};
    char buf[512];
    int n = 0;
    const char *word = "DARK MATTER";
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
    ym::log("dark-matter: %s", it->label);
}

// Which part of the menu the pointer touches: row 0..7, -1 = <, kRows = >, -2 = nothing.
static int Touching() {
    Il2CppObject *board = g_board.get(), *pointer = g_pointer.get();
    if (!Alive(board) || !Alive(pointer)) return -2;
    Vector3 p = G.trInverseTransformPoint[Tr(board)](G.trGetPosition[Tr(pointer)]());
    if (p.z < -0.035f || p.z > 0.015f) return -2;
    if (fabsf(fabsf(p.x) - kSideX) < kSideW / 2 && fabsf(p.y - kSideY) < kSideH / 2 && PageCount() > 1)
        return p.x < 0 ? -1 : kRows;
    for (int r = 0; r < kRows; r++)
        if (fabsf(p.x - ColX(r)) < kBtnW / 2 && fabsf(p.y - RowY(r)) < kBtnH / 2) return r;
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
        if (hit == -1 || hit == kRows) {
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
        float size = k * kMenuSizes[g_menuSizeIndex];
        G.trSetLocalScale[Tr(g_menu.get())](Vector3(size, size, size));
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

static void SaveSpot() {
    if (BodyPos(g_spot)) { g_haveSpot = true; Notify("Spot saved"); }
}
static void GoToSpot() {
    if (!g_haveSpot) { Notify("Save a spot first"); return; }
    TeleportPlayer(g_spot);
    Notify("Teleported to your spot");
}

static int g_jumpPowerLevel = 0;
static Il2CppObject *g_jumpPowerPlayer = nullptr;
static float g_jumpPowerOldMax = 0, g_jumpPowerOldMultiplier = 0;
static const float kJumpMax[] = {0, 11.0f, 14.0f, 18.0f};
static const float kJumpMult[] = {0, 1.8f, 2.4f, 3.0f};
static void NextJumpPower() {
    Il2CppObject *p = g_player.get();
    if (!p || !G.maxJumpSpeed.IsValid() || !G.jumpMultiplier.IsValid()) { Notify("Jump settings unavailable"); return; }
    if (f_speedBoost) { Notify("Turn off Speed Boost first"); return; }
    if (!g_jumpPowerLevel || g_jumpPowerPlayer != p) {
        g_jumpPowerOldMax = G.maxJumpSpeed[p]();
        g_jumpPowerOldMultiplier = G.jumpMultiplier[p]();
        g_jumpPowerPlayer = p;
    }
    g_jumpPowerLevel = (g_jumpPowerLevel + 1) % 4;
    if (!g_jumpPowerLevel) {
        G.maxJumpSpeed[p] = g_jumpPowerOldMax;
        G.jumpMultiplier[p] = g_jumpPowerOldMultiplier;
        g_jumpPowerPlayer = nullptr;
        Notify("Jump power: normal");
    } else {
        G.maxJumpSpeed[p] = kJumpMax[g_jumpPowerLevel];
        G.jumpMultiplier[p] = kJumpMult[g_jumpPowerLevel];
        Notify("Jump power: level %d", g_jumpPowerLevel);
    }
}
static const float kIronSpeeds[] = {15, 12, 8, 4};
static int g_ironSpeed = 0;
static void NextIronSpeed() { g_ironSpeed = (g_ironSpeed + 1) % 4; Notify("Iron Man speed: %d", (int)kIronSpeeds[g_ironSpeed]); }
static void NextTheme() { g_theme = (g_theme + 1) % kThemeCount; Notify("Theme: %s", T().name); }
static void NextMenuSize() {
    g_menuSizeIndex = (g_menuSizeIndex + 1) % 5;
    float size = kMenuSizes[g_menuSizeIndex];
    if (Alive(g_menu.get())) G.trSetLocalScale[Tr(g_menu.get())](Vector3(size, size, size));
    Notify("Menu size: %d%%", (int)(size * 100));
}
static void AllOff() {
    for (auto &it : g_items) if (it.kind == TOGGLE && it.flag != &f_notify) *it.flag = false;   // ticks undo each mod
    g_dance = -1;
    g_dupeArmed = 0;
    g_itemQueue.clear();
    PointerToHand();
    Notify("Everything off");
}

static void FlyTick(const Buttons &in, float dt) {
    Il2CppObject *head = g_head.get(), *t = PlayerTransform();
    if (!f_fly || !in.a || !head || !t) return;
    Vector3 pos = G.trGetPosition[t]() + G.trGetForward[head]() * ((f_fastFly ? 20.0f : 8.0f) * dt);
    G.trSetPosition[t](pos);
    StopMoving();
}

static void LowGravityTick(const Buttons &in, float dt) {
    Vector3 v;
    if (!f_lowGravity || f_noGravity || (f_fly && in.a) || !GetVel(v)) return;
    SetVel(v + Vector3(0, 9.81f * 0.7f * dt, 0));       // cancels 70% of gravity
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
        ym::log("dark-matter: hand layers 0x%x, platforms on layer %d", mask, layer);
    }
    if (layer >= 0 && G.goSetLayer.IsValid()) G.goSetLayer[p](layer);
    Il2CppObject *t = Tr(p);
    G.trSetPosition[t](G.trGetPosition[hand.get()]() - Vector3(0, 0.08f, 0));
    G.trSetLocalScale[t](Vector3(0.28f, 0.02f, 0.28f));
    platform.set(p);
}

static void VelocityFlyTick(const Buttons &in) {
    Il2CppObject *hand = g_rightHand.get();
    if (!f_velocityFly || !in.a || !hand) return;
    SetVel(G.trGetForward[hand]() * 20.0f);
}

static Vector3 Flat(Vector3 v);

// Car: like Fly, but along the ground. Holding A drives you the way you look, flat, and
// you hover a little above the ground (a raycast down finds it) so the bumps on blocks
// don't catch you. Off a ledge, with no ground close below, you fall normally.
static const float kCarSpeeds[] = {4.0f, 6.0f, 9.0f};
static const char *kCarNames[] = {"slow", "normal", "fast"};
static int g_carSpeed = 1;
static float g_carHeight = -1;               // your head's height above the ground, measured once
static void NextCarSpeed() { g_carSpeed = (g_carSpeed + 1) % 3; Notify("Car: %s", kCarNames[g_carSpeed]); }
static void CarTick(const Buttons &in) {
    Il2CppObject *head = g_head.get(), *player = g_player.get();
    Vector3 v;
    if (!f_car) { g_carHeight = -1; return; }
    if (!in.a || !head || !player || !GetVel(v) || !G.raycast.IsValid()) return;
    int mask = G.locomotionLayers.IsValid() ? G.locomotionLayers[player]() : -5;
    Vector3 hp = G.trGetPosition[head]();
    BNM::Structures::Unity::RaycastHit hit{};
    bool ground = G.raycast(hp, Vector3(0, -1, 0), &hit, 3.0f, mask ? mask : -5);
    Vector3 f = Flat(G.trGetForward[head]()) * kCarSpeeds[g_carSpeed];
    float vy = fminf(v.y, 1.0f);
    if (ground) {
        if (g_carHeight < 0) g_carHeight = hp.y - hit.point.y;      // your standing height
        float target = hit.point.y + g_carHeight + 0.15f;            // float 15 cm higher
        vy = (target - hp.y) * 8.0f;
        if (G.plNoGravityFor.IsValid()) G.plNoGravityFor[player](0.1f);
    }
    SetVel(Vector3(f.x, vy, f.z));
}

static void IronManTick(const Buttons &in, float dt) {
    Vector3 v;
    if (!f_ironMan || !G.trGetRight.IsValid() || (!in.x && !in.a) || !GetVel(v)) return;
    float speed = kIronSpeeds[g_ironSpeed] * dt;
    if (in.x && g_leftHand.get()) v = v + G.trGetRight[g_leftHand.get()]() * -speed;
    if (in.a && g_rightHand.get()) v = v + G.trGetRight[g_rightHand.get()]() * speed;
    SetVel(v);
}

// Speed Boost and No Gravity change your own player; turning them off puts the old
// values back. Checked every frame so they also come back after a room change.
static float g_oldJumpSpeed = 0, g_oldJumpMultiplier = 0;
static Il2CppObject *g_boosted = nullptr;      // which player has the boost (compared only)
static bool g_gravityOff = false;

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

static void ApplyGravity() {
    // The game's own "no gravity for a while" (what its levitation items use), renewed
    // every frame while the mod is on; it wears off by itself once you turn it off.
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
    if (want) ym::log("dark-matter: noclip on (layers 0x%x, body layer %d, %d colliders off)", mask, g_bodyLayer, g_noclipOffN);
}

// Teleport Gun: hold right grip to aim (a line and a ball show where you land), press
// the right trigger to go there.
static Ref g_gunLine, g_gunLineGo, g_gunBall;
static bool g_gunTrigger = false;
static void TeleportGunTick(const Buttons &in) {
    bool aiming = (f_tpGun || f_itemGun || f_launchGun) && in.rgrip;
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
        Il2CppObject *lineGo = Class("UnityEngine", "GameObject").CreateNewObjectParameters(CreateMonoString("DarkMatterAimLine"));
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
        if (f_launchGun || f_itemGun) {                   // item spawn at the aim point
            if (g_time >= g_nextItem && LeftHandItem()) {
                g_nextItem = g_time + 0.5;
                Vector3 launch = f_launchGun ? dir * 9.0f : Vector3(0, 0, 0);
                g_itemQueue.push_back({end + Vector3(0, 0.3f, 0), launch});
            }
        } else TeleportPlayer(end + Vector3(0, 1.0f, 0));
    }
    g_gunTrigger = in.rtrig;
}

// ---- vector helpers ----------------------------------------------------------------

static Vector3 Cross(Vector3 a, Vector3 b) { return Vector3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static Vector3 Flat(Vector3 v) {                      // v without its up/down part, length 1
    v.y = 0;
    float l = sqrtf(v.x * v.x + v.z * v.z);
    return l > 1e-4f ? v * (1.0f / l) : Vector3(0, 0, 1);
}

// ---- rig mods: dances -----------------------------------------------------------
// While you hold the left trigger, your hands are moved through a dance instead of
// following your controllers. The game sends your hand positions to the room as
// usual, so other players see you dance. Nothing extra is sent.

// The dance buttons work like radio buttons: picking one turns the others off,
// pressing the picked one again turns dancing off.
static void DancePicked() {
    int picked = -1;
    for (int i = 0; i < kDances; i++) if (g_danceSel[i] && i != g_dance) picked = i;
    for (int i = 0; i < kDances; i++) g_danceSel[i] = (i == picked);
    g_dance = picked;
    if (picked >= 0) Notify("Hold left trigger to dance");
}
static float Rand01() { static uint32_t x = 2463534242u; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return (x & 0xffff) / 65535.0f; }

// The controller trackers would put your hands back every frame, so they are paused
// while you dance (on the hand and the two objects above it) and turned back on after.
static Ref g_paused[12];
static int g_pausedN = 0;
static bool g_dancing = false;
static void PauseTrackers(bool pause) {
    if (!pause) {
        for (int i = 0; i < g_pausedN; i++) {
            if (Alive(g_paused[i].get())) G.behaviourSetEnabled[g_paused[i].get()](true);
            g_paused[i].clear();
        }
        g_pausedN = 0;
        return;
    }
    if (!G.behaviourSetEnabled.IsValid() || !G.trGetParent.IsValid()) return;
    for (Il2CppObject *t : {g_leftHand.get(), g_rightHand.get()})
        for (int up = 0; up < 3 && t; up++, t = G.trGetParent[t]())
            for (BNM::MonoType *type : G.trackerTypes) {
                Il2CppObject *go = G.compGameObject[t]();
                Il2CppObject *c = go ? G.goGetComponent[go]((Il2CppObject *)type) : nullptr;
                if (!c || !G.behaviourGetEnabled[c]() || g_pausedN >= 12) continue;
                G.behaviourSetEnabled[c](false);
                g_paused[g_pausedN++].set(c);
            }
}

static void DanceTick(const Buttons &in, float dt) {
    Il2CppObject *head = g_head.get(), *lh = g_leftHand.get(), *rh = g_rightHand.get();
    bool want = g_dance >= 0 && in.ltrig && head && lh && rh;
    if (want != g_dancing) {
        PauseTrackers(want);
        g_dancing = want;
        if (want) ym::log("dark-matter: dancing, paused %d trackers", g_pausedN);
    }
    if (!want) return;
    Vector3 hp = G.trGetPosition[head](), f = Flat(G.trGetForward[head]()), up(0, 1, 0);
    Vector3 r = Cross(up, f);                            // your right
    Vector3 shoulder = hp + up * -0.25f;
    float t = (float)g_time, s = sinf(t * 6), c = cosf(t * 6);
    Vector3 L, R;                                        // hand positions
    switch (g_dance) {
    case 0:   // Wave: right hand up and waving, left hand on the hip
        R = shoulder + r * (0.35f + 0.12f * s) + up * 0.45f + f * 0.1f;
        L = shoulder + r * -0.25f + up * -0.25f + f * 0.05f;
        break;
    case 1:   // Helicopter: both hands spin around you
        R = shoulder + r * (0.6f * cosf(t * 8)) + f * (0.6f * sinf(t * 8)) + up * 0.15f;
        L = shoulder - r * (0.6f * cosf(t * 8)) - f * (0.6f * sinf(t * 8)) + up * 0.15f;
        break;
    case 2:   // T-Pose
        R = shoulder + r * 0.7f + up * 0.1f;
        L = shoulder - r * 0.7f + up * 0.1f;
        break;
    case 3:   // Windmill: big circles at your sides
        R = shoulder + r * 0.4f + up * (0.5f * sinf(-t * 5)) + f * (0.5f * cosf(-t * 5));
        L = shoulder - r * 0.4f - up * (0.5f * sinf(-t * 5)) - f * (0.5f * cosf(-t * 5));
        break;
    case 4: {  // Swim: breaststroke
        float p = t * 3;
        R = shoulder + f * (0.3f + 0.25f * cosf(p)) + r * (0.08f + 0.35f * fabsf(sinf(p)));
        L = shoulder + f * (0.3f + 0.25f * cosf(p)) - r * (0.08f + 0.35f * fabsf(sinf(p)));
        break;
    }
    case 5: {  // Jumping jacks: arms from down by your sides to up over your head
        float a = (sinf(t * 6) * 0.5f + 0.5f) * 3.1f - 1.55f;
        R = shoulder + r * (0.6f * cosf(a)) + up * (0.6f * sinf(a));
        L = shoulder - r * (0.6f * cosf(a)) + up * (0.6f * sinf(a));
        break;
    }
    case 6:   // Spaz: hands in front, shaking everywhere
        R = shoulder + f * 0.35f + r * 0.2f + Vector3(Rand01() - 0.5f, Rand01() - 0.5f, Rand01() - 0.5f) * 0.3f;
        L = shoulder + f * 0.35f - r * 0.2f + Vector3(Rand01() - 0.5f, Rand01() - 0.5f, Rand01() - 0.5f) * 0.3f;
        break;
    case 7:  // Beyblade: hands spin fast around your waist
        R = shoulder - up * 0.3f + r * (0.5f * cosf(t * 20)) + f * (0.5f * sinf(t * 20));
        L = shoulder - up * 0.3f - r * (0.5f * cosf(t * 20)) - f * (0.5f * sinf(t * 20));
        break;
    case 8: { float p = t * 8.0f; R = shoulder + r * (0.25f + 0.35f * sinf(p)) - up * 0.2f - f * 0.15f; L = shoulder - r * (0.25f + 0.35f * sinf(p + 3.14159f)) - up * 0.2f - f * 0.15f; break; }
    case 9: R = shoulder + r * 0.48f + up * 0.48f + f * 0.1f; L = shoulder - r * 0.2f + up * 0.18f + f * 0.42f; break;
    case 10: { float p = 0.16f + 0.12f * fabsf(sinf(t * 7.0f)); R = shoulder + f * 0.38f - r * p + up * 0.05f; L = shoulder + f * 0.38f + r * p + up * 0.05f; break; }
    case 11: { float p = 0.22f + 0.12f * sinf(t * 9.0f); R = shoulder + r * 0.34f - up * p + f * 0.1f; L = shoulder - r * 0.34f - up * p + f * 0.1f; break; }
    case 12: { float p = sinf(t * 3.0f) > 0 ? 0.42f : -0.12f; R = shoulder + r * 0.42f + up * p + f * 0.12f; L = shoulder - r * 0.42f - up * p + f * 0.12f; break; }
    case 13: R = shoulder + r * 0.1f + up * 0.42f + f * 0.32f; L = shoulder - r * 0.42f - up * 0.12f; break;
    case 14: { float p = 0.42f + 0.08f * sinf(t * 8.0f); R = shoulder + r * 0.38f + up * p; L = shoulder - r * 0.38f + up * p; break; }
    default: { float p = sinf(t * 8.0f); R = shoulder + r * 0.28f + up * (0.42f + 0.16f * p); L = shoulder - r * 0.28f + up * (0.42f - 0.16f * p); break; }
    }
    // Keep each hand within arm's reach of its shoulder, then glide there instead of
    // jumping, so the arms move smoothly for you and for everyone watching.
    auto reach = [&](Vector3 hand, Vector3 sh) {
        Vector3 d = hand - sh;
        float len = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
        return len > 0.7f ? sh + d * (0.7f / len) : hand;
    };
    R = reach(R, shoulder + r * 0.18f);
    L = reach(L, shoulder - r * 0.18f);
    float k = g_dance == 6 ? 1.0f : fminf(1.0f, dt * 14.0f);     // Spaz stays twitchy
    G.trSetPosition[lh](G.trGetPosition[lh]() + (L - G.trGetPosition[lh]()) * k);
    G.trSetPosition[rh](G.trGetPosition[rh]() + (R - G.trGetPosition[rh]()) * k);
    Quaternion look = Quaternion::LookRotation(f, up);
    G.trSetRotation[lh](look);
    G.trSetRotation[rh](look);
}

// ---- items (everyone sees these) ------------------------------------------------
// Items are made with the game's own item message (RpcCreateItem on the room's
// NetworkMessenger), the same one the game sends when an item appears. Owner-approved
// 2026-10-31. Every way of spawning has a cooldown, and big batches are sent a few per
// frame, so the room can't be flooded.


static void Put(Il2CppObject *arr, size_t i, void *v) { ((void **)((uint8_t *)arr + 0x20))[i] = v; }

// Builds RpcCreateItem's arguments from its own parameter list, in the order the game
// uses: item id, item key, spawn kind 1, no owner, position, rotation, velocity, spin,
// properties, 1, 1.
static bool SendItem(const char *key, Vector3 pos, Quaternion rot, Vector3 vel, char *idOut = nullptr) {
    auto *mi = G.rpcCreateItem;
    Il2CppObject *nm = G.netLocal.IsValid() ? G.netLocal() : nullptr;
    if (!mi || !nm || !G.pvRpc.IsValid() || !G.arrayNew || !G.valueBox || !G.tPhotonView) return false;
    Il2CppObject *go = G.compGameObject[nm]();
    Il2CppObject *pv = go ? G.goGetComponent[go]((Il2CppObject *)G.tPhotonView) : nullptr;
    if (!pv) return false;
    // The last parameter (PhotonMessageInfo) is filled in by Photon on the receiving
    // side; it is never sent.
    int count = mi->parameters_count;
    if (count > 0 && TypeName(mi->parameters[count - 1]) == "Photon.Pun.PhotonMessageInfo") count--;
    auto *args = (Il2CppObject *)G.arrayNew(G.objectClass, count);
    if (!args) return false;
    char id[8];
    static const char kChars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < 4; i++) id[i] = kChars[(int)(Rand01() * 35.99f)];
    id[4] = 0;
    if (idOut) memcpy(idOut, id, 5);
    int strings = 0, vectors = 0, others = 0;
    for (int i = 0; i < count; i++) {
        std::string tn = TypeName(mi->parameters[i]);
        Class pc(mi->parameters[i]);
        auto *k = pc.IsValid() ? pc.GetClass() : nullptr;
        void *v = nullptr;
        if (tn == "System.String") {
            if (strings == 0) v = CreateMonoString(id);
            else if (strings == 1) v = CreateMonoString(key);
            strings++;                                   // later strings stay null
        } else if (tn == "UnityEngine.Vector3") {
            Vector3 vec = vectors == 0 ? pos : vectors == 1 ? vel : Vector3(0, 0, 0);
            vectors++;
            v = G.valueBox(k, &vec);
        } else if (tn == "UnityEngine.Quaternion") {
            v = G.valueBox(k, &rot);
        } else if (tn == "System.String[]") {
            v = G.arrayNew(G.stringClass, 0);
        } else if (k && k->byval_arg.valuetype) {
            uint8_t raw[64] = {};
            raw[0] = 1;                                 // spawn kind 1 / the two bytes = 1
            // Photon can't send the game's enum types, only plain numbers (like the kit's
            // Int32 1), so an enum goes as its underlying number type.
            auto *boxAs = k->enumtype && k->element_class ? k->element_class : k;
            v = G.valueBox(boxAs, raw);
            others++;
        }
        Put(args, i, v);
    }
    static bool loggedTypes = false;
    if (!loggedTypes) {                                   // once: what the game expects, for fixing
        loggedTypes = true;
        std::string sig;
        for (int i = 0; i < mi->parameters_count; i++) {
            Class pc(mi->parameters[i]);
            auto *k = pc.IsValid() ? pc.GetClass() : nullptr;
            std::string tn = TypeName(mi->parameters[i]);
            if (tn.size() > 30) tn = k && k->byval_arg.valuetype ? (k->enumtype ? "enum" : "struct" + std::to_string(k->instance_size - 16)) : "class";
            sig += (i ? "," : "") + tn;
        }
        ym::log("dark-matter: item message takes %s", sig.c_str());
    }
    try {
        G.pvRpc[pv](CreateMonoString("RpcCreateItem"), 0, args);      // 0 = everyone in the room
    } catch (BNM::IL2CPP::Il2CppExceptionWrapper &w) {   // the game said no: log why, don't crash the menu
        auto *ex = w.ex;
        auto *msg = ex ? (BNM::Structures::Mono::String *)ex->message : nullptr;
        auto *cls = ex ? (BNM::Structures::Mono::String *)ex->className : nullptr;
        ym::log("dark-matter: item message error %s: %s", cls ? cls->str().c_str() : "?", msg ? msg->str().c_str() : "?");
        return false;
    }
    return true;
}

static void QueueItems(const std::vector<PendingItem> &list) {
    if (!g_itemKey[0]) return;
    if (g_time < g_nextBatch) { Notify("Wait a moment"); return; }
    g_nextBatch = g_time + 3.0;
    for (auto &p : list) if (g_itemQueue.size() < 32) g_itemQueue.push_back(p);
}

static void ItemQueueTick() {
    for (int sent = 0; sent < 2 && !g_itemQueue.empty(); sent++) {   // 2 a frame at most
        PendingItem p = g_itemQueue.front();
        g_itemQueue.erase(g_itemQueue.begin());
        bool ok = SendItem(g_itemKey, p.pos, Quaternion(0, 0, 0, 1), p.vel);
        static int logged = 0;
        if (logged++ < 3) ym::log("dark-matter: spawn %s ok=%d", g_itemKey, ok ? 1 : 0);
        if (!ok) { g_itemQueue.clear(); Notify("Item spawning isn't ready in this room"); }
    }
}

// The grid in front of you: 3 across, 3 deep, and 3 high for 3x3x3.
static void SpawnGrid(int count) {
    Il2CppObject *head = g_head.get();
    if (!head) return;
    Vector3 f = Flat(G.trGetForward[head]()), r = Cross(Vector3(0, 1, 0), f);
    Vector3 base = G.trGetPosition[head]() + f * 1.2f + Vector3(0, -0.4f, 0);
    std::vector<PendingItem> list;
    for (int y = 0; y < (count > 9 ? 3 : 1); y++)
        for (int z = 0; z < 3; z++)
            for (int x = 0; x < 3; x++)
                list.push_back({base + r * ((x - 1) * 0.45f) + f * (z * 0.45f) + Vector3(0, y * 0.45f, 0), Vector3(0, 0, 0)});
    QueueItems(list);
}
// The item you're holding in your LEFT hand right now (owner 2026-10-31: items spawn as
// whatever is in your left hand when you press). One scene search per press.
static bool LeftHandItem() {
    Il2CppObject *hand = g_leftHand.get();
    if (!hand || !G.tItem || !G.itemKey.IsValid() || !(G.findScene.IsValid() || G.findSceneByType.IsValid())) return false;
    auto *arr = (uint8_t *)(G.findScene.IsValid() ? G.findScene((Il2CppObject *)G.tItem)
                                                  : G.findSceneByType((Il2CppObject *)G.tItem, 0));
    size_t n = arr ? *(uintptr_t *)(arr + 0x18) : 0;
    Vector3 hp = G.trGetPosition[hand]();
    Il2CppObject *best = nullptr;
    float bestD = 0.4f * 0.4f;
    for (size_t i = 0; i < n; i++) {
        Il2CppObject *it = ((Il2CppObject **)(arr + 0x20))[i];
        if (!Alive(it) || (G.itemHeld.IsValid() && !G.itemHeld[it]())) continue;
        Vector3 d = G.trGetPosition[G.compTransform[it]()]() - hp;
        float dd = d.x * d.x + d.y * d.y + d.z * d.z;
        if (dd < bestD) { bestD = dd; best = it; }
    }
    auto *key = best ? G.itemKey[best]() : nullptr;
    if (!key) { Notify("Hold an item in your left hand"); return false; }
    snprintf(g_itemKey, sizeof g_itemKey, "%s", key->str().c_str());
    return true;
}
static void ArmDupe9() { if (LeftHandItem()) SpawnGrid(9); }
static void ArmDupe27() { if (LeftHandItem()) SpawnGrid(27); }
static void ArmDupe27Wide() {
    if (!LeftHandItem()) return;
    Il2CppObject *head = g_head.get(); if (!head) return;
    Vector3 f = Flat(G.trGetForward[head]()), r = Cross(Vector3(0, 1, 0), f);
    Vector3 base = G.trGetPosition[head]() + f * 1.4f + Vector3(0, -0.35f, 0);
    std::vector<PendingItem> list;
    for (int z = 0; z < 3; ++z)
        for (int x = 0; x < 9; ++x)
            list.push_back({base + r * ((x - 4) * 0.32f) + f * (z * 0.42f), Vector3(0, 0, 0)});
    QueueItems(list);
}
static void SpawnItemCircle() {
    if (!LeftHandItem()) return;
    Il2CppObject *head = g_head.get(); if (!head) return;
    Vector3 hp = G.trGetPosition[head];
    std::vector<PendingItem> list;
    for (int i = 0; i < 27; ++i) {
        float a = (float)i * 6.2831853f / 27.0f;
        list.push_back({hp + Vector3(cosf(a) * 2.0f, 0.25f + (i % 3) * 0.35f, sinf(a) * 2.0f), Vector3(0, 0, 0)});
    }
    QueueItems(list);
}
static void SpawnItemRain() {
    if (!LeftHandItem()) return;
    Il2CppObject *head = g_head.get(); if (!head) return;
    Vector3 hp = G.trGetPosition[head];
    std::vector<PendingItem> list;
    for (int i = 0; i < 27; ++i) {
        float x = (Rand01() - 0.5f) * 5.0f, z = (Rand01() - 0.5f) * 5.0f;
        list.push_back({hp + Vector3(x, 3.0f + (i % 3) * 0.4f, z), Vector3(0, -2.5f, 0)});
    }
    QueueItems(list);
}

// Spawn On Hand: each right-trigger press (grip open) spawns the item at your hand.
static bool g_onHandTrigger = false;
static void SpawnOnHandTick(const Buttons &in) {
    bool press = in.rtrig && !g_onHandTrigger;
    g_onHandTrigger = in.rtrig;
    Il2CppObject *hand = g_rightHand.get();
    if (!f_spawnOnHand || !press || in.rgrip || g_menuShown || !hand || g_time < g_nextItem) return;
    if (!LeftHandItem()) return;
    g_nextItem = g_time + 0.3;
    g_itemQueue.push_back({G.trGetPosition[hand]() + G.trGetForward[hand]() * 0.1f, Vector3(0, 0, 0)});
}

// ---- local weather ------------------------------------------------------------
// Bounded local-only particles: no RPCs and no per-frame object allocation.
static Ref g_weather[20];
static int g_weatherMode = 0;
static void WeatherTick(float dt) {
    int mode = f_rain ? 1 : (f_snow ? 2 : 0);
    if (mode != g_weatherMode) { for (auto &p : g_weather) Kill(p); g_weatherMode = mode; }
    if (!mode) return;
    Il2CppObject *head = g_head.get(); if (!head) return;
    Vector3 hp = G.trGetPosition[head]();
    Color color = mode == 1 ? Color(0.38f, 0.72f, 1.0f, 1) : Color(0.88f, 0.94f, 1.0f, 1);
    for (int i = 0; i < 20; i++) {
        if (!Alive(g_weather[i].get())) {
            Il2CppObject *p = Prim(mode == 1 ? 3 : 0, color, false); if (!p) continue;
            G.trSetLocalScale[Tr(p)](mode == 1 ? Vector3(0.012f, 0.18f, 0.012f) : Vector3(0.045f, 0.045f, 0.045f));
            g_weather[i].set(p);
            G.trSetPosition[Tr(p)](hp + Vector3((Rand01() - 0.5f) * 5.0f, 1.0f + Rand01() * 3.0f, (Rand01() - 0.5f) * 5.0f));
        } else {
            Il2CppObject *p = g_weather[i].get(); Vector3 pos = G.trGetPosition[Tr(p)]();
            pos.y -= (mode == 1 ? 10.0f : 1.6f) * dt;
            if (pos.y < hp.y - 1.0f) pos = hp + Vector3((Rand01() - 0.5f) * 5.0f, 2.0f + Rand01() * 2.5f, (Rand01() - 0.5f) * 5.0f);
            G.trSetPosition[Tr(p)](pos);
        }
    }
}

// ---- callbacks ---------------------------------------------------------------

static void OnRoomChange() {
    // The old room's objects are gone (or about to be): forget them, build again.
    for (int r = 0; r < kRows; r++) { g_rowCube[r].clear(); g_rowLabel[r].clear(); g_rowText[r].clear(); }
    for (Ref *r : {&g_board, &g_bg, &g_outline, &g_title, &g_titleBar, &g_prev, &g_next, &g_notifyText, &g_tipText}) r->clear();
    for (Ref *r : {&g_menu, &g_pointer, &g_notifyGo, &g_leftPlatform, &g_rightPlatform, &g_gunBall, &g_gunLineGo}) Kill(*r);
    for (auto &p : g_weather) Kill(p);
    g_weatherMode = 0;
    PauseTrackers(false);
    g_dancing = false;
    g_gunLine.clear();
    if (g_noclipOn && g_player.get()) {           // leaving the room mid-noclip: put things back
        Buttons none;                             // trigger "released"
        NoclipTick(none);
    }
    g_boosted = nullptr;
    g_gravityOff = false;
    if (g_jumpPowerLevel && g_jumpPowerPlayer == g_player.get() && G.maxJumpSpeed.IsValid() && G.jumpMultiplier.IsValid()) {
        G.maxJumpSpeed[g_jumpPowerPlayer] = g_jumpPowerOldMax;
        G.jumpMultiplier[g_jumpPowerPlayer] = g_jumpPowerOldMultiplier;
    }
    g_jumpPowerLevel = 0; g_jumpPowerPlayer = nullptr;
    for (Ref *r : {&g_player, &g_rigidbody, &g_leftHand, &g_rightHand, &g_head}) r->clear();
    g_built = false; g_menuShown = false; g_touching = -2; g_haveSpot = false;
}

static bool g_dashWasDown = false, g_poundWasDown = false, g_flingWasDown = false;
static double g_nextDash = 0, g_nextMegaJump = 0;
static void ThumbstickFlyTick() {
    if (!f_thumbstickFly || !G.tryGetVector2.IsValid() || !G.deviceAtNode.IsValid()) return;
    XRDevice left = G.deviceAtNode(4), right = G.deviceAtNode(5);
    BNM::Structures::Unity::Vector2 la(0, 0), ra(0, 0);
    if (left.initialized) G.tryGetVector2(left.id, (BNM::Structures::Mono::String *)G.usageAxis.get(), &la);
    if (right.initialized) G.tryGetVector2(right.id, (BNM::Structures::Mono::String *)G.usageAxis.get(), &ra);
    Il2CppObject *head = g_head.get(), *player = g_player.get();
    if (!head) return;
    Vector3 f = Flat(G.trGetForward[head]()), r = Cross(Vector3(0, 1, 0), f);
    SetVel(f * (la.y * 10.0f) + r * (la.x * 10.0f) + Vector3(0, ra.y * 10.0f, 0));
    if (player && G.plNoGravityFor.IsValid()) G.plNoGravityFor[player](0.1f);
}
static void MovementExtrasTick(const Buttons &in) {
    Il2CppObject *head = g_head.get(), *hand = g_rightHand.get(); if (!head) return;
    Vector3 v;
    if (f_superSpeed && in.a && GetVel(v)) { Vector3 f = Flat(G.trGetForward[head]()); SetVel(Vector3(f.x * 15.0f, v.y, f.z * 15.0f)); }
    if (f_superFly && in.a) SetVel(G.trGetForward[head]() * 22.0f);
    if (f_rocketBoots && in.ltrig && in.rtrig && GetVel(v)) SetVel(Vector3(v.x, fmaxf(v.y, 14.0f), v.z));
    bool dashPress = in.x && !g_dashWasDown, poundPress = in.b && !g_poundWasDown;
    bool flingPress = in.a && !g_flingWasDown;
    g_dashWasDown = in.x; g_poundWasDown = in.b; g_flingWasDown = in.a;
    if (f_dash && dashPress && g_time >= g_nextDash) { Vector3 f = Flat(G.trGetForward[head]()); SetVel(f * 18.0f + Vector3(0, 3.0f, 0)); g_nextDash = g_time + 0.65; }
    if (f_megaJump && poundPress && !g_menuShown && g_time >= g_nextMegaJump && GetVel(v)) {
        SetVel(Vector3(v.x, 19.0f, v.z)); g_nextMegaJump = g_time + 0.8;
    } else if (f_groundPound && poundPress && !g_menuShown && GetVel(v)) SetVel(Vector3(v.x, -18.0f, v.z));
    if (f_flingGun && flingPress && !g_menuShown && hand) SetVel(G.trGetForward[hand]() * 22.0f);
    if (f_grapple && in.lgrip && in.rtrig && hand && G.raycast.IsValid()) {
        Vector3 start = G.trGetPosition[hand](), dir = G.trGetForward[hand]();
        BNM::Structures::Unity::RaycastHit hit{};
        if (G.raycast(start, dir, &hit, 40.0f, -5)) { Vector3 d = hit.point - start; float len = sqrtf(d.x*d.x + d.y*d.y + d.z*d.z); if (len > 0.2f) SetVel(d * (16.0f / len)); }
    }
}


// ---- multiplayer soundboard -------------------------------------------------
// Uses Photon Voice's Recorder audio-clip input. The clip is sent through the
// local player's existing voice stream; it is not a custom room RPC.
struct WavInfo { int channels = 0, sampleRate = 0; size_t dataOffset = 0, dataBytes = 0; };
static uint16_t Read16(const std::vector<uint8_t> &b, size_t p) {
    return (uint16_t)b[p] | ((uint16_t)b[p + 1] << 8);
}
static uint32_t Read32(const std::vector<uint8_t> &b, size_t p) {
    return (uint32_t)b[p] | ((uint32_t)b[p + 1] << 8) | ((uint32_t)b[p + 2] << 16) | ((uint32_t)b[p + 3] << 24);
}
static bool ParseWav(const std::vector<uint8_t> &b, WavInfo &w) {
    if (b.size() < 44 || memcmp(b.data(), "RIFF", 4) || memcmp(b.data() + 8, "WAVE", 4)) return false;
    bool fmt = false, data = false;
    for (size_t p = 12; p + 8 <= b.size();) {
        uint32_t n = Read32(b, p + 4);
        size_t body = p + 8;
        if (body + n > b.size()) return false;
        if (!memcmp(b.data() + p, "fmt ", 4)) {
            if (n < 16 || Read16(b, body) != 1 || Read16(b, body + 14) != 16) return false;
            w.channels = Read16(b, body + 2);
            w.sampleRate = (int)Read32(b, body + 4);
            fmt = true;
        } else if (!memcmp(b.data() + p, "data", 4)) {
            w.dataOffset = body; w.dataBytes = n; data = true;
        }
        p = body + n + (n & 1);
    }
    return fmt && data && (w.channels == 1 || w.channels == 2) &&
           w.sampleRate >= 8000 && w.sampleRate <= 48000 && w.dataBytes >= 2;
}
static bool DecodeMp3(const std::string &path, std::vector<float> &pcm, int &channels, int &sampleRate) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    struct stat st{};
    if (fstat(fd, &st) != 0 || st.st_size <= 0) { close(fd); return false; }
    AMediaExtractor *ex = AMediaExtractor_new();
    AMediaCodec *codec = nullptr;
    AMediaFormat *trackFormat = nullptr;
    bool ok = ex && AMediaExtractor_setDataSourceFd(ex, fd, 0, st.st_size) == AMEDIA_OK;
    close(fd);
    ssize_t track = -1;
    if (ok) {
        size_t tracks = AMediaExtractor_getTrackCount(ex);
        for (size_t i = 0; i < tracks; ++i) {
            AMediaFormat *fmt = AMediaExtractor_getTrackFormat(ex, i);
            const char *mime = nullptr;
            if (fmt && AMediaFormat_getString(fmt, AMEDIAFORMAT_KEY_MIME, &mime) && mime &&
                strncmp(mime, "audio/", 6) == 0) { track = (ssize_t)i; trackFormat = fmt; break; }
            if (fmt) AMediaFormat_delete(fmt);
        }
        ok = track >= 0 && trackFormat;
    }
    const char *mime = nullptr;
    if (ok) ok = AMediaFormat_getString(trackFormat, AMEDIAFORMAT_KEY_MIME, &mime) && mime;
    if (ok) { codec = AMediaCodec_createDecoderByType(mime); ok = codec != nullptr; }
    if (ok) ok = AMediaExtractor_selectTrack(ex, (size_t)track) == AMEDIA_OK;
    if (ok) ok = AMediaCodec_configure(codec, trackFormat, nullptr, nullptr, 0) == AMEDIA_OK;
    if (ok) ok = AMediaCodec_start(codec) == AMEDIA_OK;
    bool inputDone = false, outputDone = false;
    int32_t outChannels = 0, outRate = 0;
    size_t sampleCap = 48000u * 2u * 30u;
    while (ok && !outputDone && pcm.size() < sampleCap) {
        if (!inputDone) {
            ssize_t inIndex = AMediaCodec_dequeueInputBuffer(codec, 1000);
            if (inIndex >= 0) {
                size_t cap = 0;
                uint8_t *in = AMediaCodec_getInputBuffer(codec, (size_t)inIndex, &cap);
                ssize_t n = in ? AMediaExtractor_readSampleData(ex, in, cap) : -1;
                if (n < 0) {
                    AMediaCodec_queueInputBuffer(codec, (size_t)inIndex, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                    inputDone = true;
                } else {
                    int64_t pts = AMediaExtractor_getSampleTime(ex);
                    AMediaCodec_queueInputBuffer(codec, (size_t)inIndex, 0, (size_t)n, pts < 0 ? 0 : (uint64_t)pts, 0);
                    AMediaExtractor_advance(ex);
                }
            }
        }
        AMediaCodecBufferInfo info{};
        ssize_t outIndex = AMediaCodec_dequeueOutputBuffer(codec, &info, 1000);
        if (outIndex == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            AMediaFormat *fmt = AMediaCodec_getOutputFormat(codec);
            if (fmt) {
                AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &outChannels);
                AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_SAMPLE_RATE, &outRate);
                if (outChannels > 0 && outRate > 0) sampleCap = (size_t)outChannels * (size_t)outRate * 30u;
                AMediaFormat_delete(fmt);
            }
        } else if (outIndex >= 0) {
            size_t cap = 0;
            uint8_t *out = AMediaCodec_getOutputBuffer(codec, (size_t)outIndex, &cap);
            if (out && info.size > 0 && info.offset + (size_t)info.size <= cap) {
                const int16_t *samples = (const int16_t *)(out + info.offset);
                size_t count = (size_t)info.size / sizeof(int16_t);
                for (size_t i = 0; i < count && pcm.size() < sampleCap; ++i)
                    pcm.push_back((float)samples[i] / 32768.0f);
            }
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) outputDone = true;
            AMediaCodec_releaseOutputBuffer(codec, (size_t)outIndex, false);
        }
    }
    if (pcm.empty() || !outChannels || !outRate || outChannels > 2 || outRate < 8000 || outRate > 48000)
        ok = false;
    channels = outChannels; sampleRate = outRate;
    if (codec) { AMediaCodec_stop(codec); AMediaCodec_delete(codec); }
    if (trackFormat) AMediaFormat_delete(trackFormat);
    if (ex) AMediaExtractor_delete(ex);
    return ok;
}
static bool LoadSound(const std::string &path, std::vector<float> &pcm, int &channels, int &sampleRate) {
    std::string ext = path.substr(path.find_last_of('.') == std::string::npos ? path.size() : path.find_last_of('.'));
    for (char &c : ext) c = (char)tolower((unsigned char)c);
    if (ext == ".mp3") return DecodeMp3(path, pcm, channels, sampleRate);
    std::ifstream in(path, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    WavInfo w;
    if (!in.good() && bytes.empty()) return false;
    if (!ParseWav(bytes, w)) return false;
    size_t frameBytes = (size_t)w.channels * 2;
    size_t frames = w.dataBytes / frameBytes;
    if (!frames || frames > (size_t)w.sampleRate * 30) return false;
    pcm.resize(frames * (size_t)w.channels);
    for (size_t i = 0; i < pcm.size(); ++i) {
        int16_t v = (int16_t)Read16(bytes, w.dataOffset + i * 2);
        pcm[i] = (float)v / 32768.0f;
    }
    channels = w.channels; sampleRate = w.sampleRate;
    return true;
}
static std::string ModsSoundPath() {
    const char *dir = ym::api() ? ym::api()->modsDir : nullptr;
    return dir && dir[0] ? std::string(dir) + "/sounds" : "";
}
static double g_soundEnd = 0, g_soundStarted = 0, g_nextSoundPlay = 0;
static bool g_soundActive = false, g_soundRemote = false;
static Ref g_soundRecorder, g_oldRecorderClip;
static int g_oldSourceType = 0;
static bool g_oldLoop = false, g_oldVoiceDetection = false, g_oldRecording = false;
static bool g_haveOldVoiceDetection = false, g_haveOldRecording = false;

static void RestoreVoiceRecorder() {
    Il2CppObject *rec = g_soundRecorder.get();
    if (Alive(rec)) {
        if (G.recorderSetAudioClip.IsValid()) G.recorderSetAudioClip[rec](g_oldRecorderClip.get());
        if (G.recorderSetLoopAudioClip.IsValid()) G.recorderSetLoopAudioClip[rec](g_oldLoop);
        if (G.recorderSetSourceType.IsValid()) G.recorderSetSourceType[rec](g_oldSourceType);
        if (g_haveOldVoiceDetection && G.recorderSetVoiceDetection.IsValid()) G.recorderSetVoiceDetection[rec](g_oldVoiceDetection);
        if (g_haveOldRecording && G.recorderSetRecording.IsValid()) G.recorderSetRecording[rec](g_oldRecording);
        if (G.recorderRestart.IsValid()) G.recorderRestart[rec]();
    }
    g_soundRecorder.clear(); g_oldRecorderClip.clear();
    g_soundRemote = false;
}
static void StopSound() {
    if (g_soundRemote) RestoreVoiceRecorder();
    if (Alive(g_soundSource.get()) && G.audioSourceStop.IsValid()) G.audioSourceStop[g_soundSource.get()]();
    if (Alive(g_soundSource.get()) && G.audioSourceSetClip.IsValid()) G.audioSourceSetClip[g_soundSource.get()](nullptr);
    Kill(g_soundClip);
    g_soundActive = false;
    g_soundEnd = 0;
    g_soundStarted = 0;
}
static void ScanSounds() {
    g_soundDirectory = ModsSoundPath();
    if (g_soundDirectory.empty()) { Notify("Mods folder unavailable"); return; }
    g_soundFiles.clear();
    DIR *d = opendir(g_soundDirectory.c_str());
    if (!d) { Notify("Create mods/sounds folder first"); return; }
    while (dirent *e = readdir(d)) {
        std::string n = e->d_name;
        if (n.size() < 5) continue;
        std::string ext = n.substr(n.find_last_of('.'));
        for (char &c : ext) c = (char)tolower((unsigned char)c);
        if (ext == ".wav" || ext == ".mp3") g_soundFiles.push_back(g_soundDirectory + "/" + n);
    }
    closedir(d);
    std::sort(g_soundFiles.begin(), g_soundFiles.end());
    g_soundIndex = 0;
    Notify("Found %d WAV/MP3 files", (int)g_soundFiles.size());
}
static void SelectSound(int direction) {
    if (g_soundFiles.empty()) { Notify("No sounds. Scan sounds folder first"); return; }
    int n = (int)g_soundFiles.size();
    g_soundIndex = (size_t)((((int)g_soundIndex + direction) % n + n) % n);
    const std::string &p = g_soundFiles[g_soundIndex];
    size_t slash = p.find_last_of('/');
    Notify("Selected: %s", slash == std::string::npos ? p.c_str() : p.c_str() + slash + 1);
}
static void PreviousSound() { SelectSound(-1); }
static void NextSound() { SelectSound(1); }
static void SoundVolumeUp() {
    g_soundVolume = fminf(1.0f, g_soundVolume + 0.1f);
    if (Alive(g_soundSource.get())) G.audioSourceSetVolume[g_soundSource.get()](g_soundVolume);
    Notify("Sound volume: %d%%", (int)(g_soundVolume * 100));
}
static void SoundVolumeDown() {
    g_soundVolume = fmaxf(0.0f, g_soundVolume - 0.1f);
    if (Alive(g_soundSource.get())) G.audioSourceSetVolume[g_soundSource.get()](g_soundVolume);
    Notify("Sound volume: %d%%", (int)(g_soundVolume * 100));
}
static bool BeginVoicePlayback(Il2CppObject *clip) {
    if (!G.tRecorder || !G.findAll.IsValid() || !G.recorderGetSourceType.IsValid() ||
        !G.recorderSetSourceType.IsValid() || !G.recorderSetAudioClip.IsValid() ||
        !G.recorderSetLoopAudioClip.IsValid() || !G.recorderRestart.IsValid()) return false;
    auto *arr = (uint8_t *)G.findAll((Il2CppObject *)G.tRecorder);
    size_t count = arr ? *(uintptr_t *)(arr + 0x18) : 0;
    Il2CppObject **items = arr ? (Il2CppObject **)(arr + 0x20) : nullptr;
    for (size_t i = 0; i < count; ++i) {
        Il2CppObject *rec = items[i];
        if (!Alive(rec)) continue;
        if (G.recorderGetTransmitEnabled.IsValid() && !G.recorderGetTransmitEnabled[rec]()) continue;
        if (G.recorderGetRecording.IsValid() && !G.recorderGetRecording[rec]()) continue;
        if (G.recorderGetTransmitting.IsValid() && !G.recorderGetTransmitting[rec]()) continue;
        g_oldSourceType = G.recorderGetSourceType[rec]();
        g_oldRecorderClip.set(G.recorderGetAudioClip.IsValid() ? G.recorderGetAudioClip[rec]() : nullptr);
        g_oldLoop = G.recorderGetLoopAudioClip.IsValid() ? G.recorderGetLoopAudioClip[rec]() : false;
        g_haveOldVoiceDetection = G.recorderGetVoiceDetection.IsValid() && G.recorderSetVoiceDetection.IsValid();
        if (g_haveOldVoiceDetection) g_oldVoiceDetection = G.recorderGetVoiceDetection[rec]();
        g_haveOldRecording = G.recorderGetRecording.IsValid() && G.recorderSetRecording.IsValid();
        if (g_haveOldRecording) g_oldRecording = G.recorderGetRecording[rec]();
        g_soundRecorder.set(rec);
        g_soundRemote = true; // set before mutations so an exception can always restore the mic
        // Photon Voice InputSourceType.AudioClip is enum value 1.
        try {
            G.recorderSetLoopAudioClip[rec](false);
            if (G.recorderSetVoiceDetection.IsValid()) G.recorderSetVoiceDetection[rec](false);
            G.recorderSetAudioClip[rec](clip);
            G.recorderSetSourceType[rec](1);
            G.recorderRestart[rec]();
        } catch (...) {
            RestoreVoiceRecorder();
            ym::log("dark-matter: voice soundboard setup failed; mic restored");
            return false;
        }
        return true;
    }
    return false;
}
static void PlaySound() {
    if (g_soundActive || g_time < g_nextSoundPlay) { Notify("Soundboard is busy"); return; }
    if (g_soundFiles.empty()) ScanSounds();
    if (g_soundFiles.empty()) return;
    if (!G.audioClipCreate.IsValid() || !G.audioClipSetData.IsValid() || !G.arrayNew) {
        Notify("Unity audio methods unavailable"); return;
    }
    std::vector<float> pcm;
    int channels = 0, sampleRate = 0;
    const std::string path = g_soundFiles[g_soundIndex];
    if (!LoadSound(path, pcm, channels, sampleRate) || pcm.empty()) {
        Notify("Could not decode WAV/MP3 (max 30 sec)"); return;
    }
    size_t frames = pcm.size() / (size_t)channels;
    if (!frames || frames > (size_t)sampleRate * 30) { Notify("Sound must be 30 seconds or shorter"); return; }
    for (float &v : pcm) v = fmaxf(-1.0f, fminf(1.0f, v * g_soundVolume));
    void *managed = G.arrayNew(Class("System", "Single").GetClass(), (uintptr_t)pcm.size());
    if (!managed) { Notify("Could not allocate audio buffer"); return; }
    memcpy((uint8_t *)managed + 0x20, pcm.data(), pcm.size() * sizeof(float));
    StopSound();
    Il2CppObject *clip = G.audioClipCreate(CreateMonoString("DARK MATTER Sound"), (int)frames, channels, sampleRate, false);
    if (!clip || !G.audioClipSetData[clip](managed, 0)) {
        if (clip) G.destroy(clip);
        Notify("Could not create audio clip"); return;
    }
    g_soundClip.set(clip);
    bool remote = BeginVoicePlayback(clip);
    if (!remote) {
        if (!Alive(g_soundSource.get())) {
            Il2CppObject *go = Class("UnityEngine", "GameObject").CreateNewObjectParameters(CreateMonoString("DARK MATTER Soundboard"));
            Il2CppObject *source = go ? G.goAddComponent[go](G.tAudioSource) : nullptr;
            if (!source) { if (go) G.destroy(go); Kill(g_soundClip); Notify("No Photon recorder; local playback unavailable"); return; }
            g_soundSource.set(source);
            if (g_head.get()) G.trSetParent[Tr(go)](Tr(g_head.get()), false);
            G.audioSourceSetSpatialBlend[source](0.0f);
        }
        G.audioSourceSetVolume[g_soundSource.get()](1.0f);
        G.audioSourceSetClip[g_soundSource.get()](clip);
        G.audioSourcePlay[g_soundSource.get()]();
    }
    g_soundActive = true;
    g_soundStarted = g_time;
    double duration = (double)frames / (double)sampleRate;
    g_soundEnd = g_time + duration;
    g_nextSoundPlay = g_time + fmax(1.0, duration);
    size_t slash = path.find_last_of('/');
    Notify(remote ? "Sending to room: %s" : "Local only: %s",
           slash == std::string::npos ? path.c_str() : path.c_str() + slash + 1);
}
static void SoundboardTick() {
    if (!g_soundActive) return;
    if (g_time >= g_soundEnd) { StopSound(); Notify("Sound finished; microphone restored"); return; }
    if (g_soundRemote && Alive(g_soundRecorder.get()) && G.recorderGetTransmitting.IsValid() &&
        !G.recorderGetTransmitting[g_soundRecorder.get()]() && g_time >= g_soundStarted + 1.5 && g_time + 0.1 < g_soundEnd) {
        StopSound(); Notify("Voice stream stopped; microphone restored");
    }
}

static void OnUpdate(float dt) {
    g_time += dt;
    g_frames++;
    SoundboardTick();
    static double fpsStart = 0;
    if (g_time - fpsStart >= 1.0) { g_fps = (float)(g_frames / (g_time - fpsStart)); g_frames = 0; fpsStart = g_time; }
    if (!Lookups()) return;
    auto *player = (Il2CppObject *)ym::api()->localPlayer();
    if (!player) { if (g_soundActive) StopSound(); return; }
    if (player != g_player.get()) {                 // first frame in this room
        StopSound();
        g_soundSource.clear();
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
    LowGravityTick(in, dt);
    PlatformFor(in.lgrip, g_leftPlatform, g_leftHand);
    PlatformFor(in.rgrip, g_rightPlatform, g_rightHand);
    VelocityFlyTick(in);
    CarTick(in);
    IronManTick(in, dt);
    ApplySpeedBoost();
    ApplyGravity();
    NoclipTick(in);
    TeleportGunTick(in);
    try { SpawnOnHandTick(in); ItemQueueTick(); }
    catch (...) { g_itemQueue.clear(); ym::log("dark-matter: item code error"); }
    MovementExtrasTick(in);
    ThumbstickFlyTick();
    WeatherTick(dt);
    DanceTick(in, dt);              // last: it moves your hands after everything else
}

YM_EXPORT int YeepsMod_Init(const YeepsModApi *api) {
    if (!ym::init(api)) return -1;
    ym::onUpdate(OnUpdate);
    ym::log("DARK MATTER loaded on %s (%s)", api->gameVersion, api->buildId);
    return 0;
}
