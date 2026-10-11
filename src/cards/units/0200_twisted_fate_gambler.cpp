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

class TwistedFateGambler : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    // "When I attack, reveal the top rune of your rune deck, then recycle it.
    // Do one of the following based on its domain:
    //  [R] Deal 2 to an enemy unit here and 1 to all other enemy units here.
    //  [B] Draw 1.  [Y] Stun an enemy unit."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.rune_deck.empty()) return;
        GameObjectId rune = ps.rune_deck.back();
        const auto& robj = ctx.state.getObject(rune);
        int32_t payoff = 0;
        for (auto d : robj.domains) {
            if (d == Domain::Fury) payoff |= kFury;
            if (d == Domain::Mind) payoff |= kMind;
            if (d == Domain::Order) payoff |= kOrder;
        }
        // Reveal (public) then recycle the rune to the bottom of the rune deck.
        ctx.events.emit(CardRevealedEvent{
            /*card=*/rune, /*card_def_id=*/robj.card_def_id, /*owner=*/robj.owner,
            /*revealed_to_all=*/true, /*revealed_to=*/PlayerId::None,
            /*source_zone=*/ZoneType::RuneDeck});
        ps.rune_deck.pop_back();
        ctx.executor.recycleCards(ctx.controller, {rune});
        RIFT_TRACE(ctx.events, "TWISTED FATE: revealed and recycled the top rune");
        // "Do one of the following based on its domain" is a reflexive
        // trigger (CR 388.1): the one that matches goes on the chain now, as
        // a pending item of its own (CR 389.1) — its target is chosen when it
        // is finalized and players may respond before it resolves. A rune of
        // none of the three domains creates none.
        if (payoff != 0)
            ctx.executor.queueReflexiveTrigger(ctx.source, ctx.controller, kByDomain, {payoff});
    }

    static constexpr int kByDomain = 0;
    bool hasReflexiveTrigger() const override { return true; }
    enum : int32_t { kFury = 1, kMind = 2, kOrder = 4 };
    // `data[0]`: the revealed rune's domains. The first that matches, in the
    // printed order ([R], then [B], then [Y]).
    void onReflexiveTrigger(CardContext& ctx, int /*index*/,
                            const std::vector<int32_t>& data) override {
        if (data.empty() || !ctx.state.objectExists(ctx.source)) return;
        const int32_t payoff = data[0];
        // "here" is where I am as this is finalized and resolves.
        const auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        // Enemy units my controller may choose (not the ones that can't be
        // chosen by enemy spells and abilities).
        auto enemies = [&](bool here_only) {
            auto out = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                if (o.controller == ctx.controller) return false;
                return !here_only || (my_bf.has_value() && o.battlefieldId() == my_bf);
            });
            std::sort(out.begin(), out.end());
            return out;
        };
        if (payoff & kFury) {
            GameObjectId focus = pickTarget(ctx, "Twisted Fate [R] (deal 2 here)", enemies(true));
            if (finalizingChoices(ctx)) return;    // choices end here (CR 388.4)
            if (focus == kInvalidId) return;       // no enemy unit here / mistarget
            // "1 to all other enemy units here" is not a choice: every other
            // enemy unit at my battlefield. Collect-then-kill.
            std::vector<GameObjectId> hit;
            for (const auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller == ctx.controller) continue;
                if (!my_bf.has_value() || obj.battlefieldId() != my_bf) continue;
                hit.push_back(id);
            }
            std::sort(hit.begin(), hit.end());
            for (auto id : hit) ctx.executor.dealDamage(id, id == focus ? 2 : 1, ctx.source);
            {   // the batch is for these kills only
                DeathBatch together(ctx.executor, hit);   // they die at the same time
                for (auto id : hit)
                    if (ctx.state.getObject(id).location.has_value() &&
                        ctx.state.getObject(id).hasLethalDamage())
                        ctx.executor.killObject(id);
            }
        } else if (payoff & kMind) {
            if (finalizingChoices(ctx)) return;    // nothing to choose
            ctx.executor.drawCards(ctx.controller, 1);
        } else if (payoff & kOrder) {
            GameObjectId t = pickTarget(ctx, "Twisted Fate [Y] (stun)", enemies(false));
            if (finalizingChoices(ctx)) return;    // choices end here (CR 388.4)
            if (t == kInvalidId) return;           // no enemy unit / mistarget
            ctx.executor.stunUnitBy(t, ctx.source);
        } else {
            if (finalizingChoices(ctx)) return;
        }
        RIFT_TRACE(ctx.events, "TWISTED FATE: domain payoff resolved");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 200;
        d.def_id = R"RB(ogn-200-298)RB";
        d.name = R"RB(Twisted Fate, Gambler)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-200/298)RB";
        d.collector_number = 200;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Twisted Fate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I attack, reveal the top rune of your rune deck, then recycle it. Do one of the following based on its domain:
[R] — Deal 2 to an enemy unit here and 1 to all other enemy units here.[B] — Draw 1.[Y] — Stun an enemy unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9eff1a038528c4d633196f13259670d84b74c2a8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_200(CardRegistry& r) {
    r.registerCard(200, std::make_unique<TwistedFateGambler>());
}

} // namespace riftbound
