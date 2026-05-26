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

class Deceiver : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }

    // Find a battlefield the controller is "at" / controls — best guess for
    // "there". Prefer one the controller controls; fall back to any BF with
    // friendly units.
    static std::optional<BattlefieldId> scoredBattlefield(GameState& state,
                                                           PlayerId controller) {
        // Controlled battlefield first.
        for (auto& bf : state.battlefields) {
            if (bf.controller.has_value() && *bf.controller == controller)
                return bf.id;
        }
        // Otherwise any BF with a friendly unit.
        for (auto& bf : state.battlefields) {
            for (auto& [id, obj] : state.objects) {
                if (!obj.isUnit() || obj.controller != controller) continue;
                if (obj.battlefieldId() == bf.id) return bf.id;
            }
        }
        return std::nullopt;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ri = ctx.state.chain.resuming.value();
        switch (ri.resume_point) {
        case 0: {
            if (!ctx.state.objectExists(ctx.source)) return;
            auto& legend = ctx.state.getObject(ctx.source);
            if (legend.is_exhausted) return;          // can't pay the exhaust cost
            auto& ps = ctx.state.player(ctx.controller);
            if (ps.hand.empty()) return;              // can't pay the discard cost
            // Cost paid: exhaust the legend.
            legend.is_exhausted = true;
            // Publish the discard choice (which card of hand to discard).
            std::vector<Intent> choices;
            for (auto card_id : ps.hand) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {card_id};
                choices.push_back(c);
            }
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                       "discard 1 (Deceiver)");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty()) {
                ctx.executor.applyDiscard(ctx.controller, choice->chosen_objects[0]);
            }

            // Determine "there" — the scored battlefield.
            auto bf = scoredBattlefield(ctx.state, ctx.controller);
            LocationId loc = bf.has_value()
                ? LocationId{BattlefieldLocation{*bf}}
                : LocationId{BaseLocation{ctx.controller}};

            // Create a ready Reflection unit token there. [Temporary] up front.
            KeywordSet kw;
            kw.set(Keyword::Temporary);
            GameObjectId token = ctx.executor.createToken(
                ctx.controller, CardType::Unit, "Reflection", 0, {}, kw, loc,
                /*enter_ready=*/true);
            ctx.events.logTrace("DECEIVER: created ready Reflection token there");

            // "It becomes a copy of another unit there." Pick the highest-
            // might OTHER unit at the same location (any controller).
            if (bf.has_value()) {
                GameObjectId best = kInvalidId;
                int best_might = -1;
                for (auto& [id, obj] : ctx.state.objects) {
                    if (id == token) continue;
                    if (!obj.isUnit()) continue;
                    if (obj.battlefieldId() != *bf) continue;
                    if (obj.current_might > best_might) {
                        best_might = obj.current_might;
                        best = id;
                    }
                }
                if (best != kInvalidId) {
                    ctx.executor.copyUnit(token, best);
                    // copyUnit overwrites keywords from the copied def, so
                    // re-grant [Temporary] afterwards (CR: the token keeps it).
                    if (ctx.state.objectExists(token)) {
                        ctx.state.getObject(token).keywords.set(Keyword::Temporary);
                    }
                    ctx.events.logTrace("DECEIVER: token copied a unit there, kept [Temporary]");
                }
            }
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 756;
        d.def_id = R"RB(unl-199-219)RB";
        d.name = R"RB(Deceiver)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-199/219)RB";
        d.collector_number = 199;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(LeBlanc)RB"};
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB(When you conquer or hold, you may discard 1 and exhaust me to play a ready Reflection unit token there. Then do this: It becomes a copy of another unit there. Give it [Temporary].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c17d29602d61d6702372fec4db44c6fb2a29e8a2-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_756(CardRegistry& r) {
    r.registerCard(756, std::make_unique<Deceiver>());
}

} // namespace riftbound
