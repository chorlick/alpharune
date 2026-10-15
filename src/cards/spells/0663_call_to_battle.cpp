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

class CallToBattle : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Move a unit you control to a battlefield you control. Then, choose an
    //  opponent. They move a unit they control to the same battlefield."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }
    bool needsPlayTimeTarget() const override { return true; }

    /// Battlefields `p` controls other than the one `unit` is at, as a
    /// pickMode mask over the battlefield slots.
    static uint32_t destinationMask(const GameState& state, PlayerId p, GameObjectId unit) {
        uint32_t mask = 0;
        if (!state.objectExists(unit)) return mask;
        const auto here = state.getObject(unit).battlefieldId();
        for (size_t i = 0; i < state.battlefields.size() && i < 32; ++i) {
            const auto& bf = state.battlefields[i];
            if (!bf.controller.has_value() || *bf.controller != p) continue;
            if (here.has_value() && *here == bf.id) continue;
            mask |= (1u << i);
        }
        return mask;
    }
    // A unit you control that has a battlefield you control to move to.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != controller || !obj.location.has_value()) continue;
            if (destinationMask(state, controller, id) != 0) out.push_back(id);
        }
        return out;
    }

    // resume_data (clear of the Card::pick* helpers' slots):
    static constexpr size_t kPhaseSlot = 7;   // 0 = nothing moved yet, 1 = opponent asked, 2 = done
    static constexpr size_t kBfSlot = 8;      // the battlefield, once the first unit has moved

    static std::vector<GameObjectId> unitsNotAt(const GameState& state, PlayerId p, BattlefieldId bf) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != p || !obj.location.has_value()) continue;
            const auto at = obj.battlefieldId();
            if (at.has_value() && *at == bf) continue;
            out.push_back(id);
        }
        return out;
    }

    // The unit (its play-time target) and the battlefield it moves to (CR
    // 355.4) are chosen as the spell is played, in its finalize pass, before
    // its cost. "Choose an opponent": 1v1, one choice. "They move a unit they
    // control to the same battlefield" is their choice as it resolves (CR
    // 355.10.e).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object, [&](const GameObject& u) {
            return destinationMask(state, controller, u.id) != 0;
        });
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ri = ctx.state.chain.resuming;
        auto slot = [&](size_t i) -> int32_t& {
            while (ri->resume_data.size() <= i) ri->resume_data.push_back(0);
            return ri->resume_data[i];
        };
        const PlayerId opp = opponent(ctx.controller);
        if (ri.has_value() && slot(kPhaseSlot) >= 2) return;
        if (ri.has_value() && slot(kPhaseSlot) == 1) {
            // "They move a unit they control to the same battlefield": the
            // opponent's pick.
            slot(kPhaseSlot) = 2;
            const auto bf = BattlefieldId::fromRaw(slot(kBfSlot));
            const auto theirs = unitsNotAt(ctx.state, opp, bf);
            GameObjectId mover = takenObject(ctx);
            if (theirs.empty()) return;
            if (std::find(theirs.begin(), theirs.end(), mover) == theirs.end())
                mover = theirs.front();
            ctx.executor.moveToBattlefield(mover, bf);
            RIFT_TRACE(ctx.events, "CALL TO BATTLE: opponent moved a unit to the same BF");
            return;
        }

        const auto locked = lockedPlayTarget(ctx, targets);
        const GameObjectId unit = locked ? *locked
            : pickTarget(ctx, "Call to Battle: move a unit you control",
                         enumerateLegalTargets(ctx.state, ctx.controller));
        if (unit == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting for the agent

        // Which battlefield you control it moves to.
        const auto& bfs = ctx.state.battlefields;
        const int n = static_cast<int>(std::min<size_t>(bfs.size(), 32));
        int mode = -2;
        if (unit != kInvalidId) {
            std::vector<std::string> labels;
            for (int i = 0; i < n; ++i)
                labels.push_back(ctx.state.objectExists(bfs[i].card_object_id)
                    ? ctx.state.getObject(bfs[i].card_object_id).name
                    : "Battlefield #" + std::to_string(static_cast<int>(bfs[i].id.raw())));
            mode = pickMode(ctx, "Call to Battle: battlefield", n, labels,
                            destinationMask(ctx.state, ctx.controller, unit));
            if (mode == -1) return;   // waiting for the agent
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (mode < 0 || mode >= n) return;    // no unit / no battlefield to move to
        const BattlefieldId dest = bfs[mode].id;
        // My unit moves if it is still a legal target and the battlefield is
        // still one I control that it is not at (CR 355.4.a); "the same
        // battlefield" for the opponent is the one chosen either way.
        if (unit != kInvalidId &&
            (destinationMask(ctx.state, ctx.controller, unit) & (1u << mode)) != 0) {
            ctx.executor.moveToBattlefield(unit, dest);
            RIFT_TRACE(ctx.events, "CALL TO BATTLE: moved a friendly unit to a controlled BF");
        }

        // "Then, choose an opponent. They move a unit they control to the same
        //  battlefield." — their choice among their units that are not there.
        if (!ri.has_value()) return;
        slot(kPhaseSlot) = 2;
        const auto theirs = unitsNotAt(ctx.state, opp, dest);
        if (theirs.empty()) return;
        std::vector<Intent> opts;
        for (auto id : theirs) opts.push_back(objChoice(opp, id));
        slot(kPhaseSlot) = 1;
        slot(kBfSlot) = static_cast<int32_t>(dest.raw());
        // CR 355.10.e: chosen by the other player, as the spell resolves.
        ctx.executor.requestChoice(opp, std::move(opts),
                                   "Call to Battle: move a unit you control to that battlefield", PromptKind::ChooseObject);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 663;
        d.def_id = R"RB(unl-101-219)RB";
        d.name = R"RB(Call to Battle)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-101/219)RB";
        d.collector_number = 101;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Move a unit you control to a battlefield you control. Then, choose an opponent. They move a unit they control to the same battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3c7e0f93c72654f786f5df37895c6e95fa9d4d60-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_663(CardRegistry& r) {
    r.registerCard(663, std::make_unique<CallToBattle>());
}

} // namespace riftbound
