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

class DecreeOfFocus : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a friendly unit that's in combat with an enemy Fury ([R]) unit or
    //  that's being chosen by an enemy Fury spell. Give it +4 [M] this turn."
    //
    // "Being chosen by an enemy Fury spell" reads ChainItem::chosen_objects —
    // everything the item chose, at play time or (for spells that pick at
    // resolution) so far while resolving — plus the spell currently resolving
    // (ChainState::resuming), which has left chain.items.
    static bool hasDomain(const GameObject& obj, Domain d) {
        return std::find(obj.domains.begin(), obj.domains.end(), d) != obj.domains.end();
    }
    static bool qualifies(const GameState& state, PlayerId controller, const GameObject& unit) {
        if (unit.isAtBattlefield() && unit.combat_designation != CombatDesignation::None) {
            for (const auto& [eid, e] : state.objects.onBoard()) {
                if (!e.isUnit() || e.controller == controller || !e.location.has_value()) continue;
                if (!(*e.location == *unit.location)) continue;
                if (e.combat_designation == CombatDesignation::None) continue;
                if (hasDomain(e, Domain::Fury)) return true;
            }
        }
        auto chooses = [&](const ChainItem& item) {
            if (!item.is_spell || item.controller == controller) return false;
            if (!state.objectExists(item.source)) return false;
            if (!hasDomain(state.getObject(item.source), Domain::Fury)) return false;
            return std::find(item.chosen_objects.begin(), item.chosen_objects.end(), unit.id)
                       != item.chosen_objects.end() ||
                   std::find(item.targets.begin(), item.targets.end(), unit.id)
                       != item.targets.end();
        };
        for (const auto& item : state.chain.items)
            if (chooses(item)) return true;
        return state.chain.resuming.has_value() && chooses(*state.chain.resuming);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    using Card::enumerateLegalTargets;
    std::vector<GameObjectId> enumerateLegalTargets(const GameState& state,
                                                    PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != controller || !obj.location.has_value()) continue;
            if (qualifies(state, controller, obj)) out.push_back(id);
        }
        return out;
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Decree of Focus", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.giveTemporaryMight(picked, 4);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 827;
        d.def_id = R"RB(ven-040-166)RB";
        d.name = R"RB(Decree of Focus)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-040/166)RB";
        d.collector_number = 40;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a friendly unit that's in combat with an enemy Fury ([R]) unit or that's being chosen by an enemy Fury spell. Give it +4 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3f9a1b209cfd6097bac6fa9aec57531e300ef511-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_827(CardRegistry& r) {
    r.registerCard(827, std::make_unique<DecreeOfFocus>());
}

}  // namespace riftbound
