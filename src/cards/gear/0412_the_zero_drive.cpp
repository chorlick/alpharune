#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class TheZeroDrive : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // ── Equip [1][B] ──
    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        return standardEquip(ctx, ctx.source, unit, /*energy_cost=*/1, Domain::Mind);
    }

    // ── Equipped: "[Deathknell] — Banish me." ──
    // A Deathknell, not a replacement: the bearer dies like any unit (its own
    // Deathknells and "when a friendly unit dies" abilities trigger), and this
    // ability then banishes its card from the trash. A token bearer ceased to
    // exist as it died (CR 183.1): nothing is banished, nothing is recorded.
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIDie; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                           const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(unit) || !ctx.state.objectExists(ctx.source)) return;
        const auto& dead = ctx.state.getObject(unit);
        if (dead.isToken() || dead.zone != ZoneType::Trash) return;   // no longer where it died to
        ctx.executor.banishObject(unit);
        // "banished with this": only a Drive still on the board can play it back.
        auto& gear = ctx.state.getObject(ctx.source);
        if (gear.location.has_value()) gear.tracked_objects.push_back(unit);
        ctx.events.logTrace("THE ZERO DRIVE: Deathknell — banished " +
                            ctx.state.getObject(unit).name);
    }

    // ── "[3][B], Banish this: Play all units banished with this, ignoring
    //     their costs. (Use only if unattached.)" ──
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        // ENGINE-GAP: ActivationCost has no "banish this" field, so that part
        // of the cost is paid as the ability resolves (onActivate).
        return {.exhaust = false, .energy = 3, .power = 1, .power_domain = Domain::Mind};
    }
    // "(Use only if unattached.)" — asked of the copy that is activating. Not
    // offered with nothing to play back (the activation would only banish it).
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!GearCard::canActivateAbilityAt(state, controller, source, ability_index))
            return false;
        if (!state.objectExists(source)) return false;
        const auto& gear = state.getObject(source);
        return gear.location.has_value() && !gear.attached_to.has_value() &&
               !banishedWithThis(state, gear).empty();
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& gear = ctx.state.getObject(ctx.source);
        // Left the board or was attached in response: the cost can't be paid.
        if (!gear.location.has_value() || gear.attached_to.has_value()) return;
        const std::vector<GameObjectId> to_play = banishedWithThis(ctx.state, gear);
        gear.tracked_objects.clear();
        // "Banish this" (cost), then the effect.
        ctx.executor.banishObject(ctx.source);
        for (auto uid : to_play) {
            // playIgnoringCost takes the card out of the banishment zone itself.
            ctx.executor.playIgnoringCost(ctx.controller, uid);
            ctx.events.logTrace("THE ZERO DRIVE: replayed banished unit " +
                                ctx.state.getObject(uid).name);
        }
    }
private:
    /// The units this Drive banished that are still in banishment.
    static std::vector<GameObjectId> banishedWithThis(const GameState& state,
                                                      const GameObject& gear) {
        std::vector<GameObjectId> out;
        for (auto uid : gear.tracked_objects) {
            if (!state.objectExists(uid)) continue;
            const auto& u = state.getObject(uid);
            if (u.zone != ZoneType::Banishment || u.isToken()) continue;
            const auto& bz = state.player(u.owner).banishment;
            if (std::find(bz.begin(), bz.end(), uid) != bz.end()) out.push_back(uid);
        }
        return out;
    }
    const CardDef def_ = [] {
        CardDef d;
        d.id = 412;
        d.def_id = R"RB(sfd-090-221)RB";
        d.name = R"RB(The Zero Drive)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-090/221)RB";
        d.collector_number = 90;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [1][B] ([1][B]: Attach this to a unit you control.)
[3][B], Banish this: Play all units banished with this, ignoring their costs. (Use only if unattached.))RB";
        d.effect_text = R"RB([Deathknell] — Banish me. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cc80cbfa1a64ad22dc5f7df870e9206f6938f7f5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_412(CardRegistry& r) {
    r.registerCard(412, std::make_unique<TheZeroDrive>());
}

} // namespace riftbound
