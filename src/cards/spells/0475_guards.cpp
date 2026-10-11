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
    //
    // The token is created once per execution: its id is kept on the resolving
    // chain item (resume_data[kTokenSlot], clear of the slots the choice
    // helpers use), not on the card, so a later cast of the same card object
    // makes a new token.
    static constexpr size_t kTokenSlot = 7;
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // No location is printed, so the token is played like any unit: base
        // or a battlefield you control — the battlefield this was hidden at
        // when played from facedown (CR 811.1.d.5).
        const auto where = playLocations(ctx.state, ctx.controller,
                                         facedownPlayBattlefield(ctx.state, ctx.source));
        int mode = pickMode(ctx, "Guards!: where to play the Sand Soldier",
                            where.count(), where.labels);
        if (mode < 0 || mode >= where.count()) return;   // -1 waiting for the agent

        GameObjectId tok = kInvalidId;
        auto& ri = ctx.state.chain.resuming;
        if (ri.has_value() && ri->resume_data.size() > kTokenSlot &&
            ri->resume_data[kTokenSlot] != 0) {
            tok = static_cast<GameObjectId>(ri->resume_data[kTokenSlot] - 1);
        } else {
            // CR 187.3: a Sand Soldier token has 2 Might and the Shurima tag.
            tok = ctx.executor.createToken(ctx.controller, CardType::Unit,
                "Sand Soldier", 2, {"Shurima"}, KeywordSet{},
                where.locations[mode], /*enter_ready=*/false);
            if (ri.has_value()) {
                while (ri->resume_data.size() <= kTokenSlot) ri->resume_data.push_back(0);
                ri->resume_data[kTokenSlot] = static_cast<int32_t>(tok) + 1;
            }
            ctx.events.logTrace("GUARDS!: play 2[M] Sand Soldier token");
        }
        auto can_pay = [&]() {
            if (!ctx.state.objectExists(tok)) return false;
            const auto& t = ctx.state.getObject(tok);
            return t.location.has_value() && t.is_exhausted &&
                   payableOnePower(ctx.state, ctx.controller, Domain::Order);
        };
        int conf = confirmOptional(ctx, "Guards!: pay [Y] to ready the Sand Soldier?", can_pay);
        if (conf <= 0) return;   // waiting on the agent, declined, or not possible
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
