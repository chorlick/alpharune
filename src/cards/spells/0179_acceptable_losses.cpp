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

class AcceptableLosses : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Each player kills one of their gear." Each player chooses which of THEIR
    // OWN gear to kill. Resumable: controller picks first, then opponent.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto own_gear = [&](PlayerId p) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isGear() || obj.controller != p) continue;
                if (!obj.location.has_value()) continue;
                out.push_back(id);
            }
            return out;
        };
        auto build_choices = [&](PlayerId p, const std::vector<GameObjectId>& gear) {
            std::vector<Intent> out;
            for (auto gid : gear) {
                Intent c; c.type = IntentType::MakeChoice; c.player = p;
                c.chosen_objects = {gid}; out.push_back(std::move(c));
            }
            return out;
        };
        PlayerId opp = opponent(ctx.controller);
        // Helper to prompt the opponent (or finish). Returns true if it
        // published a choice (caller must return).
        auto prompt_opponent = [&]() -> bool {
            auto gear = own_gear(opp);
            if (gear.empty()) return false;
            ctx.executor.requestChoice(opp, build_choices(opp, gear),
                                        "Acceptable Losses: kill one of your gear");
            ri.resume_point = 2;
            return true;
        };
        switch (ri.resume_point) {
        case 0: {  // controller picks their gear
            auto gear = own_gear(ctx.controller);
            if (gear.empty()) { prompt_opponent(); return; }
            ctx.executor.requestChoice(ctx.controller,
                                        build_choices(ctx.controller, gear),
                                        "Acceptable Losses: kill one of your gear");
            ri.resume_point = 1;
            return;
        }
        case 1: {  // apply controller's kill, then prompt opponent
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty())
                ctx.executor.killObject(choice->chosen_objects[0]);
            prompt_opponent();
            return;
        }
        case 2: {  // apply opponent's kill
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty())
                ctx.executor.killObject(choice->chosen_objects[0]);
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 179;
        d.def_id = R"RB(ogn-179-298)RB";
        d.name = R"RB(Acceptable Losses)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-179/298)RB";
        d.collector_number = 179;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Each player kills one of their gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b2b470bab1ae511ab9de0b1ce576e2050532a081-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_179(CardRegistry& r) {
    r.registerCard(179, std::make_unique<AcceptableLosses>());
}

} // namespace riftbound
