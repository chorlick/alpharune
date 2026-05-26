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

class BloodMoney : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        // Capture ownership BEFORE killing — "If it was an enemy unit, play a
        // Gold gear token exhausted. If it was a friendly unit, play two."
        bool was_enemy = ctx.state.getObject(targets[0]).controller != ctx.controller;
        ctx.executor.killObject(targets[0]);
        createGoldExhausted(ctx);
        if (!was_enemy) createGoldExhausted(ctx);
        ctx.events.logTrace(std::string("BLOOD MONEY: killed ") +
                            (was_enemy ? "enemy -> 1 Gold" : "friendly -> 2 Gold"));
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_at_battlefield = true, .max_might = 2};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 483;
        d.def_id = R"RB(sfd-162-221)RB";
        d.name = R"RB(Blood Money)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-162/221)RB";
        d.collector_number = 162;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Kill a unit at a battlefield with 2 [M] or less. If it was an enemy unit, play a Gold gear token exhausted. If it was a friendly unit, play two Gold gear tokens exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0062fe2a96fc94bd8d85c01607a48e8619ed4e20-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_483(CardRegistry& r) {
    r.registerCard(483, std::make_unique<BloodMoney>());
}

} // namespace riftbound
