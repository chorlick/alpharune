#include "cards/card.h"
#include "cards/card_helpers.h"
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

class DecreeOfStrength : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose an opponent. They reveal their hand and you choose a Mind ([B])
    //  card from it. They recycle that card."
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId opp = opponent(ctx.controller);
        auto& hand = ctx.state.player(opp).hand;
        // Reveal once — not again when re-entered with the agent's pick.
        const bool first_pass = !ctx.state.chain.resuming.has_value() ||
                                ctx.state.chain.resuming->resume_point < 6;
        std::vector<GameObjectId> mind_cards;
        for (auto cid : hand) {
            if (!ctx.state.objectExists(cid)) continue;
            const auto& obj = ctx.state.getObject(cid);
            if (first_pass) {
                ctx.events.emit(CardRevealedEvent{
                    cid, obj.card_def_id, obj.owner,
                    /*revealed_to_all=*/true /* CR 425.1: to all players */,
                    PlayerId::None, ZoneType::Hand});
                RIFT_TRACE(ctx.events, "DECREE OF STRENGTH: revealed " + obj.name);
            }
            if (std::find(obj.domains.begin(), obj.domains.end(), Domain::Mind) != obj.domains.end())
                mind_cards.push_back(cid);
        }
        // At resolution: a card in a hidden zone is not a target (CR 355.10.a; batch M1).
        GameObjectId picked = pickUnseenCard(ctx, "Decree of Strength: recycle a Mind card", mind_cards);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto it = std::find(hand.begin(), hand.end(), picked);
        if (it == hand.end()) return;
        hand.erase(it);
        // Chosen from the revealed hand: both players know which card it is.
        ctx.executor.recycleCards(opp, {picked}, knowledge::everyone());
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 872;
        d.def_id = R"RB(ven-085-166)RB";
        d.name = R"RB(Decree of Strength)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-085/166)RB";
        d.collector_number = 85;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Choose an opponent. They reveal their hand and you choose a Mind ([B]) card from it. They recycle that card.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/12106889f9f76914421f38e8958a248462637a22-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_872(CardRegistry& r) {
    r.registerCard(872, std::make_unique<DecreeOfStrength>());
}

}  // namespace riftbound
