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

class Reinforce : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Look at the top 5 cards of your Main Deck. You may banish a unit from
    //  among them, then play it, reducing its cost by [5]. Recycle the
    //  remaining cards."
    // Progress lives on the resolving item: resume_point kAwait while the
    // prompt is out, resume_data = the cards looked at. kAwaitWhere while the
    // chosen unit's location is asked (CR 355.2.a): that unit's id is then
    // appended to resume_data.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        auto& ps = ctx.state.player(ctx.controller);
        const auto& bfs = ctx.state.battlefields;
        const EffectExecutor::EffectPlayCost reduced{.energy_reduction = 5};
        // A battlefield the controller may play a unit to (as Wild Claw 876):
        // one they control, not one where units can't be played, and none
        // while they may only play units to base.
        auto unitPlayableAt = [&](const BattlefieldState& bf) {
            return bf.controller.has_value() && *bf.controller == ctx.controller &&
                   !bf.blocks_unit_play && !ps.units_play_base_only;
        };

        // Re-entered with the location: take the chosen unit back off the end.
        GameObjectId chosen = kInvalidId;
        std::optional<LocationId> where_to;
        const bool placing = ri.resume_point == kAwaitWhere;
        if (placing && !ri.resume_data.empty()) {
            chosen = GameObjectId::fromRaw(ri.resume_data.back());
            ri.resume_data.pop_back();
            const int v = takenValue(ctx, 0);
            if (v >= 1 && static_cast<size_t>(v) <= bfs.size() && unitPlayableAt(bfs[v - 1]))
                where_to = LocationId{BattlefieldLocation{bfs[v - 1].id}};
        }

        // First entry: look at up to 5 from the top.
        if (ri.resume_point != kAwait && !placing) {
            ri.resume_data.clear();
            int n = std::min(5, static_cast<int>(ps.main_deck.size()));
            for (int i = 0; i < n; ++i) {
                GameObjectId cid = ps.main_deck[ps.main_deck.size() - 1 - i];
                if (!ctx.state.objectExists(cid)) continue;
                auto& obj = ctx.state.getObject(cid);
                ctx.events.emit(CardRevealedEvent{
                    cid, obj.card_def_id, obj.owner,
                    /*revealed_to_all=*/false, ctx.controller, ZoneType::MainDeck});
                ri.resume_data.push_back(static_cast<int32_t>(cid.raw()));
            }
        }

        // The units among them.
        std::vector<GameObjectId> units;
        for (auto v : ri.resume_data) {
            const auto cid = GameObjectId::fromRaw(v);
            if (ctx.state.objectExists(cid) && ctx.state.getObject(cid).isUnit())
                units.push_back(cid);
        }

        if (placing) {
            if (std::find(units.begin(), units.end(), chosen) == units.end())
                chosen = kInvalidId;
        } else if (ri.resume_point == kAwait) {
            chosen = takenObject(ctx);
            if (std::find(units.begin(), units.end(), chosen) == units.end())
                chosen = kInvalidId;   // "Banish none"
            // CR 355.2.a: no location is printed, so the unit goes to its
            // controller's base or to a battlefield they control, their
            // choice. Asked before anything leaves the deck, and only for a
            // unit that can be paid for (unpaid, it is not played).
            if (chosen != kInvalidId &&
                ctx.executor.canPlayPayingCost(ctx.controller, chosen, reduced)) {
                std::vector<Intent> where;
                for (size_t i = 0; i < bfs.size(); ++i)
                    if (unitPlayableAt(bfs[i]))
                        where.push_back(valChoice(ctx.controller, static_cast<int>(i) + 1,
                                                  "Reinforce: play to battlefield " + std::to_string(i)));
                if (!where.empty()) {
                    where.insert(where.begin(), valChoice(ctx.controller, 0, "Reinforce: play to base"));
                    ri.resume_data.push_back(static_cast<int32_t>(chosen.raw()));
                    ctx.executor.requestChoice(ctx.controller, std::move(where),
                                               "Reinforce: where to play it", PromptKind::Location);
                    ri.resume_point = kAwaitWhere;
                    return;
                }
            }
        } else if (!units.empty()) {
            // "You may": banishing none is always an option.
            std::vector<Intent> options;
            options.push_back(valChoice(ctx.controller, 0, "Banish none"));
            for (auto u : units) options.push_back(objChoice(ctx.controller, u));
            ctx.executor.requestChoice(ctx.controller, std::move(options),
                                       "Reinforce: banish a unit to play (cost reduced by 5)", PromptKind::ChooseObject);
            ri.resume_point = kAwait;
            return;
        }
        ri.resume_point = kDone;

        // Recycle the remaining cards (everything except the chosen one),
        // taking all of them out of the deck first.
        std::vector<GameObjectId> rest;
        for (auto v : ri.resume_data) {
            const auto cid = GameObjectId::fromRaw(v);
            if (!ctx.state.objectExists(cid)) continue;
            deck::take(ctx.state, cid, deck::Place::Held, knowledge::only(ctx.controller), /*slot_public=*/false);
            if (cid != chosen) rest.push_back(cid);
        }
        if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);

        if (chosen != kInvalidId) {
            // Banished, then played for its cost less [5]. Paying is optional
            // (CR 445.2): unpaid, it stays banished.
            ctx.executor.banishObject(chosen);
            const bool played =
                ctx.executor.playPayingCost(ctx.controller, chosen, where_to, reduced);
            RIFT_TRACE(ctx.events, "REINFORCE: banished " + ctx.state.getObject(chosen).name +
                                (played ? " and played it (cost reduced)" : "; not played"));
        } else {
            RIFT_TRACE(ctx.events, "REINFORCE: no unit chosen; recycled top 5");
        }
    }
private:
    enum : int { kAwait = 21, kDone = 22, kAwaitWhere = 23 };
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 62;
        d.def_id = R"RB(ogn-062-298)RB";
        d.name = R"RB(Reinforce)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-062/298)RB";
        d.collector_number = 62;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 5;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Look at the top 5 cards of your Main Deck. You may banish a unit from among them, then play it, reducing its cost by [5]. Recycle the remaining cards.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5bb4ce960c7636dac103c560a2e2f4e32f9d5390-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_62(CardRegistry& r) {
    r.registerCard(62, std::make_unique<Reinforce>());
}

} // namespace riftbound
