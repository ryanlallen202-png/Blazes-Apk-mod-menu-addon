// Native adaptations of the supported TS features. Included by menu.cpp so the
// combined library has exactly one entry point, menu, input reader and callback.

// ---- combined movement (also extracted by host tests) ------------------------
static Ref g_frozenTransform, g_rollOwner;
static Vector3 g_frozenPosition;
static float g_rollOriginal = 0;

static void RestoreFastRoll() {
    if (Alive(g_rollOwner.get()) && G.rollSpeed.IsValid()) G.rollSpeed[g_rollOwner.get()] = g_rollOriginal;
    g_rollOwner.clear();
}
static void FastRollTick() {
    auto *player = g_player.get();
    if (!f_fastRoll || !Alive(player)) { RestoreFastRoll(); return; }
    if (g_rollOwner.get() == player) return;
    RestoreFastRoll();
    float desired = 0;
    if (!G.rollSpeed.IsValid() || !diddy::combined::RollSpeed(G.rollSpeed[player](), desired)) {
        f_fastRoll = false;
        Notify("Fast roll unavailable on this build");
        return;
    }
    const float original = G.rollSpeed[player]();
    g_rollOwner.set(player);
    if (!g_rollOwner.get()) { f_fastRoll = false; return; }
    g_rollOriginal = original;
    G.rollSpeed[player] = desired;
}
static void HoverFlyTick(const Buttons &in) {
    if (!f_hoverFly || g_menuShown || !Alive(g_rightHand.get())) return;
    if (!G.plNoGravityFor.IsValid()) { f_hoverFly = false; Notify("Hover gravity control unavailable"); return; }
    SetVel(in.a ? G.trGetForward[g_rightHand.get()]() * 20.0f : Vector3(0, 0, 0));
}
static void FlippedGravityTick(float dt) {
    Vector3 v;
    if (!f_flipGravity || f_hoverFly || f_noGravity || g_menuShown || !GetVel(v)) return;
    v.y = fminf(12.0f, v.y + 19.62f * diddy::combined::SafeDt(dt));
    SetVel(v);
}
static void MenuFreezeTick() {
    if (!f_freezeMenu || !g_menuShown) { g_frozenTransform.clear(); return; }
    auto *t = PlayerTransform();
    if (!Alive(t)) { g_frozenTransform.clear(); return; }
    if (g_frozenTransform.get() != t) {
        g_frozenPosition = G.trGetPosition[t]();
        g_frozenTransform.set(t);
    }
    if (!Alive(g_frozenTransform.get())) return;
    G.trSetPosition[t](g_frozenPosition);
    StopMoving();
}
static void CombinedMovementReset() {
    RestoreFastRoll();
    g_frozenTransform.clear();
}

// ---- read-only TS utility tools ---------------------------------------------
static std::string UtilityText(BNM::Structures::Mono::String *s) {
    return s ? diddy::combined::PlainText(reinterpret_cast<const char16_t *>(s->chars), s->length) : std::string();
}
static void PrimaryGunPicked() {
    if (!f_tpGun && !f_itemGun) return;
    for (bool &flag : g_utilityFlags) flag = false;
    g_utilitySelected = -1;
}
static void UtilityPicked() {
    int picked = -1;
    for (int i = 0; i < 8; ++i) if (g_utilityFlags[i] && i != g_utilitySelected) picked = i;
    for (int i = 0; i < 8; ++i) g_utilityFlags[i] = i == picked;
    g_utilitySelected = picked;
    if (picked >= 0) {
        f_tpGun = f_itemGun = false;
        Notify("Grip to aim; press trigger once to inspect");
    }
}
static void UtilityTick(const Buttons &in) {
    // Track trigger even while the tool/menu is inactive: holding it while enabling
    // a tool must not count as a new press. Inspection never sends a network message.
    if (!g_utilityGate.Take(g_utilitySelected >= 0 && in.rgrip && !g_menuShown, in.rtrig, g_time)) return;
    if (!Alive(g_rightHand.get()) || !AudioCallable(G.raycast) || !AudioCallable(G.utilityFromId)) {
        Notify("Object inspection unavailable on this build"); return;
    }
    const Vector3 start = G.trGetPosition[g_rightHand.get()]();
    const Vector3 dir = G.trGetForward[g_rightHand.get()]();
    RaycastHit hit{};
    if (!AudioCall<bool>(G.raycast, nullptr, start + dir * 0.25f, dir, &hit, 80.0f, -5)) { Notify("No object hit"); return; }
    // Unlike RaycastHit::GetCollider's SDK internal-call fallback, this path checks
    // that an AOT invoker exists before resolving an instance ID.
    auto *collider = AudioCall<Il2CppObject *>(G.utilityFromId, nullptr, hit.m_Collider);
    if (!Alive(collider)) { Notify("Object no longer exists"); return; }
    const int tool = g_utilitySelected;
    Il2CppObject *target = collider;
    if (tool < 7) {
        if (!G.utilityTypes[tool] || !AudioCallable(G.utilityParent)) { Notify("This component is unavailable"); return; }
        target = AudioCall<Il2CppObject *>(G.utilityParent, collider, G.utilityTypes[tool]);
        if (!Alive(target)) { Notify("Aim at the matching object type"); return; }
    }
    std::string text;
    if (tool == 0) {
        if (!G.signText.IsValid() || !AudioCallable(G.signGetText)) { Notify("Sign text unavailable"); return; }
        auto *label = G.signText[target]();
        if (Alive(label)) text = UtilityText(AudioCall<BNM::Structures::Mono::String *>(G.signGetText, label));
    } else if (tool == 1) {
        Vector3 point = hit.point;
        auto *t = AudioCall<Il2CppObject *>(G.compTransform, target);
        if (Alive(t)) point = G.trGetPosition[t]();
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) return;
        g_spot = point + Vector3(0, 1, 0);
        g_haveSpot = true;
        Notify("Map origin saved: %.1f %.1f %.1f", point.x, point.y, point.z);
        ym::log("diddy combined: map world origin %.2f %.2f %.2f", point.x, point.y, point.z);
        return;
    } else if (tool == 2) {
        if (!AudioCallable(G.itemKey)) { Notify("Item key unavailable"); return; }
        text = UtilityText(AudioCall<BNM::Structures::Mono::String *>(G.itemKey, target));
    } else if (tool == 3) {
        if (!AudioCallable(G.mapKey)) { Notify("Map key unavailable"); return; }
        text = UtilityText(AudioCall<BNM::Structures::Mono::String *>(G.mapKey, target));
    } else if (tool == 4) {
        if (!AudioCallable(G.mobKey)) { Notify("Mob key unavailable"); return; }
        text = UtilityText(AudioCall<BNM::Structures::Mono::String *>(G.mobKey, target));
    } else if (tool == 5) {
        if (!G.techPattern.IsValid()) { Notify("Pattern label unavailable"); return; }
        text = UtilityText(G.techPattern[target]());
    } else if (tool == 6) {
        text = "Dispenser component found (read-only)";
    } else if (tool == 7 && AudioCallable(G.utilityName)) {
        text = UtilityText(AudioCall<BNM::Structures::Mono::String *>(G.utilityName, target));
    }
    if (text.empty()) text = "No readable label";
    ym::log("diddy combined: inspect tool=%d class=%.80s text=%s", tool,
            target->klass && target->klass->name ? target->klass->name : "?", text.c_str());
    Notify("%s", text.c_str());
}

// ---- yeepsmenuD metadata browser --------------------------------------------
// One class, or up to four member NAMES, per explicit press. No object values,
// files, account state, full-domain loops, injected scripts or export-name table.
static size_t g_metadataIndex = 0, g_metadataMember = 0;
static BNM::IL2CPP::Il2CppClass *g_metadataCurrent = nullptr;
static double g_nextMetadata = 0;
static void MetadataNextClass() {
    if (g_time < g_nextMetadata) return;
    g_nextMetadata = g_time + 0.6;
    if (!G.metadataImage || !G.metadataClass || !G.metadataImage->typeCount) {
        Notify("Metadata browser unavailable"); return;
    }
    const size_t count = G.metadataImage->typeCount;
    g_metadataIndex %= count;
    auto *raw = G.metadataClass(G.metadataImage, g_metadataIndex++);
    g_metadataCurrent = raw ? Class(raw).GetClass() : nullptr;
    g_metadataMember = 0;
    if (!g_metadataCurrent) { Notify("Class unavailable; press Next again"); return; }
    auto *k = g_metadataCurrent;
    ym::log("diddy combined: class %zu/%zu %.80s.%.96s fields=%u methods=%u properties=%u", g_metadataIndex, count,
            k->namespaze ? k->namespaze : "", k->name ? k->name : "?", unsigned(k->field_count), unsigned(k->method_count), unsigned(k->property_count));
    Notify("Class %zu/%zu: %.65s", g_metadataIndex, count, k->name ? k->name : "?");
}
static void MetadataNextMembers() {
    if (!g_metadataCurrent) { MetadataNextClass(); return; }
    if (g_time < g_nextMetadata) return;
    g_nextMetadata = g_time + 0.6;
    auto *k = g_metadataCurrent;
    const size_t fields = k->field_count, methods = k->method_count, total = fields + methods + k->property_count;
    if (!total) { Notify("This class has no members"); return; }
    if (g_metadataMember >= total) g_metadataMember = 0;
    for (int printed = 0; printed < 4 && g_metadataMember < total; ++printed, ++g_metadataMember) {
        const size_t index = g_metadataMember;
        const char *kind = "property", *name = nullptr;
        if (index < fields) { kind = "field"; if (k->fields) name = k->fields[index].name; }
        else if (index < fields + methods) {
            kind = "method";
            if (k->methods && k->methods[index - fields]) name = k->methods[index - fields]->name;
        } else if (k->properties) name = k->properties[index - fields - methods].name;
        ym::log("diddy combined: %s %.96s", kind, name ? name : "unavailable");
    }
    Notify("Members %zu/%zu: see checkpoint", g_metadataMember, total);
}
static void CheckRoomSender() {
    bool ready = false;
    try { ready = AudioCallable(G.netLocal) && Alive(AudioCall<Il2CppObject *>(G.netLocal, nullptr)); }
    catch (...) { ym::log("diddy combined: room sender check unavailable"); }
    Notify(ready ? "Room sender is ready" : "Room sender unavailable");
}
static void CombinedNotes() {
    Notify("TS layouts: @goldentrophy / @crimsoncauldron; see port notes");
    ym::log("diddy combined: native supported subset of ym.ts, YeepsMenu.ts, IIStupidYeepsMenu.ts; bounded yeepsmenuD browser");
}
