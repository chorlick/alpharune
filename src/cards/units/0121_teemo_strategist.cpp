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

class TeemoStrategist : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "choose an enemy unit here" — enemy units at my battlefield, minus
        // those that can't be chosen by enemy spells and abilities.
        std::vector<GameObjectId> legal;
        if (const auto my_bf = ctx.state.getObject(ctx.source).battlefieldId())
            legal = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.controller != ctx.controller && o.battlefieldId() == my_bf;
            });
        std::sort(legal.begin(), legal.end());
        GameObjectId picked = pickTarget(ctx, "Teemo, Strategist", legal);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        // Still waiting for the agent (the pick was not made at finalization).
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) return;
        // With no unit to choose, or one that is gone by now, the reveal and
        // the recycle still happen (CR 359.3.e: only the instruction that
        // needs the missing unit is skipped).
        // Reveal the top 5 of own Main Deck (top = back of vector).
        auto& ps = ctx.state.player(ctx.controller);
        int n = std::min(5, static_cast<int>(ps.main_deck.size()));
        std::vector<GameObjectId> revealed;
        for (int i = 0; i < n; ++i) {
            // "Reveal": both players see them.
            auto cid = deck::takeTop(ctx.state, ctx.controller, deck::Place::Held, knowledge::everyone());
            revealed.push_back(cid);
        }
        int hidden_count = 0;
        for (auto cid : revealed) {
            if (!ctx.state.objectExists(cid)) continue;
            const auto& c = ctx.state.getObject(cid);
            // Private reveal to the controller.
            ctx.events.emit(CardRevealedEvent{
                /*card=*/cid,
                /*card_def_id=*/c.card_def_id,
                /*owner=*/c.owner,
                /*revealed_to_all=*/false,
                /*revealed_to=*/ctx.controller,
                /*source_zone=*/ZoneType::MainDeck,
            });
            if (c.hasKeyword(Keyword::Hidden)) ++hidden_count;
        }
        ctx.events.logTrace("TEEMO, STRATEGIST: revealed " + std::to_string(n) +
                             " — " + std::to_string(hidden_count) + " Hidden");

        // Deal 1 for each Hidden card revealed.
        if (hidden_count > 0 && picked != kInvalidId &&
            ctx.state.getObject(picked).location.has_value()) {
            ctx.executor.dealDamage(picked, hidden_count, ctx.source);
            if (ctx.state.getObject(picked).location.has_value() &&
                ctx.state.getObject(picked).hasLethalDamage()) {
                ctx.executor.killObject(picked);
            }
        }

        // Recycle the revealed cards (to bottom of deck).
        if (!revealed.empty()) {
            ctx.executor.recycleCards(ctx.controller, revealed);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 121;
        d.def_id = R"RB(ogn-121-298)RB";
        d.name = R"RB(Teemo, Strategist)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-121/298)RB";
        d.collector_number = 121;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Yordle)RB", R"RB(Teemo)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
When I defend, choose an enemy unit here and reveal the top 5 cards of your Main Deck. Deal 1 to that unit for each card with [Hidden] revealed this way, then recycle the revealed cards.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b05f31bf744972983f61a9f5801b4ffd68fb9ebf-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_121(CardRegistry& r) {
    r.registerCard(121, std::make_unique<TeemoStrategist>());
}

} // namespace riftbound
