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

class CardSharp : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "you and each opponent may play a Gold gear token exhausted. For each
    // opponent who did, you play a Gold gear token exhausted." Each player
    // decides for themselves: first the controller, then the opponent.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        const int mine = confirmOptional(ctx, "Card Sharp: play a Gold gear token exhausted?",
                                         []() { return true; });
        if (mine == -1) return;   // waiting for the controller

        // The opponent's answer, past the choice helpers' own resume_data
        // slots: 0 = not asked yet, 1 = asked, 2 = declined, 3 = accepted.
        constexpr size_t kOppSlot = Card::kChoiceStateSlot + 1;
        if (ri.resume_data.size() <= kOppSlot) ri.resume_data.resize(kOppSlot + 1, 0);
        const PlayerId opp = opponent(ctx.controller);
        if (ri.resume_data[kOppSlot] == 0) {
            ctx.executor.requestChoice(opp,
                {valChoice(opp, 0, "Card Sharp: no Gold"),
                 valChoice(opp, 1, "Card Sharp: play a Gold gear token exhausted")},
                "Card Sharp: play a Gold gear token exhausted? (your opponent then plays one too)");
            ri.resume_data[kOppSlot] = 1;
            return;
        }
        if (ri.resume_data[kOppSlot] == 1)
            ri.resume_data[kOppSlot] = takenValue(ctx, 0) == 1 ? 3 : 2;
        const bool theirs = ri.resume_data[kOppSlot] == 3;

        if (mine == 1) createGoldExhausted(ctx);
        if (theirs) {
            LocationId loc{BaseLocation{opp}};
            auto tok = ctx.executor.createToken(opp, CardType::Gear, "Gold",
                                                0, {}, {}, loc, /*enter_ready=*/false, {},
                                                // CR 187.5: the registered Gold card
                                                kGoldGearCardDefId,
                                                /*enters_exhausted=*/true);
            (void)tok;
            // "For each opponent who did, you play a Gold gear token exhausted."
            createGoldExhausted(ctx);
        }
        RIFT_TRACE(ctx.events, std::string("CARD SHARP: Gold tokens (you ") +
                            (mine == 1 ? "yes" : "no") + ", opponent " +
                            (theirs ? "yes -> you play one more" : "no") + ")");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 403;
        d.def_id = R"RB(sfd-081-221)RB";
        d.name = R"RB(Card Sharp)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-081/221)RB";
        d.collector_number = 81;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, you and each opponent may play a Gold gear token exhausted. For each opponent who did, you play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8bfd5dc22473d439cd78d5a3cb23ed03de66953d-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_403(CardRegistry& r) {
    r.registerCard(403, std::make_unique<CardSharp>());
}

} // namespace riftbound
