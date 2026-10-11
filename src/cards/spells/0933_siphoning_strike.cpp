#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class SiphoningStrike : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Deal 4 to a unit at a battlefield. If you control 7 or more runes, deal
    //  7 to it instead. When it dies this turn, channel 1 rune exhausted."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        std::sort(legal.begin(), legal.end());
        GameObjectId picked = pickTarget(ctx, "Siphoning Strike", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        // Arm the death-watch BEFORE dealing damage (Deadly Flourish pattern):
        // TriggerManager::onUnitDied fires delayed WhenIDie abilities scoped
        // to this unit for either player.
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = TriggerType::WhenIDie;
        da.target_filter = picked;
        da.expires_on_turn = ctx.state.turn.turn_number;   // "this turn"
        ctx.state.delayed_abilities.push_back(da);
        const int dmg = runesControlled(ctx.state, ctx.controller) >= 7 ? 7 : 4;
        ctx.executor.dealDamage(picked, dmg, ctx.source);
        // Lethal damage is turned into a death by the engine's cleanup.
    }
    // The delayed "When it dies this turn" ability.
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 933;
        d.def_id = R"RB(ven-146-166)RB";
        d.name = R"RB(Siphoning Strike)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-146/166)RB";
        d.collector_number = 146;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Nasus)RB"};
        d.energy_cost = 4;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Deal 4 to a unit at a battlefield. If you control 7 or more runes, deal 7 to it instead. When it dies this turn, channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8b28e4a0681eee9b19c29770a5f167e1e68c78cf-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_933(CardRegistry& r) {
    r.registerCard(933, std::make_unique<SiphoningStrike>());
}

}  // namespace riftbound
