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

class BoneSkewer : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Choose a battlefield. An opponent reveals their hand. You may choose a
    //  unit from it. They play that unit to that battlefield, ignoring any and
    //  all costs. If they do, then do this: [Stun] it."
    static constexpr size_t kRevealedSlot = 7;   // resume_data: the hand was revealed

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);

        // The battlefield: any of them — played from facedown, the one this
        // was hidden at (CR 811.1.d.2).
        const auto& bfs = ctx.state.battlefields;
        const int n = static_cast<int>(std::min<size_t>(bfs.size(), 32));
        const auto hidden_at = facedownPlayBattlefield(ctx.state, ctx.source);
        std::vector<std::string> labels;
        uint32_t mask = 0;
        for (int i = 0; i < n; ++i) {
            labels.push_back(ctx.state.objectExists(bfs[i].card_object_id)
                ? ctx.state.getObject(bfs[i].card_object_id).name
                : "Battlefield #" + std::to_string(static_cast<int>(bfs[i].id)));
            if (!hidden_at.has_value() || *hidden_at == bfs[i].id) mask |= (1u << i);
        }
        const int mode = pickMode(ctx, "Bone Skewer: choose a battlefield", n, labels, mask);
        if (mode < 0 || mode >= n) return;   // -1 waiting, -2 no battlefield
        const BattlefieldId dest_bf = bfs[mode].id;

        // "An opponent reveals their hand" — once, after the battlefield is
        // chosen, not again when this is re-entered with an answer.
        auto& ri = ctx.state.chain.resuming;
        bool revealed = false;
        if (ri.has_value()) {
            while (ri->resume_data.size() <= kRevealedSlot) ri->resume_data.push_back(0);
            revealed = ri->resume_data[kRevealedSlot] != 0;
            ri->resume_data[kRevealedSlot] = 1;
        }
        if (!revealed) {
            for (auto cid : ctx.state.player(opp).hand) {
                if (!ctx.state.objectExists(cid)) continue;
                const auto& c = ctx.state.getObject(cid);
                ctx.events.emit(CardRevealedEvent{
                    /*card=*/cid,
                    /*card_def_id=*/c.card_def_id,
                    /*owner=*/c.owner,
                    /*revealed_to_all=*/true,
                    /*revealed_to=*/PlayerId::None,
                    /*source_zone=*/ZoneType::Hand,
                });
            }
        }

        auto hand_units = [&]() {
            std::vector<GameObjectId> out;
            for (auto cid : ctx.state.player(opp).hand)
                if (ctx.state.objectExists(cid) &&
                    ctx.state.getObject(cid).card_type == CardType::Unit)
                    out.push_back(cid);
            return out;
        };
        // "You may choose a unit from it": whether, then which. Both before
        // anything is played.
        const int go = confirmOptional(ctx, "Bone Skewer: choose a unit from the revealed hand",
                                       [&]() { return !hand_units().empty(); });
        if (go <= 0) return;                 // waiting, declined, or no unit there
        const GameObjectId chosen = pickTarget(ctx, "Bone Skewer: unit they play", hand_units());
        if (chosen == kInvalidId || !ctx.state.objectExists(chosen)) return;

        // They play it to that battlefield, ignoring all costs.
        ctx.executor.playIgnoringCost(opp, chosen,
                                      LocationId{BattlefieldLocation{dest_bf}});
        RIFT_TRACE(ctx.events, "BONE SKEWER: opponent forced to play " +
                             ctx.state.getObject(chosen).name + " to BF#" +
                             std::to_string(dest_bf));

        // "If they do, then do this: [Stun] it." — a reflexive trigger (CR
        // 388): if the unit was played it goes on the chain as an item of its
        // own.
        if (ctx.state.objectExists(chosen) &&
            ctx.state.getObject(chosen).location.has_value())
            ctx.executor.queueReflexiveTrigger(ctx.source, ctx.controller, 0,
                                               {static_cast<int32_t>(chosen)});
    }
    bool hasReflexiveTrigger() const override { return true; }
    // `data[0]`: the unit they played.
    void onReflexiveTrigger(CardContext& ctx, int /*index*/,
                            const std::vector<int32_t>& data) override {
        if (finalizingChoices(ctx)) return;   // nothing to choose
        if (data.empty()) return;
        const auto unit = static_cast<GameObjectId>(data[0]);
        if (!ctx.state.objectExists(unit) || !ctx.state.getObject(unit).location.has_value())
            return;                           // it left in response
        ctx.executor.stunUnitBy(unit, ctx.source);
        RIFT_TRACE(ctx.events, "BONE SKEWER: stunned " + ctx.state.getObject(unit).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 701;
        d.def_id = R"RB(unl-139-219)RB";
        d.name = R"RB(Bone Skewer)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-139/219)RB";
        d.collector_number = 139;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
Choose a battlefield. An opponent reveals their hand. You may choose a unit from it. They play that unit to that battlefield, ignoring any and all costs. If they do, then do this: [Stun] it. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e06e3e3033640bc8a5bc98acd4e3a0925c6e2c9a-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_701(CardRegistry& r) {
    r.registerCard(701, std::make_unique<BoneSkewer>());
}

} // namespace riftbound
