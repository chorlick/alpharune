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
    // recycle the rest), 2 take the location and play, 3 queue the reflexive
    // trigger, 5 done. resume_data[0] = the played card.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;  // needs the chain's resume slot
        auto& ri = *ctx.state.chain.resuming;

        if (ri.resume_point == 0) {
            auto top = topFive(ctx.state, ctx.controller);
            if (top.empty()) return;
            for (auto cid : top) {
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
                RIFT_TRACE(ctx.events, "WILD CLAW: nothing playable in the top 5; recycled them");
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
            // At resolution: a card in a hidden zone is not a target (CR 355.10.a; batch M1).
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                       "Wild Claw: banish and play a unit or gear", PromptKind::ChooseObject);
            ri.resume_point = 1;
            return;
        }

        // resume_data = {the chosen card, the rest...}: the rest wait, out of
        // the deck, until the chosen card has been played ("... and play it,
        // reducing its Energy cost by [5]. Recycle the rest."; testing phase R6).
        auto recycleRest = [&]() {
            std::vector<GameObjectId> rest;
            for (size_t i = 1; i < ri.resume_data.size(); ++i)
                rest.push_back(GameObjectId::fromRaw(ri.resume_data[i]));
            ri.resume_data.resize(std::min<size_t>(ri.resume_data.size(), 1));
            if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
        };
        if (ri.resume_point == 1) {
            auto choice = ctx.executor.takeChoice();
            auto top = topFive(ctx.state, ctx.controller);
            GameObjectId chosen = kInvalidId;
            if (choice && !choice->chosen_objects.empty()) chosen = choice->chosen_objects[0];
            auto cands = playable(ctx, top);
            if (std::find(cands.begin(), cands.end(), chosen) == cands.end()) chosen = kInvalidId;

            ri.resume_data = {static_cast<int32_t>(chosen.raw())};
            for (auto cid : top) {
                deck::take(ctx.state, cid, deck::Place::Held, knowledge::only(ctx.controller), /*slot_public=*/false);
                if (cid != chosen) ri.resume_data.push_back(static_cast<int32_t>(cid.raw()));
            }
            if (chosen == kInvalidId) {
                recycleRest();
                RIFT_TRACE(ctx.events, "WILD CLAW: played nothing; recycled the top cards");
                ri.resume_point = 5;
                return;
            }
            // It stays listed in banishment until it is actually played:
            // playPayingCost takes it out on success, and a payment that
            // fails leaves it banished rather than in no zone at all.
            ctx.executor.banishObject(chosen);

            // A unit goes to your base, a battlefield you control or where
            // its own permissions allow (CR 355.2), never where units can't
            // be played (playLocationsFor); gear to base.
            if (ctx.state.getObject(chosen).isUnit()) {
                const auto where = playLocationsFor(ctx, ctx.controller, chosen);
                if (where.count() > 1) {
                    std::vector<Intent> opts;
                    for (int i = 0; i < where.count(); ++i)
                        opts.push_back(intChoice(ctx.controller,
                                                 locationCode(where.locations[static_cast<size_t>(i)]),
                                                 "Wild Claw: play to " +
                                                 where.labels[static_cast<size_t>(i)]));
                    // At resolution: an effect play's location is chosen as it plays (CR 420.3.b.2; batch M1).
                    ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                               "Wild Claw: where to play it", PromptKind::Location);
                    ri.resume_point = 2;
                    return;
                }
            }
            ctx.executor.playPayingCost(ctx.controller, chosen, std::nullopt, kReduced);
            recycleRest();
            ri.resume_point = 3;
        }

        if (ri.resume_point == 2) {
            auto choice = ctx.executor.takeChoice();
            const GameObjectId chosen = GameObjectId::fromRaw(ri.resume_data[0]);
            const auto where = playLocationsFor(ctx, ctx.controller, chosen);
            const int v = (choice && choice->chosen_value.has_value()) ? *choice->chosen_value : 0;
            std::optional<LocationId> loc;
            for (const auto& l : where.locations)
                if (locationCode(l) == v) loc = l;
            ctx.executor.playPayingCost(ctx.controller, chosen, loc, kReduced);
            recycleRest();
            ri.resume_point = 3;
        }

        if (ri.resume_point == 3) {
            const GameObjectId chosen = GameObjectId::fromRaw(ri.resume_data[0]);
            ri.resume_point = 5;
            // "Then you may do this: Empower it." — a reflexive trigger (CR
            // 388): with the card played it goes on the chain as an item of
            // its own; the "may" is decided as that item is finalized (CR
            // 388.4).
            if (ctx.state.objectExists(chosen) &&
                ctx.state.getObject(chosen).location.has_value())
                ctx.executor.queueReflexiveTrigger(ctx.source, ctx.controller, 0,
                                                   {static_cast<int32_t>(chosen.raw())});
        }
    }
    bool hasReflexiveTrigger() const override { return true; }
    // `data[0]`: the card this spell played.
    void onReflexiveTrigger(CardContext& ctx, int /*index*/,
                            const std::vector<int32_t>& data) override {
        if (data.empty()) return;
        const auto played = GameObjectId::fromRaw(data[0]);
        auto can_empower = [&]() {
            return ctx.state.objectExists(played) &&
                   ctx.state.getObject(played).location.has_value() &&
                   !ctx.state.getObject(played).isEmpowered();
        };
        const int conf = confirmOptional(ctx, "Wild Claw: Empower it", can_empower);
        if (finalizingChoices(ctx)) return;   // the "may" ends here (CR 388.4)
        if (conf < 1 || !can_empower()) return;
        ctx.executor.empower(played);
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
