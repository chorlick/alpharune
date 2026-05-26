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

class Guards : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Play a 2 [M] Sand Soldier unit token. Then do this: You may pay [C] to
    //  ready it." [C] = one Power of this card's Domain (Order/[Y]).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        // Create the token once (survives the confirmOptional resume loop).
        if (!self.card_counters.count("__guards_token")) {
            auto tok = ctx.executor.createToken(ctx.controller, CardType::Unit,
                "Sand Soldier", 2, {"Sand Soldier"}, KeywordSet{},
                LocationId{BaseLocation{ctx.controller}}, /*enter_ready=*/false);
            self.card_counters["__guards_token"] = static_cast<int>(tok);
            ctx.events.logTrace("GUARDS!: play 2[M] Sand Soldier token");
        }
        GameObjectId tok = static_cast<GameObjectId>(self.card_counters["__guards_token"]);
        auto can_pay = [&]() {
            return ctx.state.objectExists(tok) &&
                   ctx.state.getObject(tok).is_exhausted;
        };
        int conf = confirmOptional(ctx, "Guards!: pay [Y] to ready the Sand Soldier?", can_pay);
        if (conf == -1) return;  // waiting on agent
        if (conf == 0) return;   // declined / not legal
        if (!payOnePower(ctx, ctx.controller, Domain::Order)) return;
        ctx.executor.readyObject(tok);
        ctx.events.logTrace("GUARDS!: paid [Y] -> readied Sand Soldier");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 475;
        d.def_id = R"RB(sfd-154-221)RB";
        d.name = R"RB(Guards!)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-154/221)RB";
        d.collector_number = 154;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 3;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
Play a 2 [M] Sand Soldier unit token. Then do this: You may pay [C] to ready it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1dc4fef81d018c6cf14f0344b51ebcdec968aab9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_475(CardRegistry& r) {
    r.registerCard(475, std::make_unique<Guards>());
}

} // namespace riftbound
