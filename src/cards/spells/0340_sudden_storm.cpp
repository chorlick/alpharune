#include "cards/card.h"
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

class SuddenStorm : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        auto& tgt = ctx.state.getObject(targets[0]);
        int dmg = (tgt.combat_designation == CombatDesignation::Attacker) ? 4 : 2;
        ctx.executor.dealDamage(targets[0], dmg, ctx.source);
        if (ctx.state.objectExists(targets[0]) &&
            ctx.state.getObject(targets[0]).hasLethalDamage()) {
            ctx.executor.killObject(targets[0]);
        }
        ctx.events.logTrace("SUDDEN STORM: deal " + std::to_string(dmg));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 340;
        d.def_id = R"RB(sfd-017-221)RB";
        d.name = R"RB(Sudden Storm)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-017/221)RB";
        d.collector_number = 17;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Deal 2 to a unit at a battlefield. If it's attacking, deal 4 to it instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ba3e8c3e95137a289dbdec88e96e5c5a9a45f2c7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_340(CardRegistry& r) {
    r.registerCard(340, std::make_unique<SuddenStorm>());
}

} // namespace riftbound
