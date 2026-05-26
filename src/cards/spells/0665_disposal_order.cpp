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

class DisposalOrder : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);
        bool opp_trash_has_cards = !ctx.state.player(opp).trash.empty();
        // If the opponent's trash is empty, mode 0 is illegal; only mode 1.
        uint32_t legal = opp_trash_has_cards ? 0b11u : 0b10u;

        static const std::vector<std::string> kModes = {
            "Recycle up to 3 from opponent's trash", "Draw 1"};
        int mode = pickMode(ctx, "Disposal Order: choose one", /*num_modes=*/2,
                            kModes, legal);
        if (mode == -1) return;       // waiting on agent
        if (mode == -2) {             // no legal mode (shouldn't happen — draw always legal)
            ctx.executor.drawCards(ctx.controller, 1);
            return;
        }

        if (mode == 0) {
            // Recycle up to 3 cards from the opponent's trash. The owner (the
            // opponent) recycles them. We take the first up-to-3 cards from
            // their trash (the agent-choice of WHICH cards is not surfaced —
            // documented approximation: take the most recent up to 3).
            auto& opp_ps = ctx.state.player(opp);
            std::vector<GameObjectId> to_recycle;
            for (auto it = opp_ps.trash.rbegin();
                 it != opp_ps.trash.rend() && (int)to_recycle.size() < 3; ++it) {
                to_recycle.push_back(*it);
            }
            if (!to_recycle.empty()) {
                ctx.executor.recycleCards(opp, to_recycle);
                ctx.events.logTrace("DISPOSAL ORDER: opponent recycles " +
                                    std::to_string(to_recycle.size()) +
                                    " card(s) from trash");
            }
        } else {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("DISPOSAL ORDER: draw 1");
        }
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
