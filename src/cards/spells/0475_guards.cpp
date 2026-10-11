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
        const auto hidden_at = facedownPlayBattlefield(ctx.state, ctx.source);
        const auto where = playLocations(ctx.state, ctx.controller, hidden_at);
        // playLocations leaves out a battlefield units can't be played to
        // (Rockfall Path; a player who may only play units to base). Played
        // from facedown that battlefield is the only location (CR 811.1.d:
        // the choice is "restricted to the battlefield where it was hidden"),
        // and "Units can't be played here." applies to it too (CR 002,
        // 054.1): two restrictions that contradict leave no location, so the
        // unit is not played at all (Vendetta FAQ, Perched Grimwyrm at
        // Rockfall Path) and the rest of the text has no "it" to ready (CR 055).
        if (where.count() == 0) return;                  // nowhere to play it
        int mode = pickMode(ctx, "Guards!: where to play the Sand Soldier",
                            where.count(), where.labels);
        if (mode < 0 || mode >= where.count()) return;   // waiting for the agent

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
        // "Then do this: You may pay [C] to ready it." — a reflexive trigger
        // (CR 388): with the token played it goes on the chain as an item of
        // its own; the "may" and the [C] are settled as that item is
        // finalized (CR 388.4).
        if (ri.has_value() && ri->resume_data.size() > kTokenSlot + 1 &&
            ri->resume_data[kTokenSlot + 1] != 0)
            return;                                   // already queued
        if (ri.has_value()) {
            while (ri->resume_data.size() <= kTokenSlot + 1) ri->resume_data.push_back(0);
            ri->resume_data[kTokenSlot + 1] = 1;
        }
        ctx.executor.queueReflexiveTrigger(ctx.source, ctx.controller, 0,
                                           {static_cast<int32_t>(tok)});
    }
    bool hasReflexiveTrigger() const override { return true; }
    // `data[0]`: the Sand Soldier token. [C] = one Power of this card's
    // Domain (Order).
    void onReflexiveTrigger(CardContext& ctx, int /*index*/,
                            const std::vector<int32_t>& data) override {
        if (data.empty()) return;
        const auto tok = static_cast<GameObjectId>(data[0]);
        auto can_ready = [&]() {
            if (!ctx.state.objectExists(tok)) return false;
            const auto& t = ctx.state.getObject(tok);
            return t.location.has_value() && t.is_exhausted;
        };
        auto can_pay = [&]() {
            return costAlreadyPaid(ctx) ||
                   (can_ready() && payableOnePower(ctx.state, ctx.controller, Domain::Order));
        };
        int conf = confirmOptional(ctx, "Guards!: pay [Y] to ready the Sand Soldier?", can_pay);
        if (conf == 1 && costDueNow(ctx) && !payOnePower(ctx, ctx.controller, Domain::Order))
            return;
        if (finalizingChoices(ctx)) return;   // the "may" and the cost end here (CR 388.4)
        if (conf < 1) return;                 // declined, or could not pay
        if (!ctx.state.objectExists(tok) || !ctx.state.getObject(tok).location.has_value())
            return;                           // the token left in response
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
