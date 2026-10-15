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

// "When I attack, you may reveal the top 2 cards of your Main Deck. You may
//  banish one, then play it. If it is a unit, you may play it here. Recycle the
//  rest." The cost of the played card is not waived.
//
// The first "you may" opens the effect: it is decided as the ability is
// finalized (CR 383.3.a). The later ones depend on the cards revealed and are
// decided as it resolves (CR 383.3.a.3).

class RekSaiSwarmQueen : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        // "you may reveal the top 2 cards of your Main Deck"
        const int go = confirmOptional(ctx, "Rek'Sai: reveal the top 2 cards of your Main Deck?",
                                       [&]() { return !ps.main_deck.empty(); });
        if (go == -1) return;                 // waiting
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (go != 1) return;                  // declined / empty deck

        // Progress of the reveal, past the choice helpers' own resume_data
        // slots: 0 = not revealed yet, 1 = waiting for the pick, 3 = a card
        // was banished and is being played, 2 = done. [kPickedSlot] is that
        // card, [kRestSlot] the other revealed card (kInvalidId if none).
        constexpr size_t kStepSlot = Card::kChoiceStateSlot + 1;
        constexpr size_t kPickedSlot = kStepSlot + 1;
        constexpr size_t kRestSlot = kStepSlot + 2;
        if (ri.resume_data.size() <= kRestSlot) ri.resume_data.resize(kRestSlot + 1, 0);
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
                Intent c; c.type = IntentType::MakeChoice; c.player = ctx.controller;
                c.chosen_objects = {cid}; choices.push_back(std::move(c));
            }
            if (choices.empty()) { ri.resume_data[kStepSlot] = 2; return; }
            // "you may banish one" — add a decline option: no object, value 0,
            // so an agent can name it (the cards are told apart by object).
            choices.push_back(valChoice(ctx.controller, 0, "Rek'Sai: banish none"));
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "Rek'Sai: banish & play one of top 2 (or decline)", PromptKind::ChooseObject);
            ri.resume_data[kStepSlot] = 1;
            return;
        }
        auto recycle = [&](GameObjectId cid) {
            if (cid == kInvalidId || !ctx.state.objectExists(cid)) return;
            if (deck::take(ctx.state, cid, deck::Place::Held, knowledge::everyone()))
                ctx.executor.recycleCards(ctx.controller, {cid});
        };
        if (ri.resume_data[kStepSlot] == 1) {
            auto choice = ctx.executor.takeChoice();
            GameObjectId picked = kInvalidId;
            if (choice && !choice->chosen_objects.empty()) picked = choice->chosen_objects[0];
            int n = std::min(2, static_cast<int>(ps.main_deck.size()));
            std::vector<GameObjectId> top;
            for (int i = 0; i < n; ++i) top.push_back(ps.main_deck[ps.main_deck.size() - 1 - i]);
            if (std::find(top.begin(), top.end(), picked) == top.end()) picked = kInvalidId;
            if (picked == kInvalidId) {
                // Nothing banished: recycle both.
                ri.resume_data[kStepSlot] = 2;
                std::vector<GameObjectId> rest;
                for (auto cid : top)
                    if (deck::take(ctx.state, cid, deck::Place::Held, knowledge::everyone()))
                        rest.push_back(cid);
                if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
                RIFT_TRACE(ctx.events, "REK'SAI: revealed top 2, banished none, recycled them");
                return;
            }
            GameObjectId other = kInvalidId;
            for (auto cid : top) if (cid != picked) other = cid;
            // Through the executor: it counts the banish and announces it
            // ("When you banish a card you own").
            ctx.executor.banishObject(picked);
            ri.resume_data[kPickedSlot] = static_cast<int32_t>(picked.raw());
            ri.resume_data[kRestSlot] = static_cast<int32_t>(other.raw());
            ri.resume_data[kStepSlot] = 3;
        }

        // "then play it". The cost is not waived (CR 356.1.b.2): unpaid, it
        // stays banished. playPayingCost takes it out of banishment itself.
        const auto picked = GameObjectId::fromRaw(ri.resume_data[kPickedSlot]);
        const auto other = GameObjectId::fromRaw(ri.resume_data[kRestSlot]);
        if (ctx.state.objectExists(picked) &&
            ctx.executor.canPlayPayingCost(ctx.controller, picked)) {
            std::optional<LocationId> loc;
            if (ctx.state.getObject(picked).isUnit()) {
                // A unit is played to its controller's base or a battlefield
                // they control (CR 355.2.a); "If it is a unit, you may play it
                // here" adds my battlefield to those (CR 355.2.b) — a
                // permission, not an instruction, so the controller chooses.
                // Mode 0 = base, mode 1 + i = battlefield slot i, the numbering
                // of pickEffectPlayLocation. Never a battlefield units can't
                // be played to, nor any battlefield for a player who may only
                // play units to base.
                const auto& bfs = ctx.state.battlefields;
                const auto here = ctx.state.objectExists(ctx.source)
                    ? ctx.state.getObject(ctx.source).battlefieldId() : std::nullopt;
                uint32_t mask = 1u;
                std::vector<std::string> labels{"Base"};
                for (size_t i = 0; i < bfs.size() && i < 31; ++i) {
                    labels.push_back("Battlefield #" + toString(bfs[i].id));
                    if (!battlefieldTakesUnitsFrom(ctx.state, ctx.controller, bfs[i])) continue;
                    // Where it may be played anyway (its controller's, or the
                    // card's own permissions: CR 355.2, 420.3.b), or "here".
                    const bool allowed = effectPlayAllowedAt(ctx, ctx.controller, picked, bfs[i]);
                    if (allowed || (here.has_value() && *here == bfs[i].id)) mask |= 1u << (i + 1);
                }
                int mode = 0;
                if (mask != 1u) {
                    mode = pickMode(ctx, "Rek'Sai: play it to",
                                    static_cast<int>(labels.size()), labels, mask,
                                    std::source_location::current(), PromptKind::Location);
                    if (mode == -1) return;   // waiting for the agent
                }
                if (mode >= 1 && static_cast<size_t>(mode) <= bfs.size() &&
                    (mask & (1u << mode)))
                    loc = LocationId{BattlefieldLocation{bfs[mode - 1].id}};
                else
                    loc = LocationId{BaseLocation{ctx.controller}};
            }
            ctx.executor.playPayingCost(ctx.controller, picked, loc);
        }
        ri.resume_data[kStepSlot] = 2;
        // Recycle the rest.
        recycle(other);
        RIFT_TRACE(ctx.events, "REK'SAI: revealed top 2, banished+played one, recycled rest");
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
