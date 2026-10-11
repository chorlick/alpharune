#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class FoxFire : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Kill any number of units at a battlefield with total Might 4 or less."
    // The units are chosen as the spell is played (CR 355.11.a), one prompt at
    // a time: the first names the battlefield, each later one adds a unit
    // there that still fits the total, or stops. Progress lives on the item:
    // resume_point kAwait while a prompt is out, resume_data = the units
    // picked so far.
    // "Any number" includes zero (CR 355.13): the spell is playable with no
    // unit to choose (no hasLegalTargets gate), and every prompt, the first
    // included, offers "Stop here".
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // A unit at a battlefield with Might 4 or less.
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object,
            [&](const GameObject& u) {
                return u.isAtBattlefield() && u.current_might <= kTotalMight;
            });
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        if (ri.resume_point != kChosen && ri.resume_point != kRechoose &&
            ri.resume_point != kRechooseAwait) {
            auto picked_before = [&ri](GameObjectId id) {
                return std::find(ri.resume_data.begin(), ri.resume_data.end(),
                                 static_cast<int32_t>(id)) != ri.resume_data.end();
            };
            auto candidates = [&]() {
                int budget = kTotalMight;
                // Played from facedown: units at that battlefield (CR 811.1.d.2).
                std::optional<BattlefieldId> bf = facedownPlayBattlefield(ctx.state, ctx.source);
                for (auto id : ri.resume_data) {
                    const auto& u = ctx.state.getObject(static_cast<GameObjectId>(id));
                    budget -= std::max(0, u.current_might);
                    bf = u.battlefieldId();
                }
                auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                    return u.isAtBattlefield() && (!bf.has_value() || u.battlefieldId() == bf) &&
                           u.current_might <= budget && !picked_before(u.id);
                });
                std::sort(units.begin(), units.end());
                return offerableChoices(ctx, units);
            };
            bool stop = false;
            if (ri.resume_point == kAwait) {
                const auto can_pick = candidates();
                const GameObjectId unit = takenObject(ctx);
                if (std::find(can_pick.begin(), can_pick.end(), unit) == can_pick.end()) {
                    stop = true;   // "stop here", or the unit is no longer available
                } else {
                    commitChosenObject(ctx, unit);
                    ri.resume_data.push_back(static_cast<int32_t>(unit));
                }
            } else if (ri.resume_point != kPicking) {
                ri.resume_data.clear();   // first entry
            }
            ri.resume_point = kPicking;
            if (!stop) {
                const auto can_pick = candidates();
                if (!can_pick.empty()) {
                    std::vector<Intent> options;
                    // "any number": the controller may stop at any prompt, the
                    // first too — no unit at all is a choice (CR 355.13).
                    options.push_back(valChoice(ctx.controller, 0, "Stop here"));
                    for (auto id : can_pick) options.push_back(objChoice(ctx.controller, id));
                    ctx.executor.requestChoice(ctx.controller, std::move(options),
                        "Fox-Fire: kill a unit at a battlefield (total Might 4 or less)");
                    ri.resume_point = kAwait;
                    return;
                }
            }
            ri.resume_point = kChosen;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)

        auto kill = [&](const std::vector<GameObjectId>& units) {
            ri.resume_point = kDone;
            {   // the batch is for these kills only
                DeathBatch together(ctx.executor, units);   // they die at the same time
                for (auto id : units) ctx.executor.killObject(id);
            }
            RIFT_TRACE(ctx.events, "FOX-FIRE: killed " + std::to_string(units.size()) +
                                " chosen units (total Might <= 4) at one battlefield");
        };
        if (ri.resume_point == kChosen) {
            // The chosen units that are still legal targets: units at a
            // battlefield. "At a battlefield with total Might 4 or less" is a
            // requirement on them as a group (CR 355.11), judged now.
            const auto legal = chooseableUnits(ctx.state, ctx.controller, [&ri](const GameObject& u) {
                return u.isAtBattlefield() &&
                       std::find(ri.resume_data.begin(), ri.resume_data.end(),
                                 static_cast<int32_t>(u.id)) != ri.resume_data.end();
            });
            if (fitsTogether(ctx.state, legal)) { kill(legal); return; }
            // They no longer fit (a unit gained Might, or they are at different
            // battlefields): the controller chooses a legal subset of the
            // ORIGINAL targets to affect (CR 355.11.b). resume_data becomes
            // [n, the n originals still legal, the subset picked so far].
            ri.resume_data.assign(1, static_cast<int32_t>(legal.size()));
            for (auto id : legal) ri.resume_data.push_back(static_cast<int32_t>(id));
            ri.resume_point = kRechoose;
        }
        // One original per prompt, from those at the subset's battlefield that
        // still fit the total. There is no "stop": a legal subset holds as
        // many targets as it can (CR 355.11.b.1), so the picking ends when no
        // original fits any more, and a lone option is taken without asking.
        if (ri.resume_data.empty()) { ri.resume_point = kDone; return; }
        const size_t n = static_cast<size_t>(ri.resume_data[0]);
        auto subset = [&]() {
            return std::vector<GameObjectId>(
                ri.resume_data.begin() + static_cast<std::ptrdiff_t>(1 + n), ri.resume_data.end());
        };
        auto can_add = [&]() {
            const auto sub = subset();
            int budget = kTotalMight;
            std::optional<BattlefieldId> bf;
            for (auto id : sub) {
                const auto& u = ctx.state.getObject(id);
                budget -= std::max(0, u.current_might);
                bf = u.battlefieldId();
            }
            std::vector<GameObjectId> out;
            for (size_t i = 1; i <= n; ++i) {
                const auto id = static_cast<GameObjectId>(ri.resume_data[i]);
                if (std::find(sub.begin(), sub.end(), id) != sub.end()) continue;
                const auto& u = ctx.state.getObject(id);
                if ((!bf.has_value() || u.battlefieldId() == bf) && u.current_might <= budget)
                    out.push_back(id);
            }
            return out;
        };
        if (ri.resume_point == kRechooseAwait) {
            const auto options = can_add();
            GameObjectId unit = takenObject(ctx);
            if (std::find(options.begin(), options.end(), unit) == options.end())
                unit = options.empty() ? kInvalidId : options.front();
            if (unit != kInvalidId) ri.resume_data.push_back(static_cast<int32_t>(unit));
            ri.resume_point = kRechoose;
        }
        for (;;) {
            const auto options = can_add();
            if (options.empty()) break;
            if (options.size() == 1) {
                ri.resume_data.push_back(static_cast<int32_t>(options.front()));
                continue;
            }
            std::vector<Intent> choices;
            for (auto id : options) choices.push_back(objChoice(ctx.controller, id));
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                "Fox-Fire: the chosen units no longer have total Might 4 or less at one"
                " battlefield - choose which of them to kill");
            ri.resume_point = kRechooseAwait;
            return;
        }
        kill(subset());
    }
private:
    static constexpr int kTotalMight = 4;
    // 0-Might units fit any total: "any number" is every unit on offer.
    enum : int { kPicking = 20, kAwait = 21, kDone = 22, kChosen = 23,
                 kRechoose = 24, kRechooseAwait = 25 };
    /// Whether `units` meet the requirement as a group: all at one battlefield,
    /// total Might 4 or less.
    static bool fitsTogether(const GameState& state, const std::vector<GameObjectId>& units) {
        int total = 0;
        std::optional<BattlefieldId> bf;
        for (auto id : units) {
            const auto& u = state.getObject(id);
            if (bf.has_value() && u.battlefieldId() != bf) return false;
            bf = u.battlefieldId();
            total += std::max(0, u.current_might);
        }
        return total <= kTotalMight;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 253;
        d.def_id = R"RB(ogn-256-298)RB";
        d.name = R"RB(Fox-Fire)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-256/298)RB";
        d.collector_number = 256;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Ahri)RB"};
        d.energy_cost = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Kill any number of units at a battlefield with total Might 4 or less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7ad9d6a46a1c54080d54950a0044da3a82e32b45-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_253(CardRegistry& r) {
    r.registerCard(253, std::make_unique<FoxFire>());
}

} // namespace riftbound
