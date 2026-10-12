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

class GustMonk : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // ── behaviour (Vendetta batch C) ──
    // "You may pay [1] as an additional cost to play me."
    OptionalAdditionalCost optionalAdditionalCost() const override {
        return {/*valid=*/true, /*energy=*/1, /*power=*/0, Domain::Chaos,
                /*any_domain=*/false, /*paid_flag=*/"__gust_monk_paid"};
    }
    // "When you play me, if you paid the additional cost, banish a card from
    //  any trash to give a unit [Assault 2] this turn."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "if you paid the additional cost" is part of the trigger condition (CR
    // 383.2.a.1): asked as I am played; unpaid, nothing is put on the chain.
    // The engine forgets the flag as each play begins
    // (Card::forgetPlayPayments).
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        if (!state.objectExists(source)) return false;
        const auto& c = state.getObject(source).card_counters;
        const auto it = c.find("__gust_monk_paid");
        return it != c.end() && it->second == 1;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // The unit that gets [Assault 2] is the target; the card banished is
        // the cost ("banish a card from any trash to ..."), picked and paid
        // as the ability goes on the chain (CR 383.3) and not refunded if the
        // unit is gone by the time it resolves.
        const auto& ri = ctx.state.chain.resuming;
        const GameObjectId unit = pickTarget(ctx, "Gust Monk: give a unit [Assault 2]",
            chooseableUnits(ctx.state, ctx.controller, [](const GameObject&) { return true; }));
        if (unit == kInvalidId && ri.has_value() && ri->resume_point == 7) return;   // waiting
        if (!costAlreadyPaid(ctx)) {
            if (unit == kInvalidId) return;           // no unit to give it to
            std::vector<GameObjectId> trash_cards;
            for (auto p : {ctx.controller, opponent(ctx.controller)})
                for (auto cid : ctx.state.player(p).trash)
                    if (ctx.state.objectExists(cid)) trash_cards.push_back(cid);
            auto card = selectObject(ctx, "Gust Monk: banish a card from a trash", trash_cards);
            if (!card.has_value() || *card == kInvalidId) return;   // waiting / no card
            if (costDueNow(ctx)) {
                if (ctx.state.getObject(*card).zone != ZoneType::Trash) return;   // left the trash
                RIFT_TRACE(ctx.events, "GUST MONK: banished " + ctx.state.getObject(*card).name +
                                    " from trash");
                ctx.executor.banishObject(*card);     // counts it, announces "banished_owned"
            }
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (unit == kInvalidId) return;               // the unit is gone: mistarget
        ctx.executor.giveTemporaryKeyword(unit, Keyword::Assault, 2);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 888;
        d.def_id = R"RB(ven-101-166)RB";
        d.name = R"RB(Gust Monk)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-101/166)RB";
        d.collector_number = 101;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(You may pay [1] as an additional cost to play me.
When you play me, if you paid the additional cost, banish a card from any trash to give a unit [Assault 2] this turn. (+2 [M] while it's an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/93a194c0c1f055c0b3bc3c0717cbe488749fed92-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_888(CardRegistry& r) {
    r.registerCard(888, std::make_unique<GustMonk>());
}

}  // namespace riftbound
