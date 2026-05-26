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

class TwistedFateGambler : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    // "When I attack, reveal the top rune of your rune deck, then recycle it.
    // Do one of the following based on its domain:
    //  [R] Deal 2 to an enemy unit here and 1 to all other enemy units here.
    //  [B] Draw 1.  [Y] Stun an enemy unit."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.rune_deck.empty()) return;
        GameObjectId rune = ps.rune_deck.back();
        if (!ctx.state.objectExists(rune)) return;

        // Determine domain payoff BEFORE recycling (read its domains).
        const auto& rdomains = ctx.state.getObject(rune).domains;
        auto has = [&](Domain d) {
            for (auto x : rdomains) if (x == d) return true;
            return false;
        };
        bool is_r = has(Domain::Fury);
        bool is_b = has(Domain::Mind);
        bool is_y = has(Domain::Order);

        // Reveal (public) then recycle the rune to the bottom of the rune deck.
        const auto& robj = ctx.state.getObject(rune);
        ctx.events.emit(CardRevealedEvent{
            /*card=*/rune, /*card_def_id=*/robj.card_def_id, /*owner=*/robj.owner,
            /*revealed_to_all=*/true, /*revealed_to=*/PlayerId::None,
            /*source_zone=*/ZoneType::RuneDeck});
        ps.rune_deck.pop_back();
        ctx.executor.recycleCards(ctx.controller, {rune});

        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        auto enemies_here = [&]() {
            std::vector<GameObjectId> out;
            if (!my_bf) return out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller == ctx.controller) continue;
                if (obj.battlefieldId() != my_bf) continue;
                out.push_back(id);
            }
            return out;
        };

        // Act on the first matching domain (R, then B, then Y).
        if (is_r) {
            auto targets = enemies_here();
            if (!targets.empty()) {
                GameObjectId focus = pickTarget(ctx, "Twisted Fate [R] (deal 2 here)",
                                                 targets);
                if (focus == kInvalidId && ctx.state.chain.resuming.has_value() &&
                    ctx.state.chain.resuming->resume_point == 7) {
                    return;  // suspended
                }
                // Collect-then-kill: deal damage to all first.
                std::vector<GameObjectId> hit;
                for (auto id : enemies_here()) {
                    if (!ctx.state.objectExists(id)) continue;
                    int amt = (id == focus) ? 2 : 1;
                    ctx.executor.dealDamage(id, amt, ctx.source);
                    hit.push_back(id);
                }
                for (auto id : hit)
                    if (ctx.state.objectExists(id) &&
                        ctx.state.getObject(id).hasLethalDamage())
                        ctx.executor.killObject(id);
            }
        } else if (is_b) {
            ctx.executor.drawCards(ctx.controller, 1);
        } else if (is_y) {
            std::vector<GameObjectId> enemies;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller == ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                enemies.push_back(id);
            }
            GameObjectId t = pickTarget(ctx, "Twisted Fate [Y] (stun)", enemies);
            if (t == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7) {
                return;  // suspended
            }
            if (t != kInvalidId && ctx.state.objectExists(t))
                ctx.executor.stunUnitBy(t, ctx.source);
        }
        ctx.events.logTrace("TWISTED FATE: revealed rune -> domain payoff");
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
