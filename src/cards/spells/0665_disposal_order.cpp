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
    // The cards are picked one at a time (each prompt has a "Done" option).
    // Progress, clear of the Card::pick* helpers' slots and resume points:
    // resume_point kAwaitCard while a card is being asked, kDone at the end;
    // resume_data[kCountSlot] = cards picked so far, followed by their ids.
    static constexpr int kAwaitCard = 30;
    static constexpr int kDone = 31;
    static constexpr size_t kCountSlot = 7;

    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);
        auto& ri_opt = ctx.state.chain.resuming;
        if (ri_opt.has_value() && ri_opt->resume_point == kDone) return;
        // Both modes are always legal: "up to 3 cards" includes none (CR
        // 355.13), so the first can be chosen with an empty opposing trash
        // (it then does nothing).
        // (judge ruling 2026-10-11: confirmed — the "up to 3" mode may be chosen with zero cards; plans/09 9.W2.)
        static const std::vector<std::string> kModes = {
            "Recycle up to 3 from opponent's trash", "Draw 1"};
        int mode = pickMode(ctx, "Disposal Order: choose one", /*num_modes=*/2,
                            kModes, 0b11u);
        if (mode == -1) return;       // waiting on agent
        if (mode != 0) {              // "Draw 1"
            ctx.executor.drawCards(ctx.controller, 1);
            RIFT_TRACE(ctx.events, "DISPOSAL ORDER: draw 1");
            return;
        }

        auto& opp_ps = ctx.state.player(opp);
        if (!ri_opt.has_value()) {
            // Direct invocation (no chain item to resume): no agent can be
            // asked, so take the most recent up to 3.
            std::vector<GameObjectId> to_recycle;
            for (auto it = opp_ps.trash.rbegin();
                 it != opp_ps.trash.rend() && to_recycle.size() < 3; ++it)
                to_recycle.push_back(*it);
            if (!to_recycle.empty()) ctx.executor.recycleCards(opp, to_recycle);
            return;
        }
        auto& ri = *ri_opt;
        while (ri.resume_data.size() <= kCountSlot) ri.resume_data.push_back(0);
        // The ids picked so far follow the count. Read by index: the loop above
        // guarantees size() > kCountSlot, which the compiler could not see
        // through an iterator-range constructor (GCC 13 -Wfree-nonheap-object,
        // a false positive: the range was never negative).
        auto picked = [&]() {
            std::vector<GameObjectId> out;
            for (size_t i = kCountSlot + 1; i < ri.resume_data.size(); ++i)
                out.push_back(GameObjectId::fromRaw(ri.resume_data[i]));
            return out;
        };
        auto candidates = [&]() {
            const auto have = picked();
            std::vector<GameObjectId> out;
            for (auto cid : opp_ps.trash)
                if (ctx.state.objectExists(cid) &&
                    std::find(have.begin(), have.end(), cid) == have.end())
                    out.push_back(cid);
            return out;
        };
        auto finish = [&]() {
            ri.resume_point = kDone;
            const auto to_recycle = picked();
            if (to_recycle.empty()) return;
            // "Their owners recycle them."
            ctx.executor.recycleCards(opp, to_recycle);
            RIFT_TRACE(ctx.events, "DISPOSAL ORDER: opponent recycles " +
                                std::to_string(to_recycle.size()) + " card(s) from trash");
        };
        if (ri.resume_point == kAwaitCard) {
            const GameObjectId card = takenObject(ctx);
            const auto legal_cards = candidates();
            if (card == kInvalidId ||
                std::find(legal_cards.begin(), legal_cards.end(), card) == legal_cards.end()) {
                finish();   // "Done"
                return;
            }
            ri.resume_data.push_back(static_cast<int32_t>(card.raw()));
            ++ri.resume_data[kCountSlot];
        }
        const auto cands = candidates();
        if (ri.resume_data[kCountSlot] >= 3 || cands.empty()) { finish(); return; }
        std::vector<Intent> opts;
        for (auto cid : cands) opts.push_back(objChoice(ctx.controller, cid));
        opts.push_back(valChoice(ctx.controller, 0, "Done"));
        ctx.executor.requestChoice(ctx.controller, std::move(opts),
            "Disposal Order: choose a card from the opponent's trash (" +
            std::to_string(ri.resume_data[kCountSlot] + 1) + " of up to 3)", PromptKind::ChooseObject);
        ri.resume_point = kAwaitCard;
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
