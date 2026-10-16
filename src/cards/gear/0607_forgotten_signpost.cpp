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

class ForgottenSignpost : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Action][>] Exhaust a unit you control, [E]: Move a different unit you
    //  control to the location of the unit you exhausted to pay for this
    //  ability."
    // Two units are chosen as the ability is activated (CR 355): the one to
    // exhaust — a cost, paid then — and a different one to move. They go
    // through pickTargetPair in the finalize pass; two pickTarget calls in one
    // callback would return the same unit twice.
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true},
            .targets = {},
            .is_action = true, .is_reaction = false,
        }};
    }
    // Offered only when the cost can be paid for an effect: a ready unit to
    // exhaust and a different unit of yours that is somewhere else.
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !exhaustable(state, controller, /*must_be_ready=*/true).empty();
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& /*targets*/) override {
        // Once the cost has been paid the exhausted unit is no longer ready:
        // it stays the legal first choice it was.
        const bool paid = costAlreadyPaid(ctx);
        // The unit exhausted is the ability's COST, not a target (costs never
        // target, CR 355.10.c; testing phase U14's class): no Deflect, not
        // "chosen". Offered only with a different unit of yours that could
        // move to it. The unit moved is the effect's target.
        std::vector<GameObjectId> payers;
        for (auto u : exhaustable(ctx.state, ctx.controller, /*must_be_ready=*/!paid))
            if (!moversTo(ctx.state, ctx.controller, u).empty()) payers.push_back(u);
        const GameObjectId to_exhaust = pickCostObject(ctx,
            "Forgotten Signpost: exhaust a unit you control (the cost)", payers);
        if (to_exhaust == kInvalidId) return;   // waiting, or nothing to pay with (stored)
        const auto moving = selectTargetOrNone(*this, ctx,
            "Forgotten Signpost: move a different unit to the exhausted unit's location",
            moversTo(ctx.state, ctx.controller, to_exhaust), 7, /*allow_none=*/false);
        if (!moving.has_value()) return;   // waiting for the agent
        const GameObjectId mover = *moving;
        if (to_exhaust != kInvalidId && mover != kInvalidId && costDueNow(ctx)) {
            // "Exhaust a unit you control" — only a ready unit can pay it.
            if (ctx.state.getObject(to_exhaust).is_exhausted) return;
            ctx.executor.exhaustObject(to_exhaust);
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 355)
        if (to_exhaust == kInvalidId || mover == kInvalidId) return;
        const auto& anchor = ctx.state.getObject(to_exhaust);
        const auto& moved = ctx.state.getObject(mover);
        if (!anchor.location.has_value() || !moved.location.has_value()) return;
        if (moved.controller != ctx.controller) return;
        moveToLocation(ctx.executor, mover, anchor.location);
        RIFT_TRACE(ctx.events, "FORGOTTEN SIGNPOST: moved a friendly unit to the exhausted unit's location");
    }
private:
    /// Units `controller` has on board, other than `anchor`, that are not
    /// already where `anchor` is.
    static std::vector<GameObjectId> moversTo(const GameState& state, PlayerId controller,
                                              GameObjectId anchor) {
        std::vector<GameObjectId> out;
        if (!state.objectExists(anchor) || !state.getObject(anchor).location.has_value())
            return out;
        const LocationId dest = *state.getObject(anchor).location;
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (id == anchor || !obj.isUnit() || obj.controller != controller) continue;
            if (!obj.location.has_value() || *obj.location == dest) continue;
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    /// Units `controller` could exhaust for the cost: on board, with a
    /// different unit that could be moved to them.
    static std::vector<GameObjectId> exhaustable(const GameState& state, PlayerId controller,
                                                 bool must_be_ready) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != controller || !obj.location.has_value()) continue;
            if (must_be_ready && obj.is_exhausted) continue;
            if (!moversTo(state, controller, id).empty()) out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    const CardDef def_ = [] {
        CardDef d;
        d.id = 607;
        d.def_id = R"RB(unl-045-219)RB";
        d.name = R"RB(Forgotten Signpost)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-045/219)RB";
        d.collector_number = 45;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB([Action][>] Exhaust a unit you control, [E]: Move a different unit you control to the location of the unit you exhausted to pay for this ability.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/edf248974d1d97ff5ff1c6745f663fa059dbc82d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_607(CardRegistry& r) {
    r.registerCard(607, std::make_unique<ForgottenSignpost>());
}

} // namespace riftbound
