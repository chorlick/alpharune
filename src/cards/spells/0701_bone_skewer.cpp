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

class BoneSkewer : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);

        // Reveal opponent's hand (public) up front.
        if (!ctx.state.chain.resuming.has_value() ||
            ctx.state.chain.resuming->resume_point < 9) {
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

        // Pair pick: (A) the battlefield (BF card object), (B) a unit from the
        // opponent's hand. GAP: "You may choose a unit" optionality is lost —
        // pickTargetPair forces a choice if any hand unit exists; if none exist
        // the second pick fizzles and nothing is played.
        std::vector<GameObjectId> bf_cards;
        for (const auto& bf : ctx.state.battlefields) {
            if (ctx.state.objectExists(bf.card_object_id)) {
                bf_cards.push_back(bf.card_object_id);
            }
        }
        auto handUnits = [&ctx, opp](GameObjectId /*picked_bf*/) {
            std::vector<GameObjectId> out;
            for (auto cid : ctx.state.player(opp).hand) {
                if (!ctx.state.objectExists(cid)) continue;
                if (ctx.state.getObject(cid).card_type == CardType::Unit) {
                    out.push_back(cid);
                }
            }
            return out;
        };
        auto pair = pickTargetPair(ctx, "Bone Skewer: choose BF + enemy unit",
                                   bf_cards, handUnits);
        GameObjectId bf_card = pair.first;
        GameObjectId chosen = pair.second;
        if (bf_card == kInvalidId) return;  // suspended or no BFs
        if (chosen == kInvalidId || !ctx.state.objectExists(chosen)) return;

        // Resolve BattlefieldId from the chosen BF card object.
        BattlefieldId dest_bf = kInvalidId;
        for (const auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == bf_card) { dest_bf = bf.id; break; }
        }
        if (dest_bf == kInvalidId) return;

        // They play it to that battlefield, ignoring all costs. Remove it
        // from the opponent's hand vector first (playIgnoringCost re-zones the
        // object but does not detach it from the source zone's id list).
        auto& opp_hand = ctx.state.player(opp).hand;
        opp_hand.erase(std::remove(opp_hand.begin(), opp_hand.end(), chosen),
                       opp_hand.end());
        ctx.executor.playIgnoringCost(opp, chosen,
                                      LocationId{BattlefieldLocation{dest_bf}});
        ctx.events.logTrace("BONE SKEWER: opponent forced to play " +
                             ctx.state.getObject(chosen).name + " to BF#" +
                             std::to_string(dest_bf));

        // "If they do, then do this: [Stun] it."
        if (ctx.state.objectExists(chosen) &&
            ctx.state.getObject(chosen).location.has_value()) {
            ctx.executor.stunUnitBy(chosen, ctx.source);
            ctx.events.logTrace("BONE SKEWER: stunned " +
                                 ctx.state.getObject(chosen).name);
        }
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
