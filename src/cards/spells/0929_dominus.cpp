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

class Dominus : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "This turn, double a unit's Might and give it "[A][A]: Ready me.""
    // The doubling is a this-turn bonus equal to its current Might. The
    // ability is granted through GameObject::temp_granted_abilities (merged
    // into granted_abilities every aura pass, cleared at end of turn).
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Targets come from the base (requirement-driven) enumerator; the
        // one-arg overload is not overridden by this card.
        auto legal = Card::enumerateLegalTargets(ctx.state, ctx.controller);
        std::sort(legal.begin(), legal.end());
        GameObjectId picked = pickTarget(ctx, "Dominus", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        const int bonus = ctx.state.getObject(picked).current_might;
        if (bonus > 0) ctx.executor.giveTemporaryMight(picked, bonus);
        if (!ctx.state.objectExists(picked)) return;
        auto& unit = ctx.state.getObject(picked);
        const GameObject::GrantedAbilityRef ref{cardDefId(), 0};
        unit.temp_granted_abilities.push_back(ref);
        unit.granted_abilities.push_back(ref);   // usable now; the aura pass keeps it
        RIFT_TRACE(ctx.events, "DOMINUS: doubled Might this turn (+" + std::to_string(bonus) +
                            ") and granted \"[A][A]: Ready me.\"");
    }
    // The granted "[A][A]: Ready me." — ability 0, dispatched as a granted
    // ability with the bearer as ctx.source.
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility ab;
        ab.cost.power = 2;
        ab.cost.power_any_domain = true;
        return {ab};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& /*state*/, PlayerId /*controller*/,
        int /*ability_index*/) const override {
        return {};
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.readyObject(ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 929;
        d.def_id = R"RB(ven-142-166)RB";
        d.name = R"RB(Dominus)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-142/166)RB";
        d.collector_number = 142;
        d.artist = R"RB(小牛设计)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Body};
        d.tags = {R"RB(Renekton)RB"};
        d.energy_cost = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
This turn, double a unit's Might and give it "[A][A]: Ready me.")RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0d5ccc8bb0b70473add784a3f4c71e1e8635e2ce-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_929(CardRegistry& r) {
    r.registerCard(929, std::make_unique<Dominus>());
}

}  // namespace riftbound
