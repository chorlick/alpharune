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

class ShurikenFlip : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Deal 2 to up to one enemy unit at a battlefield, then move a friendly
    //  unit."
    // Resumable: (1) optional enemy target ("No damage" declines), (2) the
    // friendly unit to move, (3) its destination. The picks are built by hand,
    // so each goes through offerableChoices / commitChosenObject.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        auto enemies = [&]() {
            auto out = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.controller != ctx.controller && o.isAtBattlefield();
            });
            std::sort(out.begin(), out.end());
            // Deflect (CR 809): only units whose cost the controller can pay.
            return offerableChoices(ctx, out);
        };
        auto movable = [&]() {
            auto out = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.controller == ctx.controller && !moveDests(ctx.state, o).empty();
            });
            std::sort(out.begin(), out.end());
            return out;
        };
        auto promptMove = [&]() {
            auto mine = movable();
            if (mine.empty()) { ri.resume_point = 4; return; }
            std::vector<Intent> opts;
            for (auto u : mine) opts.push_back(objChoice(ctx.controller, u));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Shuriken Flip: move a friendly unit");
            ri.resume_point = 2;
        };
        switch (ri.resume_point) {
        case 0: {
            auto foes = enemies();
            if (foes.empty()) { promptMove(); return; }
            std::vector<Intent> opts;
            for (auto u : foes) opts.push_back(objChoice(ctx.controller, u));
            // "up to one" includes none (CR 355.13); the move happens either way.
            opts.push_back(valChoice(ctx.controller, 0, "No damage"));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Shuriken Flip: deal 2 to up to one enemy unit");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            GameObjectId t = takenObject(ctx);
            auto foes = enemies();
            if (std::find(foes.begin(), foes.end(), t) != foes.end()) {
                // The unit is chosen: Deflect is paid and the choice recorded,
                // as Card::pickTarget would.
                commitChosenObject(ctx, t);
                ctx.executor.dealDamage(t, 2, ctx.source);
            }
            promptMove();
            return;
        }
        case 2: {
            GameObjectId u = takenObject(ctx);
            auto mine = movable();
            if (mine.empty()) { ri.resume_point = 4; return; }
            if (std::find(mine.begin(), mine.end(), u) == mine.end()) u = mine.front();
            commitChosenObject(ctx, u);   // "when you choose a friendly unit"
            ri.resume_data = {static_cast<int32_t>(u.raw())};
            std::vector<Intent> opts;
            for (int d : moveDests(ctx.state, ctx.state.getObject(u)))
                opts.push_back(valChoice(ctx.controller, d, destLabel(d)));
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Shuriken Flip: move it where?");
            ri.resume_point = 3;
            return;
        }
        case 3: {
            ri.resume_point = 4;
            if (ri.resume_data.empty()) return;
            auto u = GameObjectId::fromRaw(ri.resume_data[0]);
            if (!ctx.state.objectExists(u)) return;
            auto dests = moveDests(ctx.state, ctx.state.getObject(u));
            if (dests.empty()) return;
            int d = takenValue(ctx, dests.front());
            if (std::find(dests.begin(), dests.end(), d) == dests.end()) d = dests.front();
            moveTo(ctx, u, d);
            return;
        }
        default:
            return;
        }
    }
    // "[Flow] [3][A]"
    FlowCost flowCost() const override { return flow(3, 1, Domain::Fury, /*any_domain=*/true); }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 927;
        d.def_id = R"RB(ven-140-166)RB";
        d.name = R"RB(Shuriken Flip)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-140/166)RB";
        d.collector_number = 140;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Calm};
        d.tags = {R"RB(Akali)RB"};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Flow);
        d.ability_text = R"RB(Deal 2 to up to one enemy unit at a battlefield, then move a friendly unit.
[Flow] [3][A] (You may play this from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bac1ecff92d022e99a3696ad00d020530a1a09e7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_927(CardRegistry& r) {
    r.registerCard(927, std::make_unique<ShurikenFlip>());
}

}  // namespace riftbound
