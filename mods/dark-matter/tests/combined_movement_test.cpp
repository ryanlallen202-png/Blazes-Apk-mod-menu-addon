#include "CombinedSupport.h"
#include <cassert>
#include <cmath>
#include <limits>
struct Vector3 {
    float x = 0, y = 0, z = 0;
    Vector3() = default;
    Vector3(float a, float b, float c): x(a), y(b), z(c) {}
    Vector3 operator*(float k) const { return {x*k, y*k, z*k}; }
};
struct Buttons { bool a = false; };
struct Il2CppObject { bool alive = true; Vector3 position, forward{0,0,1}; float roll = 10; };
static bool failRoots = false;
struct Ref {
    Il2CppObject *value = nullptr;
    void set(Il2CppObject *o) { value = failRoots ? nullptr : o; }
    void clear() { value = nullptr; }
    Il2CppObject *get() const { return value; }
};
struct FloatField {
    bool valid = true;
    bool IsValid() const { return valid; }
    struct Bound {
        Il2CppObject *o;
        float operator()() const { return o->roll; }
        void operator=(float f) { o->roll = f; }
    };
    Bound operator[](Il2CppObject *o) { return {o}; }
};
struct Position { auto operator[](Il2CppObject *o) { return [o]() { return o->position; }; } };
struct Forward { auto operator[](Il2CppObject *o) { return [o]() { return o->forward; }; } };
struct SetPosition { auto operator[](Il2CppObject *o) { return [o](Vector3 p) { o->position = p; }; } };
struct Valid { bool valid = true; bool IsValid() const { return valid; } };
static struct { FloatField rollSpeed; Position trGetPosition; Forward trGetForward; SetPosition trSetPosition; Valid plNoGravityFor; } G;
static Ref g_player, g_rightHand;
static bool f_fastRoll = false, f_hoverFly = false, f_flipGravity = false, f_freezeMenu = false, f_noGravity = false, g_menuShown = false;
static Il2CppObject player, hand, transform;
static Vector3 velocity;
static int writes = 0;
static bool Alive(Il2CppObject *o) { return o && o->alive; }
static void Notify(const char *, ...) {}
static void SetVel(Vector3 v) { velocity = v; ++writes; }
static bool GetVel(Vector3 &v) { v = velocity; return true; }
static void StopMoving() { SetVel({0,0,0}); }
static Il2CppObject *PlayerTransform() { return &transform; }
#include "combined_movement.inc"
static bool f_tpGun = false, f_itemGun = false, g_utilityFlags[8] = {};
static int g_utilitySelected = -1;
#include "combined_selection.inc"
static void Reset() {
    failRoots = false; G = {}; player = {}; hand = {}; transform = {};
    f_fastRoll = f_hoverFly = f_flipGravity = f_freezeMenu = f_noGravity = g_menuShown = false;
    g_player.set(&player); g_rightHand.set(&hand); g_rollOwner.clear(); g_frozenTransform.clear();
    velocity = {}; writes = 0;
}
int main() {
    using namespace diddy::combined;
    Reset(); f_hoverFly = true;
    Buttons in; in.a = true; HoverFlyTick(in); assert(velocity.z == 20);
    in.a = false; HoverFlyTick(in); assert(velocity.z == 0);
    g_menuShown = true; const int before = writes; HoverFlyTick(in); assert(writes == before);
    g_menuShown = false; G.plNoGravityFor.valid = false; HoverFlyTick(in); assert(!f_hoverFly);
    Reset(); f_flipGravity = true; FlippedGravityTick(1); assert(std::abs(velocity.y - 0.981f) < 0.00001f);
    velocity.y = 12; FlippedGravityTick(0.02f); assert(velocity.y == 12);
    f_noGravity = true; const int oldWrites = writes; FlippedGravityTick(0.02f); assert(writes == oldWrites);
    Reset(); f_freezeMenu = g_menuShown = true; transform.position = {1,2,3};
    MenuFreezeTick(); transform.position = {10,20,30}; velocity = {5,5,5}; MenuFreezeTick();
    assert(transform.position.x == 1 && velocity.x == 0);
    g_menuShown = false; MenuFreezeTick(); assert(!g_frozenTransform.get());
    transform.position = {10,20,30}; g_menuShown = true; MenuFreezeTick(); assert(transform.position.x == 10);
    CombinedMovementReset(); assert(!g_frozenTransform.get());
    Reset(); f_fastRoll = true; player.roll = 12; FastRollTick(); assert(player.roll == 24);
    FastRollTick(); assert(player.roll == 24); f_fastRoll = false; FastRollTick(); assert(player.roll == 12);
    Reset(); f_fastRoll = true; player.roll = 30; FastRollTick(); assert(player.roll == 40);
    CombinedMovementReset(); assert(player.roll == 30 && !g_rollOwner.get());
    Reset(); f_fastRoll = true; G.rollSpeed.valid = false; FastRollTick(); assert(!f_fastRoll && player.roll == 10);
    Reset(); f_fastRoll = true; failRoots = true; FastRollTick(); assert(!f_fastRoll && player.roll == 10);
    Reset(); f_fastRoll = true; player.roll = std::numeric_limits<float>::infinity(); FastRollTick(); assert(!f_fastRoll);
    Reset(); f_fastRoll = true; FastRollTick(); player.alive = false; CombinedMovementReset(); assert(!g_rollOwner.get());
    // Real helper functions: edge-only triggers, inactive/menu-held triggers and cooldown.
    TriggerGate gate;
    assert(!gate.Take(false, true, 1)); assert(!gate.Take(true, true, 2));
    assert(!gate.Take(true, false, 2)); assert(gate.Take(true, true, 2));
    assert(!gate.Take(true, true, 3)); gate.Take(true, false, 3); assert(gate.Take(true, true, 3));
    gate.Take(true, false, 3.1); assert(!gate.Take(true, true, 3.1));
    gate.Reset(); assert(!gate.Take(true, true, 4));
    gate.Take(true, false, 4); assert(gate.Take(true, true, 4));
    f_tpGun = true; g_utilityFlags[2] = true; UtilityPicked();
    assert(g_utilitySelected == 2 && !f_tpGun);
    g_utilityFlags[4] = true; UtilityPicked(); assert(g_utilitySelected == 4 && !g_utilityFlags[2]);
    g_utilityFlags[4] = false; UtilityPicked(); assert(g_utilitySelected == -1);
    g_utilityFlags[0] = true; UtilityPicked(); f_itemGun = true; PrimaryGunPicked();
    assert(g_utilitySelected == -1 && !g_utilityFlags[0]);
    assert(SafeDt(-1) == 0 && SafeDt(2) == 0.05f && SafeDt(std::numeric_limits<float>::quiet_NaN()) == 0);
    float roll = 0; assert(!RollSpeed(-1, roll) && !RollSpeed(41, roll));
    assert(PlainText(nullptr, 8).empty() && PlainText(u"abc", -1).empty());
    assert(PlainText(u"<b>hi</b>\n", 10).find('<') == std::string::npos);
    assert(PlainText(u"\u00e9", 1) == "\xc3\xa9");
    assert(PlainText(u"\U0001f600", 2) == "\xf0\x9f\x98\x80");
    const char16_t bad[] = {0xd800}; assert(PlainText(bad, 1) == "\xef\xbf\xbd");
    const std::u16string longText(1000, u'X'); assert(PlainText(longText.data(), 1000).size() == 180);
}
