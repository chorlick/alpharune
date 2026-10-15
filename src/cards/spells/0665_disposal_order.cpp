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

class DisposalOrder : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose one — Choose up to 3 cards from opponents' trashes. Their owners
    //  recycle them. / Draw 1."
    // The mode (CR 355.3) and the cards — targets in a public zone, up to 3,
    // zero allowed (CR 355.10.a.1, 355.13) — are chosen as the spell is
    // played, in its finalize pass, before its cost. One card per prompt,
    // each with "Done". A chosen card that has left that trash as it resolves
    // is a mistarget; the others are still recycled.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;   // cards in a trash, not units
    }
    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId opp = opponent(ctx.controller);
        // Both modes are always legal: "up to 3 cards" includes none (CR
        // 355.13), so the first can be chosen with an empty opposing trash
        // (it then does nothing).
        // (judge ruling 2026-10-11: confirmed — the "up to 3" mode may be chosen with zero cards; plans/09 9.W2.)
        static const std::vector<std::string> kModes = {
            "Recycle up to 3 from opponent's trash", "Draw 1"};
        const int mode = pickMode(ctx, "Disposal Order: choose one", /*num_modes=*/2,
                                  kModes, 0b11u);
        if (mode == -1) return;       // waiting on agent
        std::vector<GameObjectId> cards;
        if (mode == 0) {
            auto in_trash = [&]() {
                const auto& t = ctx.state.player(opp).trash;
                std::vector<GameObjectId> out;
                for (auto cid : t) if (ctx.state.objectExists(cid)) out.push_back(cid);
                return out;
            };
            const auto picks = selectSeveralTargets(*this, ctx,
                "Disposal Order: choose a card from the opponent's trash (up to 3)",
                in_trash(), /*max_picks=*/3);
            if (!picks.has_value()) return;   // waiting on agent
            cards = *picks;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (mode != 0) {              // "Draw 1"
            ctx.executor.drawCards(ctx.controller, 1);
            RIFT_TRACE(ctx.events, "DISPOSAL ORDER: draw 1");
            return;
        }
        // selectSeveralTargets has dropped the cards no longer in that trash.
        if (cards.empty()) return;
        // "Their owners recycle them."
        ctx.executor.recycleCards(opp, cards);
        RIFT_TRACE(ctx.events, "DISPOSAL ORDER: opponent recycles " +
                            std::to_string(cards.size()) + " card(s) from trash");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 665;
        d.def_id = R"RB(unl-103-219)RB";
        d.name = R"RB(Disposal Order)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-103/219)RB";
        d.collector_number = 103;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose one —
Choose up to 3 cards from opponents' trashes. Their owners recycle them.Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/de7b7c683f16a297418bc9a326178c520f17ef2b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_665(CardRegistry& r) {
    r.registerCard(665, std::make_unique<DisposalOrder>());
}

} // namespace riftbound
