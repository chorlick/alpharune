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

// "You may play a unit from hand to a battlefield you control, reducing its
//  cost by [3]." ([Hidden] / [Action] are engine-handled timing.)
//
// NOTE: there is no partial cost-reduction play path (playIgnoringCost ignores
// ALL cost). The chosen unit is played for free — documented approximation of
// "-[3]"; most reachable units cost <= 3.

class HereToHelp : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        auto unit_choices = [&]() {
            std::vector<Intent> out;
            for (auto cid : ps.hand) {
                if (!ctx.state.objectExists(cid)) continue;
                if (!ctx.state.getObject(cid).isUnit()) continue;
                Intent c; c.type = IntentType::MakeChoice; c.player = ctx.controller;
                c.chosen_objects = {cid}; out.push_back(std::move(c));
            }
            return out;
        };
        switch (ri.resume_point) {
        case 0: {
            auto choices = unit_choices();
            if (choices.empty()) return;  // no unit to play — "you may" fizzles
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "Here to Help: play a unit from hand");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (!choice || choice->chosen_objects.empty()) return;
            GameObjectId unit = choice->chosen_objects[0];
            if (!ctx.state.objectExists(unit)) return;
            // Choose a battlefield you control (fallback: base).
            LocationId dest{BaseLocation{ctx.controller}};
            for (const auto& bf : ctx.state.battlefields) {
                if (bf.controller && *bf.controller == ctx.controller) {
                    dest = LocationId{BattlefieldLocation{bf.id}};
                    break;
                }
            }
            // Remove from hand before re-playing.
            auto it = std::find(ps.hand.begin(), ps.hand.end(), unit);
            if (it != ps.hand.end()) ps.hand.erase(it);
            ctx.executor.playIgnoringCost(ctx.controller, unit, dest);
            ctx.events.logTrace("HERE TO HELP: played a unit from hand to a "
                                "controlled battlefield (-[3] approximated as free)");
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 433;
        d.def_id = R"RB(sfd-111-221)RB";
        d.name = R"RB(Here to Help)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-111/221)RB";
        d.collector_number = 111;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
You may play a unit from hand to a battlefield you control, reducing its cost by [3].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6daa8caf27f76f2b0bce189fb9df196129582afa-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_433(CardRegistry& r) {
    r.registerCard(433, std::make_unique<HereToHelp>());
}

} // namespace riftbound
