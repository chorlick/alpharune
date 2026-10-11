#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "engine/deck_ops.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "When a friendly unit dies, you may exhaust me to draw 1, then put a card
//  from your hand on the top or bottom of your Main Deck."

class AltarOfMemories : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // The "may" and the exhaust (the cost) are decided as the ability is
    // finalized (CR 383.3). The card to put back is chosen as it resolves,
    // after the draw: it may be the card just drawn.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyUnitDies; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "you may exhaust me": the Altar must be on the board and ready.
        auto still_legal = [&]() {
            if (!ctx.state.objectExists(ctx.source)) return false;
            const auto& self = ctx.state.getObject(ctx.source);
            return self.location.has_value() && !self.is_exhausted;
        };
        const int conf = confirmOptional(ctx, "Altar of Memories: exhaust me to draw 1?", still_legal);
        if (conf == 1 && costDueNow(ctx)) ctx.executor.exhaustObject(ctx.source);
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (conf != 1) return;                // waiting on the agent / declined / not legal

        // Draw 1 — once per resolution. The two picks below suspend and
        // re-enter this callback from the top; the guard lives on the
        // resolving chain item, so every trigger gets its own draw.
        if (ctx.state.chain.resuming.has_value()) {
            auto& rd = ctx.state.chain.resuming->resume_data;
            while (rd.size() <= kDrewSlot) rd.push_back(0);
            if (rd[kDrewSlot] == 0) {
                rd[kDrewSlot] = 1;
                ctx.executor.drawCards(ctx.controller, 1);
                ctx.events.logTrace("ALTAR OF MEMORIES: exhausted -> draw 1");
            }
        } else {
            ctx.executor.drawCards(ctx.controller, 1);
        }
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.hand.empty()) return;
        // Choose a hand card to put on top or bottom of the Main Deck.
        std::vector<GameObjectId> hand(ps.hand.begin(), ps.hand.end());
        GameObjectId card = pickTarget(ctx, "Altar: put which hand card onto deck?", hand);
        if (card == kInvalidId) return;  // suspend or empty
        if (std::find(ps.hand.begin(), ps.hand.end(), card) == ps.hand.end()) return;
        int mode = pickMode(ctx, "Altar: top or bottom of deck?", 2, {"top", "bottom"});
        if (mode < 0) return;   // suspend / no legal mode
        ps.hand.erase(std::find(ps.hand.begin(), ps.hand.end(), card));
        // The owner knows where it went; the opponent sees only that a hand card did.
        if (mode == 0) deck::putOnTop(ctx.state, ctx.controller, card, deck::Place::Hand);
        else           deck::putOnBottom(ctx.state, ctx.controller, card, deck::Place::Hand);
        ctx.events.logTrace(std::string("ALTAR OF MEMORIES: put a hand card on ") +
                            (mode == 0 ? "top" : "bottom") + " of deck");
    }
private:
    static constexpr size_t kDrewSlot = 7;   // resume_data slots 0..6 belong to the choice helpers
    const CardDef def_ = [] {
        CardDef d;
        d.id = 490;
        d.def_id = R"RB(sfd-169-221)RB";
        d.name = R"RB(Altar of Memories)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-169/221)RB";
        d.collector_number = 169;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When a friendly unit dies, you may exhaust me to draw 1, then put a card from your hand on the top or bottom of your Main Deck.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3ddc94a035d793d48987c6fe5d65fa11ea25c808-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_490(CardRegistry& r) {
    r.registerCard(490, std::make_unique<AltarOfMemories>());
}

} // namespace riftbound
