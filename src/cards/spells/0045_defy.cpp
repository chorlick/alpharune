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

class Defy : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // Playable only if there is a counterable spell on the chain. The [4]
    // cost gate is re-checked at resolve via the CardDB def lookup; only the
    // printed energy cost is modeled ([A] = additional power, not tracked).
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it) {
            if (!it->is_spell) continue;
            if (it->card_def_id == kInvalidId) continue;
            return true;  // cost re-checked at resolve via def lookup
        }
        return false;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.chain.items.empty()) return;
        auto& top = ctx.state.chain.items.back();
        if (!top.is_spell) return;

        // Cost gate: "no more than [4]". Read the static energy cost of the
        // spell being countered from the CardDB.
        int cost = 0;
        if (top.card_def_id != kInvalidId) {
            cost = ctx.executor.cardDB().get(top.card_def_id).energy_cost;
        }
        if (cost > 4) {
            ctx.events.logTrace("DEFY: cannot counter — cost " +
                                 std::to_string(cost) + " exceeds [4]");
            return;  // illegal target; spell resolves normally
        }

        auto countered = top.source;
        revertCounteredPlay(ctx, top);  // CR 425.1.b
        ctx.state.chain.items.pop_back();
        if (ctx.state.objectExists(countered)) {
            auto& obj = ctx.state.getObject(countered);
            ctx.events.logTrace("COUNTER: " + obj.name + " countered by Defy -> trash");
            obj.zone = ZoneType::Trash;
            obj.location = std::nullopt;
            ctx.state.player(obj.owner).trash.push_back(countered);
        }
        // NOTE: removed the previously-present (un-printed) "draw 1".
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 45;
        d.def_id = R"RB(ogn-045-298)RB";
        d.name = R"RB(Defy)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-045/298)RB";
        d.collector_number = 45;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Counter a spell that costs no more than [4] and no more than [A].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4989bfcc4bd7be77051f0c2c349a981ba9c273e0-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_45(CardRegistry& r) {
    r.registerCard(45, std::make_unique<Defy>());
}

} // namespace riftbound
