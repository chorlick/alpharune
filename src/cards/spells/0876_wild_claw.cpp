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

class WildClaw : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Look at the top 5 cards of your Main Deck. You may banish a unit or gear
    //  from among them and play it, reducing its Energy cost by [5]. Recycle
    //  the rest. Then you may do this: Empower it."
    //
    // The reduced cost is paid through the engine's cost path
    // (EffectExecutor::playPayingCost) when the card is put into play.
    static constexpr int kEnergyReduction = 5;

    static bool isBaseRune(const GameObject& o, PlayerId p) {
        return o.isRune() && o.controller == p && o.location.has_value() &&
               std::holds_alternative<BaseLocation>(*o.location);
    }
    static bool runePaysFor(const GameObject& rune, const CardDef& d) {
        if (d.domains.empty()) return true;
        for (auto rd : rune.domains)
            if (std::find(d.domains.begin(), d.domains.end(), rd) != d.domains.end()) return true;
        return false;
    }
    static constexpr EffectPlayCost kReduced{.ignore_energy = false, .ignore_power = false,
                                              .energy_reduction = 5};
    static std::vector<GameObjectId> topFive(const GameState& state, PlayerId p) {
        const auto& deck = state.player(p).main_deck;
        std::vector<GameObjectId> out;
        const size_t n = std::min<size_t>(5, deck.size());
        for (size_t i = 0; i < n; ++i) out.push_back(deck[deck.size() - 1 - i]);
        return out;
    }
    static std::vector<GameObjectId> playable(CardContext& ctx, const std::vector<GameObjectId>& top) {
        std::vector<GameObjectId> out;
        for (auto cid : top) {
            if (!ctx.state.objectExists(cid)) continue;
            const auto& o = ctx.state.getObject(cid);
            if (!o.isUnit() && !o.isGear()) continue;
            if (o.card_def_id == kInvalidId) continue;
            // "reducing its Energy cost by [5]": the rest is owed (CR 356.1.b.2)
            // and goes through the engine's cost path.
            if (ctx.executor.canPlayPayingCost(ctx.controller, cid, kReduced))
                out.push_back(cid);
        }
        return out;
    }
    static Intent intChoice(PlayerId p, int value, const std::string& label) {
        Intent i;
        i.type = IntentType::MakeChoice;
        i.player = p;
        i.chosen_value = value;
        i.choice_label = label;
        return i;
    }

    // Resume points: 0 look + offer a card, 1 take the card (banish, pay,
    // recycle the rest), 2 take the location and play, 3 offer Empower,
    // 4 take the Empower answer. resume_data[0] = the played card.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;  // needs the chain's resume slot
        auto& ri = *ctx.state.chain.resuming;
        auto& ps = ctx.state.player(ctx.controller);
        const auto& bfs = ctx.state.battlefields;

        if (ri.resume_point == 0) {
            auto top = topFive(ctx.state, ctx.controller);
            if (top.empty()) return;
            int rank = 0;
            for (auto cid : top) {
                knowledge::seenInDeck(ctx.state, cid, knowledge::only(ctx.controller), DeckEnd::Top, rank++);
                if (!ctx.state.objectExists(cid)) continue;
                const auto& o = ctx.state.getObject(cid);
                ctx.events.emit(CardRevealedEvent{
                    cid, o.card_def_id, o.owner,
                    /*revealed_to_all=*/false, ctx.controller, ZoneType::MainDeck});
            }
            auto cands = playable(ctx, top);
            if (cands.empty()) {
                for (auto cid : top)
                    deck::take(ctx.state, cid, deck::Place::Held, knowledge::only(ctx.controller), /*slot_public=*/false);
                ctx.executor.recycleCards(ctx.controller, top);
                ctx.events.logTrace("WILD CLAW: nothing playable in the top 5; recycled them");
                return;
            }
            std::vector<Intent> choices;
            choices.push_back(intChoice(ctx.controller, 0, "Wild Claw: play nothing"));
            for (auto cid : cands) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {cid};
                choices.push_back(std::move(c));
            }
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                       "Wild Claw: banish and play a unit or gear");
            ri.resume_point = 1;
            return;
        }

        if (ri.resume_point == 1) {
            auto choice = ctx.executor.takeChoice();
            auto top = topFive(ctx.state, ctx.controller);
            GameObjectId chosen = kInvalidId;
            if (choice && !choice->chosen_objects.empty()) chosen = choice->chosen_objects[0];
            auto cands = playable(ctx, top);
            if (std::find(cands.begin(), cands.end(), chosen) == cands.end()) chosen = kInvalidId;

            std::vector<GameObjectId> rest;
            for (auto cid : top) {
                deck::take(ctx.state, cid, deck::Place::Held, knowledge::only(ctx.controller), /*slot_public=*/false);
                if (cid != chosen) rest.push_back(cid);
            }
            ri.resume_data = {static_cast<int32_t>(chosen)};
            if (chosen != kInvalidId) {
                ctx.executor.banishObject(chosen);
                auto& bz = ps.banishment;
                bz.erase(std::remove(bz.begin(), bz.end(), chosen), bz.end());
            }
            if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
            if (chosen == kInvalidId) {
                ctx.events.logTrace("WILD CLAW: played nothing; recycled the top cards");
                ri.resume_point = 5;
                return;
            }

            // A unit goes to your base or a battlefield you control; gear to base.
            std::vector<Intent> where;
            if (ctx.state.getObject(chosen).isUnit()) {
                for (size_t i = 0; i < bfs.size(); ++i)
                    if (bfs[i].controller.has_value() && *bfs[i].controller == ctx.controller)
                        where.push_back(intChoice(ctx.controller, static_cast<int>(i) + 1,
                                                  "Wild Claw: play to battlefield " + std::to_string(i)));
            }
            if (!where.empty()) {
                where.insert(where.begin(), intChoice(ctx.controller, 0, "Wild Claw: play to base"));
                ctx.executor.requestChoice(ctx.controller, std::move(where),
                                           "Wild Claw: where to play it");
                ri.resume_point = 2;
                return;
            }
            ctx.executor.playPayingCost(ctx.controller, chosen, std::nullopt, kReduced);
            ri.resume_point = 3;
        }

        if (ri.resume_point == 2) {
            auto choice = ctx.executor.takeChoice();
            const GameObjectId chosen = static_cast<GameObjectId>(ri.resume_data[0]);
            LocationId loc = BaseLocation{ctx.controller};
            const int v = (choice && choice->chosen_value.has_value()) ? *choice->chosen_value : 0;
            if (v >= 1 && static_cast<size_t>(v) <= bfs.size() &&
                bfs[v - 1].controller.has_value() && *bfs[v - 1].controller == ctx.controller)
                loc = BattlefieldLocation{bfs[v - 1].id};
            ctx.executor.playPayingCost(ctx.controller, chosen, loc, kReduced);
            ri.resume_point = 3;
        }

        if (ri.resume_point == 3) {
            const GameObjectId chosen = static_cast<GameObjectId>(ri.resume_data[0]);
            ri.resume_point = 5;
            if (!ctx.state.objectExists(chosen)) return;
            const auto& played = ctx.state.getObject(chosen);
            if (!played.location.has_value() || played.isEmpowered()) return;
            ctx.executor.requestChoice(
                ctx.controller,
                {intChoice(ctx.controller, 0, "No — Wild Claw: Empower " + played.name),
                 intChoice(ctx.controller, 1, "Yes — Wild Claw: Empower " + played.name)},
                "may: Wild Claw: Empower " + played.name);
            ri.resume_point = 4;
            return;
        }

        if (ri.resume_point == 4) {
            auto choice = ctx.executor.takeChoice();
            const GameObjectId chosen = static_cast<GameObjectId>(ri.resume_data[0]);
            ri.resume_point = 5;
            if (choice && choice->chosen_value.has_value() && *choice->chosen_value == 1)
                ctx.executor.empower(chosen);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 876;
        d.def_id = R"RB(ven-089-166)RB";
        d.name = R"RB(Wild Claw)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-089/166)RB";
        d.collector_number = 89;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Look at the top 5 cards of your Main Deck. You may banish a unit or gear from among them and play it, reducing its Energy cost by [5]. Recycle the rest. Then you may do this: Empower it. (It becomes Empowered if it's not already.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a58871b08a51efb8ce33f399fe4d168fd25b596a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_876(CardRegistry& r) {
    r.registerCard(876, std::make_unique<WildClaw>());
}

}  // namespace riftbound
