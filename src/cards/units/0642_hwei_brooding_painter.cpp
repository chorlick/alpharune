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
                // Ready up to 2 runes (channelRunes brings runes in ready;
                // here we ready up to 2 already-exhausted runes belonging to
                // the controller).
                int readied = 0;
                for (auto& [id, obj] : ctx.state.objects) {
                    if (readied >= 2) break;
                    if (obj.isRune() && obj.controller == ctx.controller &&
                        obj.is_exhausted) {
                        ctx.executor.readyObject(id);
                        ++readied;
                    }
                }
                ctx.events.logTrace("HWEI: discarded Gear -> readied " +
                                     std::to_string(readied) + " rune(s)");
            } else if (dtype == CardType::Unit) {
                if (ctx.state.objectExists(ctx.source)) {
                    ctx.executor.giveTemporaryMight(ctx.source, 3);
                    ctx.events.logTrace("HWEI: discarded Unit -> +3 [M] this turn");
                }
            }
            return;
        }
        }
    }
private:
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
