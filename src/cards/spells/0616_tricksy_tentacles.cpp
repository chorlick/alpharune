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

class TricksyTentacles : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Move any number of enemy units with the same controller and a total
    //  Might of 8 or less to a single location."
    // The units are targets chosen as a group (CR 355.11.a, 355.13: zero
    // allowed) and the location is the move's destination (CR 355.4): both
    // are chosen as the spell is played, in its finalize pass, before its
    // cost and before anyone can respond. One unit per prompt (each with
    // "Done") against the Might budget, then the location. 1v1: every enemy
    // unit has the same controller. As it resolves, the chosen units that are
    // still legal targets move; if they no longer have total Might 8 or less
    // the controller chooses a legal subset of them (CR 355.11.b).
    // Progress: resume_point kPicking / kAwaitUnit / kAskDest / kAwaitDest /
    // kChosen / kDone; resume_data[kDestSlot] = the location (moveDests code, -1 none),
    // [kCountSlot] = units picked, then the units; the subset helper's slots
    // follow them.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;   // enemy units only
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        const bool ours = ri.resume_point == kPicking || ri.resume_point == kAwaitUnit ||
                          ri.resume_point == kAskDest || ri.resume_point == kAwaitDest ||
                          ri.resume_point == kChosen;
        if (!ours) {
            ri.resume_data.assign(kCountSlot + 1, 0);
            ri.resume_data[kDestSlot] = -1;
            ri.resume_point = kPicking;
        }
        auto picked = [&]() {
            std::vector<GameObjectId> out;
            const auto n = static_cast<size_t>(ri.resume_data[kCountSlot]);
            for (size_t i = 0; i < n; ++i)
                out.push_back(GameObjectId::fromRaw(ri.resume_data[kCountSlot + 1 + i]));
            return out;
        };
        auto enemies = [&]() {
            return chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.controller != ctx.controller;
            });
        };
        auto mightOf = [&](const std::vector<GameObjectId>& units) {
            int total = 0;
            for (auto id : units) total += std::max(0, ctx.state.getObject(id).current_might);
            return total;
        };
        auto candidates = [&]() {
            const auto have = picked();
            const int budget = kTotalMight - mightOf(have);
            std::vector<GameObjectId> out;
            for (auto id : enemies())
                if (std::find(have.begin(), have.end(), id) == have.end() &&
                    ctx.state.getObject(id).current_might <= budget)
                    out.push_back(id);
            return offerableChoices(ctx, out);   // Deflect (CR 809)
        };
        // Locations at least one chosen unit can move to (CR 355.4.a).
        auto destinations = [&]() {
            std::vector<int> out;
            for (auto id : picked())
                for (int d : moveDests(ctx.state, ctx.state.getObject(id)))
                    if (std::find(out.begin(), out.end(), d) == out.end()) out.push_back(d);
            std::sort(out.begin(), out.end());
            return out;
        };
        if (ri.resume_point == kAwaitUnit) {
            const GameObjectId unit = takenObject(ctx);
            const auto legal = candidates();
            if (std::find(legal.begin(), legal.end(), unit) != legal.end()) {
                commitChosenObject(ctx, unit);   // Deflect, "chosen" bookkeeping
                ri.resume_data.push_back(static_cast<int32_t>(unit.raw()));
                ++ri.resume_data[kCountSlot];
                ri.resume_point = kPicking;
            } else {
                ri.resume_point = kAskDest;   // "Done": on to the location
            }
        }
        if (ri.resume_point == kPicking) {
            const auto c = candidates();
            if (!c.empty()) {
                std::vector<Intent> opts;
                for (auto id : c) opts.push_back(objChoice(ctx.controller, id));
                opts.push_back(valChoice(ctx.controller, 0, "Done"));
                ctx.executor.requestChoice(ctx.controller, std::move(opts),
                    "Tricksy Tentacles: move an enemy unit (Might left: " +
                    std::to_string(kTotalMight - mightOf(picked())) + ")", PromptKind::ChooseObject);
                ri.resume_point = kAwaitUnit;
                return;
            }
            ri.resume_point = kAskDest;
        }
        if (ri.resume_point == kAskDest) {
            const auto dests = destinations();
            if (dests.empty()) {
                ri.resume_point = kChosen;   // nothing chosen, or nowhere to go
            } else {
                std::vector<Intent> opts;
                for (int d : dests) opts.push_back(valChoice(ctx.controller, d, destLabel(d)));
                ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                           "Tricksy Tentacles: move them where?", PromptKind::Location);
                ri.resume_point = kAwaitDest;
                return;
            }
        }
        if (ri.resume_point == kAwaitDest) {
            const auto dests = destinations();
            int d = takenValue(ctx, dests.empty() ? -1 : dests.front());
            if (std::find(dests.begin(), dests.end(), d) == dests.end())
                d = dests.empty() ? -1 : dests.front();
            ri.resume_data[kDestSlot] = d;
            ri.resume_point = kChosen;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)

        const int d = ri.resume_data[kDestSlot];
        const auto chosen = picked();
        if (d < 0 || chosen.empty()) { ri.resume_point = kDone; return; }
        // The chosen units that are still legal targets (CR 359.3.e.4-5).
        std::vector<GameObjectId> originals;
        const auto now = enemies();
        for (auto id : chosen)
            if (std::find(now.begin(), now.end(), id) != now.end() &&
                ctx.state.stillTheChosenObject(ri, id))
                originals.push_back(id);
        const auto group = chooseFittingSubset(ctx,
            "Tricksy Tentacles: the chosen units no longer have total Might 8 or less -"
            " choose which of them to move",
            originals, [&](const std::vector<GameObjectId>& set) {
                return mightOf(set) <= kTotalMight;
            }, kCountSlot + 1 + chosen.size());
        if (!group.has_value()) return;   // waiting for the agent
        ri.resume_point = kDone;
        for (auto id : *group) {
            const auto& u = ctx.state.getObject(id);
            if (!u.location.has_value()) continue;
            // Already there, or unable to go there: it does not move.
            const auto dests = moveDests(ctx.state, u);
            if (std::find(dests.begin(), dests.end(), d) == dests.end()) continue;
            moveTo(ctx, id, d);
        }
        RIFT_TRACE(ctx.events, "TRICKSY TENTACLES: moved the chosen enemy units to " + destLabel(d));
    }
private:
    static constexpr int kTotalMight = 8;
    enum : int { kPicking = 40, kAwaitUnit = 41, kAskDest = 42, kChosen = 43, kDone = 44,
                 kAwaitDest = 45 };
    static constexpr size_t kDestSlot = 7;
    static constexpr size_t kCountSlot = 8;
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 616;
        d.def_id = R"RB(unl-054-219)RB";
        d.name = R"RB(Tricksy Tentacles)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-054/219)RB";
        d.collector_number = 54;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Move any number of enemy units with the same controller and a total Might of 8 or less to a single location.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7e1e77df44c076599fa780f7fd01a4550f67af45-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_616(CardRegistry& r) {
    r.registerCard(616, std::make_unique<TricksyTentacles>());
}

} // namespace riftbound
