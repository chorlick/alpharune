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

    // "Counter a spell that costs no more than [4] and no more than [A]."
    // Both caps are on the spell's printed cost: at most 4 Energy and at most
    // one Power. Defy counters the item directly beneath it, so that item is
    // what has to qualify.
    static bool counterable(const GameState& state, const CardDB& db) {
        if (state.chain.items.empty()) return false;
        const auto& top = state.chain.items.back();
        if (!top.is_spell || top.card_def_id == kInvalidId) return false;
        const auto& d = db.get(top.card_def_id);
        return d.energy_cost <= 4 && d.power_cost <= 1;
    }
    // Not playable unless the spell it would counter qualifies (the printed
    // costs come from the card table the engine hands the gate). Checked
    // again as Defy resolves: the chain may have changed.
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/,
                         const CardDB& db) const override {
        return counterable(state, db);
    }
    // Without the card table only the half the state shows can be answered:
    // there must be a spell directly beneath.
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        return !state.chain.items.empty() && state.chain.items.back().is_spell;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.chain.items.empty()) return;
        auto& top = ctx.state.chain.items.back();
        if (!top.is_spell) return;
        if (isUncounterable(ctx, top)) return;

        if (!counterable(ctx.state, ctx.executor.cardDB())) {
            ctx.events.logTrace("DEFY: cannot counter — the spell costs more than [4] "
                                "or more than [A]");
            return;  // illegal target; spell resolves normally
        }

        auto countered = top.source;
        revertCounteredPlay(ctx, top);  // CR 425.1.b
        ctx.state.chain.items.pop_back();
        if (ctx.state.objectExists(countered)) {
            auto& obj = ctx.state.getObject(countered);
            ctx.events.logTrace("COUNTER: " + obj.name + " countered by Defy -> trash");
            disposeCounteredCard(ctx, countered);
        }
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
