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

class PartyFavors : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Each other player chooses Cards or Runes. For each player that chooses
    //  Cards, you and that player each draw 1. For each player that chooses
    //  Runes, you and that player each channel 1 rune exhausted."
    // 1v1: the single other player is the opponent. They choose (resumable).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        PlayerId opp = opponent(ctx.controller);
        switch (ri.resume_point) {
        case 0: {
            // The two answers are int-coded (0 = Cards, 1 = Runes) so each has
            // its own action-vocab slot.
            // At resolution: another player's choice (CR 355.10.e; batch M1).
            ctx.executor.requestChoice(opp,
                {valChoice(opp, 0, "Cards"), valChoice(opp, 1, "Runes")},
                "Party Favors: choose Cards or Runes");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            ri.resume_point = 2;
            const bool chose_runes = takenValue(ctx, 0) == 1;
            if (chose_runes) {
                ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
                ctx.executor.channelRunes(opp, 1, /*enter_exhausted=*/true);
                RIFT_TRACE(ctx.events, "PARTY FAVORS: opponent chose Runes -> both channel 1 exhausted");
            } else {
                ctx.executor.drawCards(ctx.controller, 1);
                ctx.executor.drawCards(opp, 1);
                RIFT_TRACE(ctx.events, "PARTY FAVORS: opponent chose Cards -> both draw 1");
            }
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 71;
        d.def_id = R"RB(ogn-071-298)RB";
        d.name = R"RB(Party Favors)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-071/298)RB";
        d.collector_number = 71;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Each other player chooses Cards or Runes. For each player that chooses Cards, you and that player each draw 1. For each player that chooses Runes, you and that player each channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fddcef3c55663c5d6856f6d039d19cacfd64abb5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_71(CardRegistry& r) {
    r.registerCard(71, std::make_unique<PartyFavors>());
}

} // namespace riftbound
