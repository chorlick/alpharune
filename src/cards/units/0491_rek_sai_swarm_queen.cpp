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

// "When I attack, you may reveal the top 2 cards of your Main Deck. You may
//  banish one, then play it. If it is a unit, you may play it here. Recycle the
//  rest." Played for free (no partial-cost path).

class RekSaiSwarmQueen : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        // "you may reveal the top 2 cards of your Main Deck"
        const int go = confirmOptional(ctx, "Rek'Sai: reveal the top 2 cards of your Main Deck?",
                                       [&]() { return !ps.main_deck.empty(); });
        if (go != 1) return;   // waiting / declined / empty deck

        // Progress of the reveal, past the choice helpers' own resume_data
        // slots: 0 = not revealed yet, 1 = waiting for the pick, 2 = done.
        constexpr size_t kStepSlot = Card::kChoiceStateSlot + 1;
        if (ri.resume_data.size() <= kStepSlot) ri.resume_data.resize(kStepSlot + 1, 0);
        if (ri.resume_data[kStepSlot] == 2) return;
        if (ri.resume_data[kStepSlot] == 0) {
            // Void Hatchling: "If you would reveal cards from a deck, look at
            // the top card first."
            ctx.executor.lookBeforeReveal(ctx.controller);
            int n = std::min(2, static_cast<int>(ps.main_deck.size()));
            std::vector<Intent> choices;
            for (int i = 0; i < n; ++i) {
                GameObjectId cid = ps.main_deck[ps.main_deck.size() - 1 - i];
                if (!ctx.state.objectExists(cid)) continue;
                const auto& obj = ctx.state.getObject(cid);
                // "Reveal": both players see the top two.
                ctx.events.emit(CardRevealedEvent{cid, obj.card_def_id, obj.owner,
                                                  true, ctx.controller, ZoneType::MainDeck});
                knowledge::seenInDeck(ctx.state, cid, knowledge::everyone(), DeckEnd::Top, i);
                Intent c; c.type = IntentType::MakeChoice; c.player = ctx.controller;
                c.chosen_objects = {cid}; choices.push_back(std::move(c));
            }
            if (choices.empty()) { ri.resume_data[kStepSlot] = 2; return; }
            // "you may banish one" — add a decline option (empty chosen_objects).
            { Intent decline; decline.type = IntentType::MakeChoice;
              decline.player = ctx.controller; choices.push_back(std::move(decline)); }
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "Rek'Sai: banish & play one of top 2 (or decline)");
            ri.resume_data[kStepSlot] = 1;
            return;
        }
        {
            ri.resume_data[kStepSlot] = 2;
            auto choice = ctx.executor.takeChoice();
            GameObjectId picked = kInvalidId;
            if (choice && !choice->chosen_objects.empty()) picked = choice->chosen_objects[0];
            int n = std::min(2, static_cast<int>(ps.main_deck.size()));
            std::vector<GameObjectId> top;
            for (int i = 0; i < n; ++i) top.push_back(ps.main_deck[ps.main_deck.size() - 1 - i]);
            if (std::find(top.begin(), top.end(), picked) == top.end()) picked = kInvalidId;
            // Banish + play the picked card; if a unit, play it "here" (Rek'Sai's BF).
            if (picked != kInvalidId) {
                // Through the executor: it counts the banish and announces it
                // ("When you banish a card you own").
                ctx.executor.banishObject(picked);
                std::optional<LocationId> loc;
                if (ctx.state.getObject(picked).isUnit()) {
                    auto bf = ctx.state.getObject(ctx.source).battlefieldId();
                    if (bf) loc = LocationId{BattlefieldLocation{*bf}};  // "play it here"
                }
                // The cost is not waived (CR 356.1.b.2): unpaid, it stays
                // banished. playPayingCost takes it out of banishment itself.
                if (ctx.executor.canPlayPayingCost(ctx.controller, picked))
                    ctx.executor.playPayingCost(ctx.controller, picked, loc);
            }
            // Recycle the rest.
            std::vector<GameObjectId> rest;
            for (auto cid : top) {
                if (cid == picked) continue;
                deck::take(ctx.state, cid, deck::Place::Held, knowledge::everyone());
                rest.push_back(cid);
            }
            if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
            ctx.events.logTrace("REK'SAI: revealed top 2, banished+played one, recycled rest");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 491;
        d.def_id = R"RB(sfd-170-221)RB";
        d.name = R"RB(Rek'Sai, Swarm Queen)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-170/221)RB";
        d.collector_number = 170;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Rek'Sai)RB", R"RB(The Void)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I attack, you may reveal the top 2 cards of your Main Deck. You may banish one, then play it. If it is a unit, you may play it here. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5470aaad3bdb7ff5d2e605b07d93cedb7254c54d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_491(CardRegistry& r) {
    r.registerCard(491, std::make_unique<RekSaiSwarmQueen>());
}

} // namespace riftbound
