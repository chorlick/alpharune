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

class HweiBroodingPainter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);

        auto build_choices = [&]() {
            std::vector<Intent> out;
            for (auto cid : ps.hand) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {cid};
                out.push_back(std::move(c));
            }
            return out;
        };

        switch (ri.resume_point) {
        case 0: {
            // Draw 1, then prompt the discard.
            ctx.executor.drawCards(ctx.controller, 1);
            if (ps.hand.empty()) return;  // nothing to discard, no branch
            ctx.executor.requestChoice(ctx.controller, build_choices(),
                                        "Hwei: discard 1");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (!choice || choice->chosen_objects.empty()) return;
            auto cid = choice->chosen_objects[0];
            if (!ctx.state.objectExists(cid)) return;

            // Capture the discarded card's type BEFORE it leaves hand.
            CardType dtype = ctx.state.getObject(cid).card_type;
            ctx.executor.applyDiscard(ctx.controller, cid);

            // Branch on the discarded card's type.
            if (dtype == CardType::Spell) {
                ctx.executor.drawCards(ctx.controller, 1);
                ctx.events.logTrace("HWEI: discarded Spell -> draw 1");
            } else if (dtype == CardType::Gear) {
                // "Ready up to 2 runes": asked below, once the discard is done.
                ri.resume_point = 2;
                readyUpToTwoRunes(ctx);
            } else if (dtype == CardType::Unit) {
                if (ctx.state.objectExists(ctx.source) &&
                    ctx.state.getObject(ctx.source).location.has_value()) {
                    ctx.executor.giveTemporaryMight(ctx.source, 3);
                    ctx.events.logTrace("HWEI: discarded Unit -> +3 [M] this turn");
                }
            }
            return;
        }
        case 2:   // re-entered with the answer to a rune prompt
            readyUpToTwoRunes(ctx);
            return;
        }
    }
private:
    // "Gear — Ready up to 2 runes." The controller names the runes one at a
    // time and may stop at any prompt, the first included (CR 355.13): which
    // runes matters when more than two are exhausted (a rune left exhausted is
    // the one to recycle for Power). With no exhausted rune nothing is asked.
    //
    // Timing: unlike the other "ready up to N runes" cards this is NOT a
    // choice of the move ability's finalization (no kFinalizeTrigger). "Do the
    // following based on the discarded card's type" is a reflexive trigger (CR
    // 388.1.b; Vendetta FAQ on Hwei, rule 387.1.b): it comes into being while
    // the move ability resolves, after the discard, and its choices are made
    // when IT is finalized (CR 389.1, 355.5.b) — at once, before anyone has
    // priority. So the runes are named here, right after the discard. (The
    // engine has no chain item of its own for a reflexive trigger — cf.
    // Twisted Fate 200, Deceiver 756 — so the readying follows inline.)
    //
    // "runes", with no "friendly" (Sona 73) or "your" (Ekko 110): the word
    // refers to any rune on the board (CR 133.3.a, 355.9.a.1) and only a
    // printed restriction narrows a choice (CR 355.9.b), so an opponent's
    // exhausted rune may be named too.
    static void readyUpToTwoRunes(CardContext& ctx) {
        auto readiable = [&](GameObjectId id) {
            if (!ctx.state.objectExists(id)) return false;
            const auto& obj = ctx.state.getObject(id);
            return obj.isRune() && obj.is_exhausted && obj.location.has_value();
        };
        std::vector<GameObjectId> exhausted;
        for (auto& [id, obj] : ctx.state.objects)
            if (readiable(id)) exhausted.push_back(id);
        const auto chosen = selectSeveral(ctx, "Hwei: ready a rune (up to two)", exhausted,
                                          /*max_picks=*/2);
        if (!chosen.has_value()) return;   // waiting for the agent
        int readied = 0;
        for (auto id : *chosen) {
            if (!readiable(id)) continue;
            ctx.executor.readyObject(id);
            ++readied;
        }
        ctx.events.logTrace("HWEI: discarded Gear -> readied " + std::to_string(readied) +
                            " rune(s)");
    }
    const CardDef def_ = [] {
        CardDef d;
        d.id = 642;
        d.def_id = R"RB(unl-080-219)RB";
        d.name = R"RB(Hwei, Brooding Painter)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-080/219)RB";
        d.collector_number = 80;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Ionia)RB", R"RB(Hwei)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I move, draw 1, then discard 1. Then, do the following based on the discarded card's type:
Spell — Draw 1.Gear — Ready up to 2 runes.Unit — Give me +3 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ddbb13ba5617cc443b9a3a51485b1697da570121-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_642(CardRegistry& r) {
    r.registerCard(642, std::make_unique<HweiBroodingPainter>());
}

} // namespace riftbound
