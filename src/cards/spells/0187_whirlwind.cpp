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

class Whirlwind : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Starting with the next player, each player may return a unit to its
    // owner's hand." Next player (opponent) chooses first, then the controller.
    // Each choice is optional ("may"): a decline option (empty chosen_objects)
    // is offered alongside every unit. The units are not targets: a set
    // chosen in part by other players (CR 355.10.e). So "can't be chosen by
    // enemy spells and abilities" (untargetable: Vendetta core rules patch
    // notes) does not keep a unit out, and no [Deflect] is owed (CR 809.1.c:
    // "spells and abilities ... that target"; testing phase R9).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        PlayerId opp = opponent(ctx.controller);
        auto units_for = [&](PlayerId) {
            return boardObjects(ctx.state, [](const GameObject& o) { return o.isUnit(); });
        };
        auto prompt = [&](PlayerId p) {
            std::vector<Intent> choices;
            // Decline option first.
            Intent decline; decline.type = IntentType::MakeChoice; decline.player = p;
            decline.choice_label = "Decline";
            decline.chosen_value = 0;   // nameable by value, not only by position
            choices.push_back(decline);
            for (auto uid : units_for(p)) choices.push_back(objChoice(p, uid));
            // At resolution: each player chooses (CR 355.10.e; batch M1).
            ctx.executor.requestChoice(p, std::move(choices),
                                        "Whirlwind: you may return a unit to its owner's hand", PromptKind::ChooseObject);
        };
        auto apply = [&](PlayerId p) {
            const GameObjectId unit = takenObject(ctx);
            const auto units = units_for(p);
            if (std::find(units.begin(), units.end(), unit) == units.end()) return;
            ctx.executor.bounceToHand(unit);
        };
        switch (ri.resume_point) {
        case 0:  // next player (opponent) chooses
            prompt(opp);
            ri.resume_point = 1;
            return;
        case 1:  // apply opponent, then controller chooses
            apply(opp);
            prompt(ctx.controller);
            ri.resume_point = 2;
            return;
        case 2:  // apply controller
            apply(ctx.controller);
            ri.resume_point = 3;
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 187;
        d.def_id = R"RB(ogn-187-298)RB";
        d.name = R"RB(Whirlwind)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-187/298)RB";
        d.collector_number = 187;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Starting with the next player, each player may return a unit to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/24f5e0a8811b5a0e0930c0f6476600e2a37a5f93-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_187(CardRegistry& r) {
    r.registerCard(187, std::make_unique<Whirlwind>());
}

} // namespace riftbound
